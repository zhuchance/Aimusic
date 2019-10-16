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
#include "tensorflow/compiler/xla/service/dynamic_padder.h"

#include <algorithm>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "tensorflow/compiler/xla/literal.h"
#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/compiler/xla/service/dynamic_dimension_inference.h"
#include "tensorflow/compiler/xla/service/hlo_dce.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/core/lib/core/errors.h"

namespace xla {

namespace {

// ChooseIdentityValue looks at the instruction's operand, returns a
// identity value which, when padded, doesn't change the result of the
// instruction.
//
// nullopt is returned if padding doesn't need to be reset.
StatusOr<HloInstruction*> ChooseIdentityValue(HloInstruction* inst,
                                              int64 operand_number) {
  HloComputation* comp = inst->parent();
  // Padding on elementwise operation doesn't affect the result of the effective
  // data.
  if (inst->IsElementwise()) {
    return nullptr;
  }

  switch (inst->opcode()) {
    case HloOpcode::kReduce: {
      TF_RET_CHECK(operand_number < inst->operand_count() / 2)
          << "Only data operand with dynamic dimension is valid.";
      // Variadic reduce has different init value for different operand, given a
      // data operand number, find the init value index.
      int64 init_value_index = inst->operand_count() / 2 + operand_number;
      return inst->mutable_operand(init_value_index);
    }
    case HloOpcode::kReduceWindow: {
      // Because of the way we do reduce, we already require the `init` operand
      // of hlo reduce instruction to be identity value. Here we reuse the
      // operand.
      return inst->mutable_operand(1);
    }

    case HloOpcode::kConvolution:
    case HloOpcode::kDot: {
      // Use 0 as padding value for convolution and dot.
      PrimitiveType ptype = inst->shape().element_type();
      return comp->AddInstruction(
          HloInstruction::CreateConstant(LiteralUtil::Zero(ptype)));
    }

    case HloOpcode::kPad: {
      return inst->mutable_operand(1);
    }

    case HloOpcode::kSelectAndScatter: {
      return inst->mutable_operand(2);
    }
    case HloOpcode::kScatter: {
      if (operand_number != 1) {
        return nullptr;
      }
      PrimitiveType indices_ptype =
          inst->operand(operand_number)->shape().element_type();

      return comp->AddInstruction(
          HloInstruction::CreateConstant(LiteralUtil::MaxValue(indices_ptype)));
    }
    case HloOpcode::kParameter:
    case HloOpcode::kGather:
    case HloOpcode::kDynamicSlice:
    case HloOpcode::kDynamicUpdateSlice:
    case HloOpcode::kGetDimensionSize:
    case HloOpcode::kSetDimensionSize:
    case HloOpcode::kConcatenate:
    case HloOpcode::kReshape:
    case HloOpcode::kReverse:
    case HloOpcode::kTuple:
    case HloOpcode::kAllReduce:
    case HloOpcode::kBroadcast:
    case HloOpcode::kTranspose:
    case HloOpcode::kSort:
    case HloOpcode::kSlice:
      return nullptr;
    default:
      return UnimplementedStrCat("Unimplemented padding for instruction: ",
                                 inst->ToString());
  }
}

bool ShouldSkipPadOnOperand(const HloInstruction* inst, int64 operand_num,
                            int64 dimension) {
  if ((inst->opcode() == HloOpcode::kReduceWindow ||
       inst->opcode() == HloOpcode::kSelectAndScatter) &&
      operand_num == 0 && inst->window().dimensions(dimension).size() == 1) {
    return true;
  }

  if (operand_num == 0 && inst->opcode() == HloOpcode::kConvolution &&
      inst->convolution_dimension_numbers().input_batch_dimension() ==
          dimension) {
    return true;
  }
  return false;
}

// Generates a mask representing the effective area of data and padded area of
// data using iota and dynamic_size. For example, given a dimension of 7
// elements and 5 effective elements:
//
// iota = [0, 1, 2, 3, 4, 5, 6]
// broadcast_dynamic_size = [5, 5, 5, 5, 5, 5, 5]
// mask = lt(iota, broadcast_dynamic_size) = [t, t, t, t, t, f, f]
//
// Once the mask is generated, the input data is then padded using the
// mask and pad value.
//
HloInstruction* PadWithScalar(HloInstruction* inst, int64 dim,
                              HloInstruction* dynamic_size,
                              HloInstruction* padding_scalar) {
  const Shape mask_shape =
      ShapeUtil::ChangeElementType(inst->shape(), xla::S32);
  const Shape pred_shape =
      ShapeUtil::ChangeElementType(inst->shape(), xla::PRED);
  HloComputation* computation = inst->parent();
  HloInstruction* iota =
      computation->AddInstruction(HloInstruction::CreateIota(mask_shape, dim));

  HloInstruction* broadcasted_effective_size = computation->AddInstruction(
      HloInstruction::CreateBroadcast(mask_shape, dynamic_size, {}));
  HloInstruction* pred =
      computation->AddInstruction(HloInstruction::CreateCompare(
          pred_shape, iota, broadcasted_effective_size,
          ComparisonDirection::kLt));

  HloInstruction* broadcasted_identity_value = computation->AddInstruction(
      HloInstruction::CreateBroadcast(inst->shape(), padding_scalar, {}));
  HloInstruction* padded = computation->AddInstruction(
      HloInstruction::CreateTernary(inst->shape(), HloOpcode::kSelect, pred,
                                    inst, broadcasted_identity_value));
  return padded;
}

// In a reshape if a dynamci dimension is splitted into multiple output
// dimensions, we need to rewrite the input of the reshape.
//
// The reason for this is that a continuous input may not be evenly reshaped
// into output.  Image we have [<=6] where valid data has size 4 and padding (P)
// data has size 2: [a,b,c,d,P,P]
//
// And we have a reshape that produces dynamic output dimensions.
//
// [<=6]
//  |
// Reshape
//  |
// [2, <=3]
//
// This should produce the same result as if the data has no padding:
//
// [4]     // [a, b, c, d]
//  |
// Reshape
//  |
// [2, 2]  // [[a,b], [c,d]]
//
// Without reshape rewriting, the result looks like:
//
// [[a,b,c]
//  [d,P,P]], which is incorrect.
//
// We need to rewrite the reshape such that it produces:
// [[a,b,P]
//  [c,d,P]]
//
// The way we do this is by a 6-steps double-sorting algorithm:
//
// 1.First we use the output shape to generate a binary 0-1 masking, which masks
// out the padded area of the output:
// [[0,0,1]
//  [0,0,1]]
//
// 2.Then we do an inverse reshape to reshape it from output shape back to input
// shape [2,3]->[6]:
//  [0,0,1,0,0,1]
//
// 3.We then generate an iota mask using the input shape:
//  [0,1,2,3,4,5]
//
// 4.Stable sort the iota mask using the binary mask as key:
//  key  [0,0,1,0,0,1]
//  value[0,1,2,3,4,5]
//     | Sort by key
//     v
//  key  [0,0,0,0,1,1]
//  value[0,1,3,4,2,5]
//
// 5.Sort the original input [a,b,c,d,P,P] using the sorted iota mask:
//  key  [0,1,3,4,2,5]
//  value[a,b,c,d,P,P]
//     | Sort by key
//     v
//  key  [0,1,2,3,4,5]
//  value[a,b,P,c,d,P]
//
// 6.Feed the sorted input to original reshape[6]->[2,3], we can get the correct
// reshape:
//  [[a,b,P]
//   [c,d,P]]
//
Status RewriteDynamicReshapeSplitInput(
    HloInstruction* reshape, int64 input_dim,
    absl::Span<const int64> output_dims,
    DynamicDimensionInference* dynamic_dimension_inference) {
  const Shape operand_shape = reshape->operand(0)->shape();
  TF_RET_CHECK(output_dims.size() > 1);

  HloComputation* comp = reshape->parent();
  const Shape mask_input_shape =
      ShapeUtil::ChangeElementType(operand_shape, xla::S32);
  const Shape mask_reshaped_shape =
      ShapeUtil::ChangeElementType(reshape->shape(), xla::S32);

  HloInstruction* zero = comp->AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::Zero(S32)));
  HloInstruction* one = comp->AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::One(S32)));
  // Step 1 -- generate binary mask.
  // Mask starts with all zero, each dynamic dimension sets one dimension of the
  // mask to partially one.
  HloInstruction* binary_mask = comp->AddInstruction(
      HloInstruction::CreateBroadcast(mask_reshaped_shape, zero, {}));

  bool need_rewrite = false;

  // Index starts from 1 since there is no need to rewrite a major output
  // dimension.
  for (int64 i = 1; i < output_dims.size(); ++i) {
    const int64 output_dim = output_dims[i];
    HloInstruction* dynamic_size =
        dynamic_dimension_inference->GetDynamicSize(reshape, {}, output_dim);
    if (dynamic_size == nullptr) {
      continue;
    }
    // If there is dynamic dimension in the output, need rewrite the input.
    need_rewrite = true;

    binary_mask = PadWithScalar(binary_mask, output_dim, dynamic_size, one);
  }
  if (!need_rewrite) {
    return Status::OK();
  }
  // Step 2.
  // Do a reverse reshape to flatten the binary mask (with output shape) back to
  // input shape.
  HloInstruction* input_shape_binary_mask = comp->AddInstruction(
      HloInstruction::CreateReshape(mask_input_shape, binary_mask));

  // Step 3. Generate iota mask.
  HloInstruction* iota_mask = comp->AddInstruction(
      HloInstruction::CreateIota(mask_input_shape, input_dim));

  // Step 4. Sort iota.
  // Use binary mark to sort iota mask, then use iota mask to reshape input.
  HloComputation::Builder comp_builder("compare_bianry_iota");
  {
    HloInstruction* lhs_key =
        comp_builder.AddInstruction(HloInstruction::CreateParameter(
            0, ShapeUtil::MakeShape(S32, {}), "lhs_key_binary"));
    HloInstruction* rhs_key =
        comp_builder.AddInstruction(HloInstruction::CreateParameter(
            1, ShapeUtil::MakeShape(S32, {}), "rhs_key_binary"));

    // Values for lhs and rhs
    comp_builder.AddInstruction(HloInstruction::CreateParameter(
        2, ShapeUtil::MakeShape(S32, {}), "lhs_iota"));
    comp_builder.AddInstruction(HloInstruction::CreateParameter(
        3, ShapeUtil::MakeShape(S32, {}), "rhs_iota"));
    comp_builder.AddInstruction(
        HloInstruction::CreateCompare(ShapeUtil::MakeShape(PRED, {}), lhs_key,
                                      rhs_key, ComparisonDirection::kLt));
  }

  HloComputation* compare_binary_iota =
      comp->parent()->AddEmbeddedComputation(comp_builder.Build());

  HloInstruction* sorted_binary_iota =
      comp->AddInstruction(HloInstruction::CreateSort(
          ShapeUtil::MakeTupleShape({mask_input_shape, mask_input_shape}),
          input_dim, {input_shape_binary_mask, iota_mask}, compare_binary_iota,
          /*is_stable=*/true));
  HloInstruction* sorted_iota_mask =
      comp->AddInstruction(HloInstruction::CreateGetTupleElement(
          mask_input_shape, sorted_binary_iota, 1));

  // Step 5. Sort original input using iota mask as key.
  HloComputation::Builder comp_builder_iota("compare_bianry_iota");
  {
    HloInstruction* lhs_key =
        comp_builder_iota.AddInstruction(HloInstruction::CreateParameter(
            0, ShapeUtil::MakeShape(S32, {}), "lhs_key_iota"));
    HloInstruction* rhs_key =
        comp_builder_iota.AddInstruction(HloInstruction::CreateParameter(
            1, ShapeUtil::MakeShape(S32, {}), "rhs_key_iota"));

    // Values for lhs and rhs
    comp_builder_iota.AddInstruction(HloInstruction::CreateParameter(
        2, ShapeUtil::MakeShape(operand_shape.element_type(), {}),
        "lhs_value"));
    comp_builder_iota.AddInstruction(HloInstruction::CreateParameter(
        3, ShapeUtil::MakeShape(operand_shape.element_type(), {}),
        "rhs_value"));
    comp_builder_iota.AddInstruction(
        HloInstruction::CreateCompare(ShapeUtil::MakeShape(PRED, {}), lhs_key,
                                      rhs_key, ComparisonDirection::kLt));
  }

  HloComputation* compare_iota_value =
      comp->parent()->AddEmbeddedComputation(comp_builder_iota.Build());

  // Temporarily removes dynamic dimension before entering sort -- we want the
  // sort to ignore dynamic dimension.
  HloInstruction* operand_static_dim_size =
      comp->AddInstruction(HloInstruction::CreateConstant(
          LiteralUtil::CreateR0<int32>(operand_shape.dimensions(input_dim))));

  HloInstruction* operand_static =
      comp->AddInstruction(HloInstruction::CreateSetDimensionSize(
          operand_shape, reshape->mutable_operand(0), operand_static_dim_size,
          input_dim));

  HloInstruction* sorted_iota_value =
      comp->AddInstruction(HloInstruction::CreateSort(
          ShapeUtil::MakeTupleShape({mask_input_shape, operand_shape}),
          input_dim, {sorted_iota_mask, operand_static}, compare_iota_value,
          /*is_stable=*/true));
  // Step 6: Feed sorted input to original reshape.
  HloInstruction* sorted_operand =
      comp->AddInstruction(HloInstruction::CreateGetTupleElement(
          operand_shape, sorted_iota_value, 1));

  TF_RETURN_IF_ERROR(reshape->ReplaceOperandWith(0, sorted_operand));

  HloInstruction* reshape_dynamic = reshape;

  auto users = reshape->users();

  // Forward the output dynamic dimension.
  for (int64 output_dim : output_dims) {
    HloInstruction* output_dynamic_size =
        dynamic_dimension_inference->GetDynamicSize(reshape, {}, output_dim);
    if (output_dynamic_size != nullptr) {
      reshape_dynamic =
          comp->AddInstruction(HloInstruction::CreateSetDimensionSize(
              reshape->shape(), reshape_dynamic, output_dynamic_size,
              output_dim));
    }
  }

  for (auto* user : users) {
    TF_RETURN_IF_ERROR(reshape->ReplaceUseWith(user, reshape_dynamic));
  }
  TF_RETURN_IF_ERROR(dynamic_dimension_inference->ForwardDynamicSize(
      reshape, reshape_dynamic, {}));

  return Status::OK();
}

