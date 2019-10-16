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

#ifndef TENSORFLOW_LITE_DELEGATES_GPU_CL_KERNELS_CONV_POWERVR_H_
#define TENSORFLOW_LITE_DELEGATES_GPU_CL_KERNELS_CONV_POWERVR_H_

#include <vector>

#include "tensorflow/lite/delegates/gpu/cl/buffer.h"
#include "tensorflow/lite/delegates/gpu/cl/cl_device.h"
#include "tensorflow/lite/delegates/gpu/cl/kernels/gpu_operation.h"
#include "tensorflow/lite/delegates/gpu/cl/kernels/util.h"
#include "tensorflow/lite/delegates/gpu/cl/linear_storage.h"
#include "tensorflow/lite/delegates/gpu/cl/tensor.h"
#include "tensorflow/lite/delegates/gpu/cl/util.h"
#include "tensorflow/lite/delegates/gpu/common/data_type.h"
#include "tensorflow/lite/delegates/gpu/common/operations.h"
#include "tensorflow/lite/delegates/gpu/common/shape.h"
#include "tensorflow/lite/delegates/gpu/common/status.h"
#include "tensorflow/lite/delegates/gpu/common/tensor.h"
#include "tensorflow/lite/delegates/gpu/common/types.h"

namespace tflite {
namespace gpu {
namespace cl {

class ConvPowerVR : public GPUOperation {
 public:
  ConvPowerVR() = default;
  Status AddToQueue(CLCommandQueue* queue) override;

  Status Compile(const CreationContext& creation_context) override;

  // Move only
  ConvPowerVR(ConvPowerVR&& operation);
  ConvPowerVR& operator=(ConvPowerVR&& operation);
  ConvPowerVR(const ConvPowerVR&) = delete;
  ConvPowerVR& operator=(const ConvPowerVR&) = delete;

 private:
  struct ConvParams {
    int3 block_size;
    int3 work_group_size;
    int3 work_group_launch_order;
    int src_depth_loop_size;
    bool explicit_sync;
    bool x_kernel_is_1;
    bool y_kernel_is_1;
  };

  ConvPowerVR(const OperationDef& definition,
              const Convolution2DAttributes& attr, const CLDevice& device);
  ConvPowerVR(const OperationDef& definition,
              const FullyConnectedAttributes& attr, const CLDevice& device);

  template <DataType T>
  Status UploadData(const ::tflite::gpu::Tensor<OHWI, T>& weights,
                    const ::tflite::gpu::Tensor<Linear, T>& biases,
                    CLContext* context);
  template <DataType T>
  Status UploadWeights(const ::tflite::gpu::Tensor<OHWI, T>& weights,
                       CLContext* context);
  template <DataType S, typename T>
  void RearrangeWeight(const ::tflite::gpu::Tensor<OHWI, S>& weights,
                       absl::Span<T> dst);

  friend Status CreateConvPowerVR(const CreationContext& creation_context,
                                  const OperationDef& definition,
                                  const Convolution2DAttributes& attr,
                                  ConvPowerVR* result);

  friend Status CreateConvPowerVR(const CreationContext& creation_context,
                                  const OperationDef& definition,
                                  const FullyConnectedAttributes& attr,
                                  ConvPowerVR* result);

  friend std::string GenerateConvPowerVR1x1(
      const OperationDef& op_def, bool stride_correction,
      const ConvParams& conv_params,
      const std::vector<ElementwiseOperation*>& linked_operations);

  ConvParams GuessBestParams(const CLDevice& device,
                             const OperationDef& definition,
                             const Convolution2DAttributes& attr) const;
  ConvParams GuessBestParams(const CLDevice& device,
                             const OperationDef& definition,
                             const FullyConnectedAttributes& attr) const;
  ConvParams GuessBestParams(const CLDevice& device,
                             const OperationDef& definition, int src_depth,
                             int dst_depth, bool x_kernel_is_1,
                             bool y_kernel_is_1) const;

  Status BindArguments();
  int3 GetGridSize() const;

  Buffer weights_;
  LinearStorage biases_;

  int4 stride_padding_;
  int4 kernel_dilation_;
  ConvParams conv_params_;

