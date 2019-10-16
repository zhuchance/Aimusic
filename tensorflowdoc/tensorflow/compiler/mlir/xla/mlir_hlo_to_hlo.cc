/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/mlir/xla/mlir_hlo_to_hlo.h"

#include <memory>

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/Dialect/StandardOps/Ops.h"  // TF:local_config_mlir
#include "mlir/IR/Attributes.h"  // TF:local_config_mlir
#include "mlir/IR/Function.h"  // TF:local_config_mlir
#include "mlir/IR/Location.h"  // TF:local_config_mlir
#include "mlir/IR/MLIRContext.h"  // TF:local_config_mlir
#include "mlir/IR/Matchers.h"  // TF:local_config_mlir
#include "mlir/IR/Module.h"  // TF:local_config_mlir
#include "mlir/IR/Operation.h"  // TF:local_config_mlir
#include "tensorflow/compiler/mlir/xla/ir/hlo_ops.h"
#include "tensorflow/compiler/mlir/xla/type_to_shape.h"
#include "tensorflow/compiler/xla/client/xla_builder.h"
#include "tensorflow/compiler/xla/comparison_util.h"
#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"

using ::tensorflow::int16;
using ::tensorflow::int32;
using ::tensorflow::int64;
using ::tensorflow::int8;
using ::tensorflow::uint16;
using ::tensorflow::uint32;
using ::tensorflow::uint64;
using ::tensorflow::uint8;

static std::vector<int64> ConvertDenseIntAttr(mlir::DenseIntElementsAttr attr) {
  auto values = attr.getValues<int64>();
  return {values.begin(), values.end()};
}

// Converts the broadcast_dimensions attribute into a vector of dimension
// numbers (empty if the attribute is absent).
static std::vector<int64> Convert_broadcast_dimensions(
    llvm::Optional<mlir::DenseIntElementsAttr> broadcast_dimensions) {
  if (!broadcast_dimensions.hasValue()) return {};

  return ConvertDenseIntAttr(*broadcast_dimensions);
}

// Converts the broadcast_sizes attribute into a vector of dimension sizes.
static std::vector<int64> Convert_broadcast_sizes(
    mlir::DenseIntElementsAttr broadcast_sizes) {
  return ConvertDenseIntAttr(broadcast_sizes);
}

static std::vector<int64> Convert_permutation(
    mlir::DenseIntElementsAttr permutation) {
  return ConvertDenseIntAttr(permutation);
}

static std::vector<int64> Convert_ArrayRef(llvm::ArrayRef<int64_t> values) {
  return {values.begin(), values.end()};
}

// Converts the precision config array of strings attribute into the
// corresponding XLA proto. All the strings are assumed to be valid names of the
// Precision enum. This should have been checked in the op verify method.
static std::unique_ptr<xla::PrecisionConfig> Convert_precision_config(
    llvm::Optional<mlir::ArrayAttr> optional_precision_config_attr) {
  if (!optional_precision_config_attr.hasValue()) return nullptr;

  auto precision_config = absl::make_unique<xla::PrecisionConfig>();
  for (auto attr : optional_precision_config_attr.getValue()) {
    xla::PrecisionConfig::Precision p;
    auto operand_precision = attr.cast<mlir::StringAttr>().getValue().str();
    // TODO(jpienaar): Update this to ensure this is captured by verify.
    if (xla::PrecisionConfig::Precision_Parse(operand_precision, &p)) {
      precision_config->add_operand_precision(p);
    } else {
      auto* context = attr.getContext();
      mlir::emitError(mlir::UnknownLoc::get(context))
          << "unexpected operand precision " << operand_precision;
      return nullptr;
    }
  }

  return precision_config;
}

static xla::DotDimensionNumbers Convert_dot_dimension_numbers(
    mlir::xla_hlo::DotDimensionNumbers dot_dimension_numbers_attr) {
  xla::DotDimensionNumbers dot_dimension_numbers;

  auto rhs_contracting_dimensions =
      dot_dimension_numbers_attr.rhs_contracting_dimensions()
          .cast<mlir::DenseIntElementsAttr>();
  auto lhs_contracting_dimensions =
      dot_dimension_numbers_attr.lhs_contracting_dimensions()
          .cast<mlir::DenseIntElementsAttr>();
  auto rhs_batch_dimensions =
      dot_dimension_numbers_attr.rhs_batching_dimensions()
          .cast<mlir::DenseIntElementsAttr>();
  auto lhs_batch_dimensions =
      dot_dimension_numbers_attr.lhs_batching_dimensions()
          .cast<mlir::DenseIntElementsAttr>();

  for (auto val : rhs_contracting_dimensions) {
    dot_dimension_numbers.add_rhs_contracting_dimensions(val.getSExtValue());
  }
  for (auto val : lhs_contracting_dimensions) {
    dot_dimension_numbers.add_lhs_contracting_dimensions(val.getSExtValue());
  }

  for (auto val : rhs_batch_dimensions) {
    dot_dimension_numbers.add_rhs_batch_dimensions(val.getSExtValue());
  }

  for (auto val : lhs_batch_dimensions) {
    dot_dimension_numbers.add_lhs_batch_dimensions(val.getSExtValue());
  }

  return dot_dimension_numbers;
}