Status RewriteDynamicReshapeCombineInput(
    HloInstruction* reshape, int64 input_dim, int64 output_dim,
    HloInstruction* dynamic_size,
    DynamicDimensionInference* dynamic_dimension_inference) {
  // Rewrite dynamic reshape into reshape followed by a sort, all padded
  // data will be moved to the end.
  const HloInstruction* operand = reshape->operand(0);
  HloComputation* comp = reshape->parent();
  HloInstruction* zero = comp->AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::Zero(S32)));
  HloInstruction* one = comp->AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::One(S32)));
  const Shape mask_shape =
      ShapeUtil::ChangeElementType(operand->shape(), xla::S32);
  const Shape mask_reshaped_shape =
      ShapeUtil::ChangeElementType(reshape->shape(), xla::S32);
  HloInstruction* broadcasted_zero = comp->AddInstruction(
      HloInstruction::CreateBroadcast(mask_shape, zero, {}));
  // Pad masking area with 1s, rest with 0s.
  HloInstruction* padding_mask =
      PadWithScalar(broadcasted_zero, input_dim, dynamic_size, one);
  HloInstruction* mask_reshaped = comp->AddInstruction(
      HloInstruction::CreateReshape(mask_reshaped_shape, padding_mask));

  // Build computation for reshape, key is the mask shape, value is reshape's
  // original data.
  HloComputation::Builder comp_builder("compare");
  HloInstruction* lhs_key =
      comp_builder.AddInstruction(HloInstruction::CreateParameter(
          0, ShapeUtil::MakeShape(S32, {}), "lhs_key"));
  HloInstruction* rhs_key =
      comp_builder.AddInstruction(HloInstruction::CreateParameter(
          1, ShapeUtil::MakeShape(S32, {}), "rhs_key"));

  // Values for lhs and rhs
  comp_builder.AddInstruction(HloInstruction::CreateParameter(
      2, ShapeUtil::MakeShape(operand->shape().element_type(), {}),
      "lhs_value"));
  comp_builder.AddInstruction(HloInstruction::CreateParameter(
      3, ShapeUtil::MakeShape(operand->shape().element_type(), {}),
      "rhs_value"));
  comp_builder.AddInstruction(
      HloInstruction::CreateCompare(ShapeUtil::MakeShape(PRED, {}), lhs_key,
                                    rhs_key, ComparisonDirection::kLt));
  HloComputation* compare =
      comp->parent()->AddEmbeddedComputation(comp_builder.Build());

  HloInstruction* static_dim_size = comp->AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::CreateR0<int32>(
          reshape->shape().dimensions(output_dim))));

  // Temporarily removes dynamic dimension of the reshape before we send it to
  // the sort -- we want padded area to also participate in the sort.
  HloInstruction* reshape_static =
      comp->AddInstruction(HloInstruction::CreateSetDimensionSize(
          reshape->shape(), reshape, static_dim_size, output_dim));

  // Use mask_reshaped as key, sort reshaped data as value.
  HloInstruction* sort = comp->AddInstruction(HloInstruction::CreateSort(
      ShapeUtil::MakeTupleShape({mask_reshaped_shape, reshape->shape()}),
      output_dim, {mask_reshaped, reshape_static}, compare,
      /*is_stable=*/true));
  HloInstruction* dynamic_reshape = comp->AddInstruction(
      HloInstruction::CreateGetTupleElement(reshape->shape(), sort, 1));
  // Forward dynamic size to the newly created reshape.
  HloInstruction* output_dynamic_size =
      dynamic_dimension_inference->GetDynamicSize(reshape, {}, output_dim);
  TF_RET_CHECK(output_dynamic_size != nullptr);
  dynamic_reshape = comp->AddInstruction(HloInstruction::CreateSetDimensionSize(
      dynamic_reshape->shape(), dynamic_reshape, output_dynamic_size,
      output_dim));
  auto users = reshape->users();
  for (auto* user : users) {
    // Avoid cycles by not replacing the staic reshape and get_dimension_size.
    if (user != reshape_static && user != output_dynamic_size) {
      TF_RETURN_IF_ERROR(reshape->ReplaceUseWith(user, dynamic_reshape));
    }
  }

  if (reshape == comp->root_instruction()) {
    comp->set_root_instruction(dynamic_reshape);
  }

  TF_RETURN_IF_ERROR(dynamic_dimension_inference->ForwardDynamicSize(
      reshape, dynamic_reshape, {}));

  return Status::OK();
}