  CLKernel kernel_;
};

template <DataType T>
Status ConvPowerVR::UploadData(const ::tflite::gpu::Tensor<OHWI, T>& weights,
                               const ::tflite::gpu::Tensor<Linear, T>& biases,
                               CLContext* context) {
  RETURN_IF_ERROR(UploadWeights(weights, context));
  LinearStorageCreateInfo create_info;
  create_info.storage_type = LinearStorageType::BUFFER;
  create_info.data_type = definition_.precision == CalculationsPrecision::F16
                              ? DataType::FLOAT16
                              : DataType::FLOAT32;
  create_info.aligned_size = weights.shape.o;
  RETURN_IF_ERROR(CreateLinearStorage(create_info, biases, context, &biases_));
  return OkStatus();
}

template <DataType T>
Status ConvPowerVR::UploadWeights(const ::tflite::gpu::Tensor<OHWI, T>& weights,
                                  CLContext* context) {
  const int dst_depth = IntegralDivideRoundUp(weights.shape.o, 4);
  const int src_depth = IntegralDivideRoundUp(weights.shape.i, 4);

  const bool f32_weights = definition_.precision != CalculationsPrecision::F16;
  const int float4_size = f32_weights ? sizeof(float4) : sizeof(half4);

  const int dst_depth_aligned = AlignByN(dst_depth, conv_params_.block_size.z);
  const int elements_count =
      weights.shape.h * weights.shape.w * src_depth * dst_depth_aligned * 4;

  if (f32_weights) {
    std::vector<float4> gpu_data(elements_count);
    RearrangeWeight(weights, absl::MakeSpan(gpu_data));
    return CreateReadOnlyBuffer(float4_size * elements_count, gpu_data.data(),
                                context, &weights_);
  } else {
    std::vector<half4> gpu_data(elements_count);
    RearrangeWeight(weights, absl::MakeSpan(gpu_data));
    return CreateReadOnlyBuffer(float4_size * elements_count, gpu_data.data(),
                                context, &weights_);
  }
}

template <DataType S, typename T>
void ConvPowerVR::RearrangeWeight(const ::tflite::gpu::Tensor<OHWI, S>& weights,
                                  absl::Span<T> dst) {
  const int dst_depth = IntegralDivideRoundUp(weights.shape.o, 4);
  const int src_depth = IntegralDivideRoundUp(weights.shape.i, 4);
  const int kernel_x = weights.shape.w;
  const int kernel_y = weights.shape.h;

  int counter = 0;
  for (int d = 0;
       d < IntegralDivideRoundUp(dst_depth, conv_params_.block_size.z); ++d) {
    for (int y = 0; y < kernel_y; ++y) {
      for (int x = 0; x < kernel_x; ++x) {
        for (int s = 0; s < src_depth; ++s) {
          for (int k = 0; k < conv_params_.block_size.z; ++k) {
            T filters[4];
            for (int i = 0; i < 4; ++i) {
              for (int j = 0; j < 4; ++j) {
                const int s_ch = s * 4 + j;
                const int d_ch = (d * conv_params_.block_size.z + k) * 4 + i;
                if (s_ch < weights.shape.i && d_ch < weights.shape.o) {
                  const int f_index =
                      weights.shape.LinearIndex({d_ch, y, x, s_ch});
                  filters[j][i] = weights.data[f_index];
                } else {
                  filters[j][i] = 0.0f;
                }
              }
            }
            dst[counter++] = filters[0];
            dst[counter++] = filters[1];
            dst[counter++] = filters[2];
            dst[counter++] = filters[3];
          }
        }
      }
    }
  }
}

Status CreateConvPowerVR(const CreationContext& creation_context,
                         const OperationDef& definition,
                         const Convolution2DAttributes& attr,
                         ConvPowerVR* result);

Status CreateConvPowerVR(const CreationContext& creation_context,
                         const OperationDef& definition,
                         const FullyConnectedAttributes& attr,
                         ConvPowerVR* result);

}  // namespace cl
}  // namespace gpu
}  // namespace tflite

#endif  // TENSORFLOW_LITE_DELEGATES_GPU_CL_KERNELS_CONV_POWERVR_H_
