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

// This transformation pass applies quantization propagation on TFLite dialect.
#include <string>

#include "absl/memory/memory.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "mlir/IR/MLIRContext.h"  // TF:local_config_mlir
#include "mlir/IR/PatternMatch.h"  // TF:local_config_mlir
#include "mlir/Pass/Pass.h"  // TF:local_config_mlir
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/lite/quantization/quantization_config.h"
#include "tensorflow/compiler/mlir/lite/quantization/quantization_utils.h"
#include "tensorflow/compiler/mlir/lite/transforms/passes.h"
#include "tensorflow/core/framework/types.pb.h"

// NOLINTNEXTLINE
static llvm::cl::list<std::string> quantize_whitelist(
    "tfl-test-quantize-whitelist", llvm::cl::value_desc("list"),
    llvm::cl::desc("comma seprarated list of whitelisted functions to be "
                   "quantized. Only used in tests"),
    llvm::cl::CommaSeparated);

// NOLINTNEXTLINE
static llvm::cl::opt<bool> quantize_signed(
    "tfl-test-quantize-signed", llvm::cl::value_desc("bool"),
    llvm::cl::desc("signed inference type. Only used in tests"),
    llvm::cl::init(false));

//===----------------------------------------------------------------------===//
// The prepare-quantize Pass.
//
namespace mlir {
namespace TFL {

namespace {

// Applies prepare quantization on the model in TFL dialect. This pass runs
// before the quantization pass and propagate the quantization parameters
// across ops. This step is necessary for post-training quantization and also
// making the quantization rule for some operations in the quantization-aware
// training quantization simpler.
class PrepareQuantizePass : public FunctionPass<PrepareQuantizePass> {
 public:
  // Constructor used by the PassRegistration and enforce uint8 quantization.
  explicit PrepareQuantizePass() {
    if (quantize_signed)
      quant_specs_.inference_type = tensorflow::DT_QINT8;
    else
      quant_specs_.inference_type = tensorflow::DT_QUINT8;
  }

  // Constructor used by manually creating the pass.
  explicit PrepareQuantizePass(const QuantizationSpecs& quant_specs)
      : quant_specs_(quant_specs) {}

  void runOnFunction() override;

 private:
  // Set the quantization parameters of the input nodes. These parameters are
  // converted from the user specified input value ranges. The input nodes with
  // non-float tensor types will be skipped because they are not quantizable.
  // Return true if number of input nodes doesn't equal to that of the input
  // ranges.
  bool SetInputNodesQuantizationParams(FuncOp func);

  // Verify the quantization specification is expected for quantizing the
  // current function.
  bool IsLegalQuantSpecs(FuncOp func) {
    if (func.getName() == quant_specs_.target_func) {
      return func.getNumArguments() == quant_specs_.input_ranges.size();
    }
    return true;
  }

  // Get the min and max values from the quantization specification for the
  // current function function and argument index. Uses default values if
  // the function is specified in the `quantize_whitelist`.
  std::pair<double, double> GetMinMaxValuesForArgument(
      llvm::StringRef func_name, int index) {
    if (func_name == quant_specs_.target_func) {
      return quant_specs_.input_ranges[index];
    } else {
      return {0.0, 255.0};
    }
  }

  QuantizationSpecs quant_specs_;
};

bool PrepareQuantizePass::SetInputNodesQuantizationParams(FuncOp func) {
  StringRef func_name = func.getName();
  auto& target_func = quant_specs_.target_func;

  // Skip this function because it isn't the target function from the spec or
  // in the function while list.
  if (target_func != func_name &&
      !llvm::is_contained(quantize_whitelist, func_name)) {
    return false;
  }

  // If the validation fails, the pass should stop immediately.
  if (!IsLegalQuantSpecs(func)) {
    return true;
  }

  OpBuilder builder(func);
  bool is_signed = quant_specs_.IsSignedInferneceType();
  IntegerAttr num_bits =
      builder.getI32IntegerAttr(quant_specs_.GetQuantizationTypeWidth());
  BoolAttr narrow_range = builder.getBoolAttr(false);

  for (int i = 0, e = func.getNumArguments(); i != e; ++i) {
    Value* arg = func.getArgument(i);
    if (!arg->hasOneUse() ||
        !llvm::isa<TFL::InputOp>(*arg->getUsers().begin())) {
      return true;
    }

    Operation* input = *arg->getUsers().begin();
    auto input_op = llvm::cast<TFL::InputOp>(input);
    Location loc = input_op.getLoc();
    Type input_type = input_op.input()->getType();

    if (auto shaped = input_type.dyn_cast<ShapedType>()) {
      if (shaped.getElementType().isa<FloatType>()) {
        auto min_max = GetMinMaxValuesForArgument(func_name, i);
        TypeAttr params = GetQuantizedTypeAttr(
            builder, input_type, builder.getF64FloatAttr(min_max.first),
            builder.getF64FloatAttr(min_max.second), num_bits, narrow_range,
            is_signed);
        builder.setInsertionPoint(input->getBlock(),
                                  ++Block::iterator(input_op));
        auto q_op = builder.create<TFL::QuantizeOp>(loc, params.getValue(),
                                                    input_op.output(), params);
        auto dq_op =
            builder.create<TFL::DequantizeOp>(loc, input_type, q_op.output());
        input_op.output()->replaceAllUsesWith(dq_op.output());
        q_op.setOperand(input_op.output());
      }
    }
  }

  return false;
}

#include "tensorflow/compiler/mlir/lite/utils/generated_op_quant_spec_getters.inc"

using PrepareQuantStats =
    TFL::ConvertStatsToQDQs<TFL::QuantizeOp, TFL::DequantizeOp>;

void PrepareQuantizePass::runOnFunction() {
  FuncOp func = getFunction();
  MLIRContext* ctx = func.getContext();
  // Set the quantization parameters for the quantizable input nodes. If this
  // failed, return the function immediately. This is only required for
  // quantization aware training model conversion.
  // TODO(fengliuai): send the signal to the pass manager.
  if (!quant_specs_.post_training_quantization &&
      SetInputNodesQuantizationParams(func)) {
    return;
  }

  // During the legalization, unsigned quantized type is used, so we have to
  // convert all of them to signed.
  OwningRewritePatternList patterns;
  bool is_signed = quant_specs_.IsSignedInferneceType();
  if (is_signed) {
    patterns.insert<ConvertUnsignedToSigned<TFL::QuantizeOp>>(ctx);
    // Convert quant stats to int8 quantization parameters.
    // Currently, only activation stats are imported, so narrow_range = false.
    patterns.insert<PrepareQuantStats>(8, false, true, ctx);
  } else {
    // Convert quant stats to uint8 quantization parameters.
    // Currently, only activation stats are imported, so narrow_range = false.
    patterns.insert<PrepareQuantStats>(8, false, false, ctx);
  }
  applyPatternsGreedily(func, patterns);

  // Finally, the quantization parameters can be propagated to the rest of the
  // values (tensors).
  ApplyQuantizationParamsPropagation(func, is_signed, GetOpQuantSpec);
}

}  // namespace

// Creates an instance of the TensorFlow Lite dialect PrepareQuantize pass.
std::unique_ptr<OpPassBase<FuncOp>> CreatePrepareQuantizePass(
    const QuantizationSpecs& quant_specs) {
  return std::make_unique<PrepareQuantizePass>(quant_specs);
}

static PassRegistration<PrepareQuantizePass> pass(
    "tfl-prepare-quantize", "Prepare TFL dialect for quantization");

}  // namespace TFL
}  // namespace mlir