Status RewriteDynamicReshapeSingleDim(
    HloInstruction* reshape, int64 input_dim, HloInstruction* dynamic_size,
    DynamicDimensionInference* dynamic_dimension_inference) {
  VLOG(2) << "Rewriting dynamic reshape " << reshape->ToString()
          << " input dim: " << input_dim;
  const Shape operand_shape = reshape->operand(0)->shape();
  const Shape output_shape = reshape->shape();

  const int64 static_input_dim_size = operand_shape.dimensions()[input_dim];

  // Don't need to rewrite size 1 input dims.
  if (static_input_dim_size == 1) {
    return Status::OK();
  }

  auto common_factors =
      CommonFactors(operand_shape.dimensions(), output_shape.dimensions());
  // If there are multiple input dims combining into one output dim,
  // input_dim_start and input_dim_end represent the input dimension range.
  int64 input_dim_start = -1;
  int64 input_dim_end = -1;
  // Similarly when one input dim is splitted into multiple outputs, we use
  // output_dim_start and output_dim_start to represent the output dimension
  // range.
  int64 output_dim_start = -1;
  int64 output_dim_end = -1;
  // Find common_factors that the input belong to.
  for (int64 i = 0; i < common_factors.size() - 1; ++i) {
    auto start = common_factors[i];
    auto end = common_factors[i + 1];
    if (input_dim >= start.first && input_dim < end.first) {
      // Found the common_factor group that the input_dim belongs to.
      input_dim_start = start.first;
      input_dim_end = end.first;
      output_dim_start = start.second;
      output_dim_end = end.second;
    }
  }

  TF_RET_CHECK(output_dim_end - output_dim_start > 0);

  std::vector<int64> output_dims;
  for (int64 i = output_dim_start; i < output_dim_end; ++i) {
    output_dims.push_back(i);
  }

  const int64 first_output_dim = output_dims[0];

  if (reshape->shape().dimensions(first_output_dim) < static_input_dim_size) {
    // One input dimension is splitted into multiple output dimensions.
    return RewriteDynamicReshapeSplitInput(reshape, input_dim, output_dims,
                                           dynamic_dimension_inference);
  }

  if (reshape->shape().dimensions(first_output_dim) == static_input_dim_size) {
    // Unchanged dynamic dimension doesn't need a rewrite.
    return Status::OK();
  }

  // Multiple dimensions got combined into one output.
  if (input_dim != input_dim_start) {
    // If 'input_dim' is not the first dimension that got combined into the
    // output. A reshape rewrite on the output is needed:
    //
    //  Need a write (d is dynamic):
    //  1, 2, d
    //   |
    //  Reshape
    //   |
    //   2d
    //
    //  Don't need rewrite:
    //  d, 2
    //   |
    //  Reshape
    //   |
    //   2d
    //
    return RewriteDynamicReshapeCombineInput(reshape, input_dim,
                                             first_output_dim, dynamic_size,
                                             dynamic_dimension_inference);
  }
  return Status::OK();
}