// Converts the comparison_direction string attribute into the XLA enum. The
// string is assumed to correspond to exactly one of the allowed strings
// representing the enum. This should have been checked in the op verify method.
static xla::ComparisonDirection Convert_comparison_direction(
    llvm::StringRef comparison_direction_string) {
  return xla::StringToComparisonDirection(comparison_direction_string.str())
      .ValueOrDie();
}

// Passes through everything except for unique_ptr, on which it calls get().
// This exists to allow the generated code to call XLA functions that take a raw
// pointer. In particular, PrecisionConfig is passed to xla::Dot and xla::Conv
// as a pointer and there is otherwise no way to avoid a memory leak.
template <typename T>
T Unwrap(T t) {
  return t;
}

template <typename T>
T* Unwrap(const std::unique_ptr<T>& t) {
  return t.get();
}

// Convert APInt into an int.
// TODO(hpucha): This should be consolidated into a general place.
static int ConvertAPInt(llvm::APInt i) { return i.getSExtValue(); }

// Convert APFloat to double.
static double ConvertAPFloat(llvm::APFloat value) {
  const auto& semantics = value.getSemantics();
  bool losesInfo = false;
  if (&semantics != &llvm::APFloat::IEEEdouble())
    value.convert(llvm::APFloat::IEEEdouble(),
                  llvm::APFloat::rmNearestTiesToEven, &losesInfo);
  return value.convertToDouble();
}

namespace mlir {
namespace {
class ConvertToHloModule {
 public:
  using ValueLoweringMap = llvm::DenseMap<Value*, xla::XlaOp>;
  using FunctionLoweringMap = llvm::DenseMap<mlir::FuncOp, xla::XlaComputation>;

  explicit ConvertToHloModule(mlir::ModuleOp module, bool use_tuple_args,
                              bool always_return_tuple)
      : module_(module),
        module_builder_("main"),
        use_tuple_args_(use_tuple_args),
        always_return_tuple_(always_return_tuple) {}

  // Perform the lowering to XLA. This function returns failure if an error was
  // encountered.
  LogicalResult Run() {
    for (auto func : module_.getOps<FuncOp>()) {
      if (func.empty()) continue;
      if (failed(RunOnFunction(func))) return failure();
    }
    return success();
  }

  // Lower a specific function to HLO.
  LogicalResult RunOnFunction(mlir::FuncOp f);

  // Lower a `mlir::Region` to a `XlaComputation`
  LogicalResult LowerRegionAsComputation(mlir::Region* region,
                                         xla::XlaComputation* func);

  // Lower a single `Block` to a `XlaComputation`
  LogicalResult LowerBasicBlockAsFunction(Block* block,
                                          xla::XlaBuilder* builder,
                                          xla::XlaComputation* result);

  ::xla::HloModuleProto ConsumeMainProto() {
    return lowered_computation_[module_.lookupSymbol<mlir::FuncOp>("main")]
        .proto();
  }

 private:
  LogicalResult Lower(mlir::Operation* inst, xla::XlaBuilder* builder,
                      ConvertToHloModule::ValueLoweringMap* value_lowering,
                      xla::XlaComputation* result);

  // The module being lowered.
  mlir::ModuleOp module_;

  // The top-level XlaBuilder.
  xla::XlaBuilder module_builder_;

  // Map between function and lowered computation.
  FunctionLoweringMap lowered_computation_;

  // Whether the entry function should take a single tuple as input.
  bool use_tuple_args_;

  // Whether to always return a tuple.
  bool always_return_tuple_;

  // Unique suffix to give to the name of the next lowered region.
  size_t region_id_ = 0;
};

}  // namespace
}  // namespace mlir

