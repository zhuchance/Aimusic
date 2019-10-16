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

#include "tensorflow/lite/delegates/gpu/cl/kernels/upsample.h"

#include "tensorflow/lite/delegates/gpu/cl/kernels/util.h"
#include "tensorflow/lite/delegates/gpu/cl/kernels/work_group_picking.h"
#include "tensorflow/lite/delegates/gpu/cl/tensor_type.h"

namespace tflite {
namespace gpu {
namespace cl {
namespace {

std::string GetUpsampleCode(
    const OperationDef& op_def,
    const std::vector<ElementwiseOperation*>& linked_operations) {
  TensorCodeGenerator src_tensor("src_data",
                                 {"src_size.x", "src_size.y", "src_size.z"},
                                 op_def.src_tensors[0]);
  TensorCodeGenerator dst_tensor("dst_data",
                                 {"dst_size.x", "dst_size.y", "dst_size.z"},
                                 op_def.dst_tensors[0]);

  std::string c = GetCommonDefines(op_def.precision);
  c += "__kernel void main_function(\n";
  c += src_tensor.GetDeclaration(AccessType::READ);
  c += GetArgsDeclaration(linked_operations);
  c += dst_tensor.GetDeclaration(AccessType::WRITE) + ",\n";
  c += "    int4 src_size,         \n";
  c += "    int4 dst_size,         \n";
  c += "    int2 border,           \n";
  c += "    float2 scale_factor    \n";
  c += ") {\n";
  c += "  int Y = get_global_id(1);\n";
  c += "  int Z = get_global_id(2);\n";
  if (op_def.batch_support) {
    c += "  int linear_id = get_global_id(0);\n";
    c += "  int X = linear_id / dst_size.w;\n";
    c += "  int B = linear_id % dst_size.w;\n";
    c += "  if (get_global_id(0) >= dst_size.x || Y >= dst_size.y || Z >= "
         "dst_size.z) return;\n";
  } else {
    c += "  int X = get_global_id(0);\n";
    c += "  if (X >= dst_size.x || Y >= dst_size.y || Z >= dst_size.z) "
         "return;\n";
  }
  c += "  float2 f_coords = (float2)(X, Y) * scale_factor;\n";
  c += "  int4 st;\n";
  c += "  st.xy = (int2)(f_coords.x, f_coords.y);\n";
  c += "  st.zw = min(st.xy + (int2)(1, 1), border);\n";
  c += "  float2 t = f_coords - (float2)(st.x, st.y);\n";
  if (op_def.batch_support) {
    c += "  st.x = st.x * src_size.w + B;\n";
    c += "  st.z = st.z * src_size.w + B;\n";
    c += "  X = X * dst_size.w + B;\n";
  }
  c += "  float4 src0 = " + src_tensor.ReadAsFloat3D("st.x", "st.y", "Z") +
       ";\n";
  c += "  float4 src1 = " + src_tensor.ReadAsFloat3D("st.z", "st.y", "Z") +
       ";\n";
  c += "  float4 src2 = " + src_tensor.ReadAsFloat3D("st.x", "st.w", "Z") +
       ";\n";
  c += "  float4 src3 = " + src_tensor.ReadAsFloat3D("st.z", "st.w", "Z") +
       ";\n";
  c += "  FLT4 r0 = TO_FLT4(mix(mix(src0, src1, t.x), mix(src2, src3, t.x), "
       "t.y));\n";
  const LinkingContext context{"r0", "X", "Y", "Z"};
  c += PostProcess(linked_operations, context);
  c += "  " + dst_tensor.Write3D("r0", "X", "Y", "Z");
  c += "}\n";
  return c;
}

}  // namespace

Upsample::Upsample(Upsample&& operation)
    : GPUOperation(std::move(operation)),
      attr_(operation.attr_),
      kernel_(std::move(operation.kernel_)),
      work_group_size_(operation.work_group_size_) {}

Upsample& Upsample::operator=(Upsample&& operation) {
  if (this != &operation) {
    attr_ = operation.attr_;
    kernel_ = std::move(operation.kernel_);
    std::swap(work_group_size_, operation.work_group_size_);
    GPUOperation::operator=(std::move(operation));
  }
  return *this;
}

Status Upsample::Compile(const CreationContext& creation_context) {
  const auto code = GetUpsampleCode(definition_, linked_operations_);
  return creation_context.cache->GetOrCreateCLKernel(
      code, "main_function", *creation_context.context,
      *creation_context.device, &kernel_);
}

Status Upsample::BindArguments() {
  kernel_.ResetBindingCounter();
  RETURN_IF_ERROR(kernel_.SetMemoryAuto(src_[0]->GetMemoryPtr()));
  RETURN_IF_ERROR(BindArgs(&kernel_, linked_operations_));
  RETURN_IF_ERROR(kernel_.SetMemoryAuto(dst_[0]->GetMemoryPtrForWriting()));
  RETURN_IF_ERROR(kernel_.SetBytesAuto(src_[0]->GetWBatchedHDB()));
  RETURN_IF_ERROR(kernel_.SetBytesAuto(dst_[0]->GetWBatchedHDB()));
  RETURN_IF_ERROR(
      kernel_.SetBytesAuto(int2(src_[0]->Width() - 1, src_[0]->Height() - 1)));
  float2 scale_factor =
      float2(CalculateResizeScale(src_[0]->Width(), dst_[0]->Width(), attr_),
             CalculateResizeScale(src_[0]->Height(), dst_[0]->Height(), attr_));
  RETURN_IF_ERROR(kernel_.SetBytesAuto(scale_factor));
  return OkStatus();
}

int3 Upsample::GetGridSize() const {
  const int grid_x = dst_[0]->Width() * dst_[0]->Batch();
  const int grid_y = dst_[0]->Height();
  const int grid_z = dst_[0]->Depth();
  return int3(grid_x, grid_y, grid_z);
}

Status Upsample::AddToQueue(CLCommandQueue* queue) {
  RETURN_IF_ERROR(BindArguments());
  return queue->DispatchImplicit(kernel_, GetGridSize(), work_group_size_);
}

Status Upsample::Tune(const TuningParameters& params) {
  RETURN_IF_ERROR(BindArguments());
  return GetBestWorkGroup(params, kernel_, GetGridSize(), &work_group_size_);
}

Upsample CreateUpsample(const OperationDef& definition,
                        const Upsample2DAttributes& attr) {
  return Upsample(definition, attr);
}

}  // namespace cl
}  // namespace gpu
}  // namespace tflite