StatusOr<bool> RewriteDynamicReshape(
    HloInstruction* reshape,
    DynamicDimensionInference* dynamic_dimension_inference) {
  bool changed = false;
  HloInstruction* operand = reshape->mutable_operand(0);

  // We append sort instructions after reshape if there is a dynamic input, and
  // the order of sort matters. Rewrite minor dimensions first in case multiple
  // inputs have dynamic dimensions to ensure correct order of sort.
  for (int64 input_dim = operand->shape().rank() - 1; input_dim >= 0;
       --input_dim) {
    HloInstruction* operand_dynamic_size =
        dynamic_dimension_inference->GetDynamicSize(operand, {}, input_dim);

    if (operand_dynamic_size == nullptr) {
      continue;
    }
    TF_RETURN_IF_ERROR(RewriteDynamicReshapeSingleDim(
        reshape, input_dim, operand_dynamic_size, dynamic_dimension_inference));

    changed = true;
  }
  return changed;
}

// For all dynamic outputs that live out of the computation, add unpad
// operations.
Status InsertUnpadsForModuleOutputs(
    const DynamicDimensionInference& dynamic_dimension_inference,
    HloModule* module) {
  auto root = module->entry_computation()->root_instruction();
  absl::flat_hash_set<ShapeIndex> dynamic_outputs;
  ShapeUtil::ForEachSubshape(
      root->shape(), [&](const Shape& subshape, const ShapeIndex& index) {
        if (subshape.IsArray()) {
          bool has_dynamic_output = false;
          for (int64 dim = 0; dim < subshape.rank(); ++dim) {
            if (dynamic_dimension_inference.GetDynamicSize(root, index, dim) !=
                nullptr) {
              CHECK_LE(index.size(), 1) << "XLA doesn't support nested output "
                                           "dimensions that has dynamic size";
              has_dynamic_output = true;
            }
          }
          if (has_dynamic_output) {
            dynamic_outputs.insert(index);
          }
        }
      });
  int64 dynamic_index = 0;
  if (!dynamic_outputs.empty()) {
    if (root->shape().IsTuple()) {
      std::vector<HloInstruction*> new_root_operands;
      ShapeUtil::ForEachSubshape(root->shape(), [&](const Shape& subshape,
                                                    const ShapeIndex& index) {
        if (!subshape.IsArray()) {
          return;
        }
        auto gte = module->entry_computation()->AddInstruction(
            HloInstruction::CreateGetTupleElement(subshape, root, index[0]));

        if (dynamic_outputs.contains(index)) {
          CHECK_EQ(index.size(), 1)
              << "XLA only support 1 layer nested output tuple";
          // For dynamic outputs, creates an unpad operation.
          std::vector<HloInstruction*> unpad_operands;
          // First operand is the original input. Rest are dimension values.
          unpad_operands.push_back(gte);
          for (int64 dim = 0; dim < subshape.rank(); ++dim) {
            HloInstruction* dynamic_size =
                dynamic_dimension_inference.GetDynamicSize(root, index, dim);
            if (dynamic_size != nullptr) {
              unpad_operands.push_back(dynamic_size);
            } else {
              auto const_size = HloInstruction::CreateConstant(
                  LiteralUtil::CreateR0<int32>(subshape.dimensions(dim)));
              unpad_operands.push_back(
                  module->entry_computation()->AddInstruction(
                      std::move(const_size)));
            }
          }
          // This is a dynamic output, add unpad operation.
          auto unpad = HloInstruction::CreateCustomCall(
              subshape, unpad_operands, "Unpad",
              absl::StrFormat("%i", dynamic_index++));
          new_root_operands.push_back(
              module->entry_computation()->AddInstruction(std::move(unpad)));
        } else {
          new_root_operands.push_back(gte);
        }
      });

      auto new_root = module->entry_computation()->AddInstruction(
          HloInstruction::CreateTuple(new_root_operands));
      module->entry_computation()->set_root_instruction(new_root);
    } else {
      std::vector<HloInstruction*> unpad_operands;
      // First operand is the original input. Rest are dimension values.
      unpad_operands.push_back(root);
      for (int64 dim = 0; dim < root->shape().rank(); ++dim) {
        HloInstruction* dynamic_size =
            dynamic_dimension_inference.GetDynamicSize(root, {}, dim);
        if (dynamic_size != nullptr) {
          unpad_operands.push_back(dynamic_size);
        } else {
          auto const_size = HloInstruction::CreateConstant(
              LiteralUtil::CreateR0<int32>(root->shape().dimensions(dim)));
          unpad_operands.push_back(module->entry_computation()->AddInstruction(
              std::move(const_size)));
        }
        // This is a dynamic output, add unpad operation.
        auto unpad = module->entry_computation()->AddInstruction(
            HloInstruction::CreateCustomCall(root->shape(), unpad_operands,
                                             "Unpad", "0"));
        module->entry_computation()->set_root_instruction(unpad);
      }
    }
  }
  return Status::OK();
}

}  // namespace