namespace {

struct OpLoweringContext {
  llvm::DenseMap<mlir::Value*, xla::XlaOp>* values;
  mlir::ConvertToHloModule* converter;
  xla::XlaBuilder* builder;
};

llvm::SmallVector<xla::XlaOp, 4> GetTuple(mlir::Operation::operand_range values,
                                          OpLoweringContext ctx) {
  llvm::SmallVector<xla::XlaOp, 4> ops;
  for (mlir::Value* value : values) {
    ops.push_back((*ctx.values)[value]);
  }
  return ops;
}

}  // namespace

namespace mlir {
namespace xla_hlo {
namespace {

LogicalResult ExportXlaOp(BroadcastInDimOp op, OpLoweringContext ctx) {
  auto type = op.getType().dyn_cast<RankedTensorType>();
  if (!type) return failure();
  auto& value_map = *ctx.values;
  value_map[op] =
      BroadcastInDim(value_map[op.operand()], Convert_ArrayRef(type.getShape()),
                     Convert_broadcast_dimensions(op.broadcast_dimensions()));
  return success();
}

LogicalResult ExportXlaOp(ConcatenateOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(ConstOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(ConvOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(ConvertOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(CopyOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(DynamicSliceOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(DynamicUpdateSliceOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(GatherOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(GetTupleElementOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(IotaOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(PadOp op, OpLoweringContext ctx) { return failure(); }

LogicalResult ExportXlaOp(ReduceOp op, OpLoweringContext ctx) {
  auto& value_map = *ctx.values;
  xla::XlaComputation body;
  if (failed(ctx.converter->LowerRegionAsComputation(&op.body(), &body))) {
    return failure();
  }
  xla::XlaOp result =
      xla::Reduce(ctx.builder, GetTuple(op.operands(), ctx),
                  GetTuple(op.init_values(), ctx), body,
                  Convert_broadcast_dimensions(op.dimensions()));
  if (op.getNumResults() == 1) {
    value_map[op.getResult(0)] = result;
  } else {
    for (auto item : llvm::enumerate(op.getResults())) {
      value_map[item.value()] = xla::GetTupleElement(result, item.index());
    }
  }
  return success();
}

LogicalResult ExportXlaOp(ReduceWindowOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(ReshapeOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(ReturnOp op, OpLoweringContext ctx) {
  // Failure on purpose because `xla_hlo::ReturnOp` will be handled by
  // special purpose logic in `ConvertToHloModule::Lower`.
  return failure();
}

LogicalResult ExportXlaOp(ReverseOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(RngUniformOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(SelectAndScatterOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(SliceOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(TupleOp op, OpLoweringContext ctx) {
  return failure();
}

LogicalResult ExportXlaOp(WhileOp op, OpLoweringContext ctx) {
  return failure();
}

}  // namespace
}  // namespace xla_hlo
}  // namespace mlir

#include "tensorflow/compiler/mlir/xla/operator_writers.inc"

namespace mlir {
namespace {

StatusOr<xla::Literal> CreateLiteralFromAttr(Type type, ElementsAttr attr) {
  xla::Shape shape = xla::TypeToShape(type);

#define ELEMENTS_ATTR_TO_LITERAL(xla_type, cpp_type)       \
  case xla_type: {                                         \
    xla::Array<cpp_type> source_data(shape.dimensions());  \
    source_data.SetValues(attr.getValues<cpp_type>());     \
    return xla::LiteralUtil::CreateFromArray(source_data); \
  }

  switch (shape.element_type()) {
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::PRED, bool)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::F32, float)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::F64, double)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::S8, int8)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::S16, int16)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::S32, int32)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::S64, int64)
    // TODO(b/130356985): Update once MLIR supports unsigned integers.
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::U8, uint8)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::U16, uint16)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::U32, uint32)
    ELEMENTS_ATTR_TO_LITERAL(xla::PrimitiveType::U64, uint64)
    default:
      return tensorflow::errors::Internal(absl::StrCat(
          "Unsupported type: ", xla::PrimitiveType_Name(shape.element_type())));
  }
#undef ELEMENTS_ATTR_TO_LITERAL
}

LogicalResult ConvertToHloModule::Lower(
    mlir::Operation* inst, xla::XlaBuilder* builder,
    ConvertToHloModule::ValueLoweringMap* value_lowering,
    xla::XlaComputation* result) {
  if (succeeded(ExportXlaOperator(inst, {value_lowering, this, builder}))) {
    return success();
  }

  auto& value_map = *value_lowering;
  ElementsAttr const_attr;
  // TODO(jpienaar): This doesn't support layouts yet.
  if (matchPattern(inst, m_Constant(&const_attr))) {
    auto literal_or =
        CreateLiteralFromAttr(*inst->result_type_begin(), const_attr);
    if (!literal_or.ok()) return inst->emitError("unsupported elemental type");
    value_map[inst->getResult(0)] =
        xla::ConstantLiteral(builder, literal_or.ValueOrDie());
    return success();
  }

  if (isa<xla_hlo::ReturnOp>(inst) || isa<mlir::ReturnOp>(inst)) {
    // Construct the return value for the function. If there are multiple
    // values returned, then create a tuple, else return value directly.
    xla::XlaOp return_value;
    unsigned num_return_values = inst->getNumOperands();
    if (always_return_tuple_ || num_return_values > 1) {
      std::vector<xla::XlaOp> returns(num_return_values);
      for (unsigned i = 0, e = inst->getNumOperands(); i != e; ++i) {
        returns[i] = value_map[inst->getOperand(i)];
      }
      return_value = xla::Tuple(builder, returns);
    } else if (num_return_values == 1) {
      return_value = value_map[inst->getOperand(0)];
    }

    // Build the XlaComputation and check for failures.
    auto computation_or =
        return_value.valid() ? builder->Build(return_value) : builder->Build();
    if (!computation_or.ok()) {
      inst->emitError(llvm::Twine(computation_or.status().error_message()));
      return failure();
    }
    *result = std::move(computation_or.ValueOrDie());
    return success();
  }

  inst->emitError("unable to lower operation of type '" +
                  inst->getName().getStringRef().str() + '\'');
  return failure();
}

LogicalResult ConvertToHloModule::RunOnFunction(mlir::FuncOp f) {
  if (f.getBlocks().size() != 1) {
    return f.emitError("only single block Function suppored");
  }

  // Create a sub-builder if this is not the main function.
  std::unique_ptr<xla::XlaBuilder> builder_up;
  bool entry_function = f.getName().str() == "main";
  if (!entry_function)
    builder_up = module_builder_.CreateSubBuilder(f.getName().str());
  auto& builder = entry_function ? module_builder_ : *builder_up;

  xla::XlaComputation computation;
  if (failed(LowerBasicBlockAsFunction(&f.front(), &builder, &computation))) {
    return failure();
  }
  lowered_computation_[f] = std::move(computation);
  return success();
}

LogicalResult ConvertToHloModule::LowerBasicBlockAsFunction(
    Block* block, xla::XlaBuilder* builder, xla::XlaComputation* result) {
  auto& bb = *block;
  // Mapping from the Value to lowered XlaOp. The code below lowers in
  // program order and will fail if an operand is unseen. This can be improved.
  ValueLoweringMap lowering;

  // If using tuples as input, then there is only one input parameter that is a
  // tuple.
  if (use_tuple_args_) {
    std::vector<xla::Shape> arg_shapes;
    arg_shapes.reserve(bb.getNumArguments());
    for (auto& arg : bb.getArguments())
      arg_shapes.push_back(xla::TypeToShape(arg->getType()));
    xla::Shape input_shape = xla::ShapeUtil::MakeTupleShape(arg_shapes);
    auto tuple = xla::Parameter(builder, 0, input_shape, "arg_tuple");
    for (auto& it : llvm::enumerate(bb.getArguments())) {
      lowering[it.value()] = xla::GetTupleElement(tuple, it.index());
    }
  } else {
    for (auto& it : llvm::enumerate(bb.getArguments())) {
      auto* arg = it.value();
      auto num = it.index();
      xla::Shape shape = xla::TypeToShape(arg->getType());
      lowering[arg] =
          xla::Parameter(builder, num, shape, absl::StrCat("Arg_", num));
    }
  }

  for (auto& inst : bb)
    if (failed(Lower(&inst, builder, &lowering, result))) return failure();

  return success();
}

LogicalResult ConvertToHloModule::LowerRegionAsComputation(
    mlir::Region* region, xla::XlaComputation* func) {
  std::unique_ptr<xla::XlaBuilder> builder =
      module_builder_.CreateSubBuilder(absl::StrCat("region_", region_id_++));
  return LowerBasicBlockAsFunction(&region->front(), builder.get(), func);
}

}  // namespace

Status ConvertMlirHloToHlo(mlir::ModuleOp module, xla::HloProto* hlo_proto,
                           bool use_tuple_args, bool always_return_tuple) {
  mlir::StatusScopedDiagnosticHandler diag_handler(module.getContext());
  ConvertToHloModule converter(module, use_tuple_args, always_return_tuple);
  if (failed(converter.Run())) return diag_handler.ConsumeStatus();
  auto hlo_module = converter.ConsumeMainProto();
  hlo_proto->mutable_hlo_module()->Swap(&hlo_module);
  return Status::OK();
}

}  // namespace mlir