StatusOr<bool> DynamicPadder::Run(HloModule* module) {
  bool changed = false;
  VLOG(2) << "Pre DynamicPadder HLO:";
  XLA_VLOG_LINES(2, module->ToString());
  TF_ASSIGN_OR_RETURN(DynamicDimensionInference dynamic_dimension_inference,
                      DynamicDimensionInference::Run(module));

  for (HloComputation* computation : module->computations()) {
    for (HloInstruction* inst : computation->instructions()) {
      for (int64 operand_num = 0; operand_num < inst->operand_count();
           ++operand_num) {
        HloInstruction* original_operand = inst->mutable_operand(operand_num);
        HloInstruction* operand = original_operand;
        if (!operand->shape().IsArray()) {
          continue;
        }

        if (inst->opcode() == HloOpcode::kReshape) {
          TF_ASSIGN_OR_RETURN(changed, RewriteDynamicReshape(
                                           inst, &dynamic_dimension_inference));
          continue;
        }
        for (int64 input_dim = 0; input_dim < operand->shape().rank();
             ++input_dim) {
          HloInstruction* operand_dynamic_size =
              dynamic_dimension_inference.GetDynamicSize(original_operand, {},
                                                         input_dim);
          if (operand_dynamic_size == nullptr) {
            continue;
          }
          VLOG(2) << "Has dynamic dimension of operand" << operand_num << " @"
                  << input_dim;

          if (ShouldSkipPadOnOperand(inst, operand_num, input_dim)) {
            continue;
          }

          TF_ASSIGN_OR_RETURN(HloInstruction * identity_value,
                              ChooseIdentityValue(inst, operand_num));
          if (identity_value == nullptr) {
            continue;
          }

          HloInstruction* padded = PadWithScalar(
              operand, input_dim, operand_dynamic_size, identity_value);
          TF_RETURN_IF_ERROR(inst->ReplaceOperandWith(operand_num, padded));
          operand = inst->mutable_operand(operand_num);
          changed = true;
        }
      }
    }
  }

  TF_RETURN_IF_ERROR(
      InsertUnpadsForModuleOutputs(dynamic_dimension_inference, module));

  HloDCE dce;
  TF_ASSIGN_OR_RETURN(changed, dce.Run(module));

  VLOG(2) << "Post DynamicPadder HLO:";
  XLA_VLOG_LINES(2, module->ToString());

  return changed;
}

}  // namespace xla
