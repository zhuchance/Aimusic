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

#include "tensorflow/lite/delegates/gpu/delegate.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "tensorflow/lite/builtin_ops.h"
#include "tensorflow/lite/delegates/gpu/api.h"
#include "tensorflow/lite/delegates/gpu/cl/api.h"
#include "tensorflow/lite/delegates/gpu/cl/opencl_wrapper.h"
#include "tensorflow/lite/delegates/gpu/cl/tensor_type_util.h"
#include "tensorflow/lite/delegates/gpu/common/model.h"
#include "tensorflow/lite/delegates/gpu/common/model_builder.h"
#include "tensorflow/lite/delegates/gpu/common/model_transformer.h"
#include "tensorflow/lite/delegates/gpu/common/status.h"
#include "tensorflow/lite/delegates/gpu/common/transformations/general_transformations.h"
#include "tensorflow/lite/delegates/gpu/gl/api2.h"
#include "tensorflow/lite/minimal_logging.h"

namespace tflite {
namespace gpu {
namespace {

// Forward declarations.
TfLiteStatus DelegatePrepare(TfLiteContext* context, TfLiteDelegate* delegate);

class Delegate {
 public:
  explicit Delegate(const TfLiteGpuDelegateOptionsV2* options) {
    options_ = options ? *options : TfLiteGpuDelegateOptionsV2Default();
  }

  Status Prepare(TfLiteContext* context,
                 const TfLiteDelegateParams* delegate_params) {
    // Extract TFLite delegate execution plan from the context and convert it
    // into FlowGraph32.
    GraphFloat32 graph;
    RETURN_IF_ERROR(BuildModel(context, delegate_params, &graph));

    // Apply general transformations on the graph.
    NullTransformationReporter reporter;
    ModelTransformer transformer(&graph, &reporter);
    if (!ApplyGeneralTransformations(&transformer)) {
      return InternalError("Graph general transformations failed");
    }

    std::vector<uint32_t> input_refs;
    {
      const auto& inputs = graph.inputs();
      input_refs.reserve(inputs.size());
      for (auto input : inputs) {
        input_refs.push_back(input->tensor.ref);
      }
    }
    std::vector<uint32_t> output_refs;
    {
      const auto& outputs = graph.outputs();
      output_refs.reserve(outputs.size());
      for (auto output : outputs) {
        output_refs.push_back(output->tensor.ref);
      }
    }

    std::unique_ptr<InferenceBuilder> builder;
    Status status = InitializeOpenClApi(&graph, &builder);
    if (!status.ok()) {
      context->ReportError(context, "%s", status.error_message().c_str());
      context->ReportError(context, "Falling back to OpenGL");
      RETURN_IF_ERROR(InitializeOpenGlApi(&graph, &builder));
    }

    // At this point tflite didn't allocate tensors yet, therefore, collect
    // indices and set all input and output tensors from tflite later.
    input_indices_.reserve(input_refs.size());
    for (uint32_t tensor_index : input_refs) {
      const int64_t object_index = input_indices_.size();
      input_indices_.push_back(tensor_index);
      RETURN_IF_ERROR(
          builder->SetInputObjectDef(object_index, GetObjectDef(tensor_index)));
    }
    output_indices_.reserve(output_refs.size());
    for (uint32_t tensor_index : output_refs) {
      const int64_t object_index = output_indices_.size();
      output_indices_.push_back(tensor_index);
      RETURN_IF_ERROR(builder->SetOutputObjectDef(object_index,
                                                  GetObjectDef(tensor_index)));
    }

    return builder->Build(&runner_);
  }

  Status SetInputsAndOutputs(TfLiteContext* context) {
    int i = 0;
    for (auto index : input_indices_) {
      RETURN_IF_ERROR(
          runner_->SetInputObject(i++, GetTensorObject(index, context)));
    }
    i = 0;
    for (auto index : output_indices_) {
      RETURN_IF_ERROR(
          runner_->SetOutputObject(i++, GetTensorObject(index, context)));
    }
    return OkStatus();
  }

  Status Invoke(TfLiteContext* context) {
    RETURN_IF_ERROR(SetInputsAndOutputs(context));
    return runner_->Run();
  }

  ObjectDef GetObjectDef(int index) const {
    ObjectDef default_object_def;
    default_object_def.data_type = DataType::FLOAT32;
    default_object_def.data_layout = DataLayout::BHWC;
    default_object_def.object_type = ObjectType::CPU_MEMORY;
    default_object_def.user_provided = true;
    return default_object_def;
  }

  TensorObject GetTensorObject(int index, TfLiteContext* context) const {
    auto& tensor = context->tensors[index];
    return MakeCpuMemory(absl::MakeSpan(tensor.data.raw, tensor.bytes));
  }

  TfLiteDelegate* tflite_delegate() { return &delegate_; }

 private:
  Status InitializeOpenClApi(GraphFloat32* graph,
                             std::unique_ptr<InferenceBuilder>* builder) {
    cl::InferenceEnvironmentOptions env_options;
    cl::InferenceEnvironmentProperties properties;
    RETURN_IF_ERROR(cl::NewInferenceEnvironment(env_options, &cl_environment_,
                                                &properties));
    cl::InferenceOptions options;
    options.allow_precision_loss = options_.is_precision_loss_allowed != 0;
    RETURN_IF_ERROR(cl_environment_->NewInferenceBuilder(
        options, std::move(*graph), builder));
    TFLITE_LOG_PROD_ONCE(tflite::TFLITE_LOG_INFO,
                         "Initialized OpenCL-based API.");
    return OkStatus();
  }

  Status InitializeOpenGlApi(GraphFloat32* graph,
                             std::unique_ptr<InferenceBuilder>* builder) {
    gl::InferenceEnvironmentOptions env_options;
    gl::InferenceEnvironmentProperties properties;
    RETURN_IF_ERROR(
        NewInferenceEnvironment(env_options, &gl_environment_, &properties));
    gl::InferenceOptions options;
    options.allow_precision_loss = options_.is_precision_loss_allowed != 0;
    options.fuse_operations =
        options_.inference_preference !=
        TFLITE_GPU_INFERENCE_PREFERENCE_FAST_SINGLE_ANSWER;
    options.inline_parameters =
        options_.inference_preference !=
        TFLITE_GPU_INFERENCE_PREFERENCE_FAST_SINGLE_ANSWER;
    RETURN_IF_ERROR(gl_environment_->NewInferenceBuilder(std::move(*graph),
                                                         options, builder));
    TFLITE_LOG_PROD_ONCE(tflite::TFLITE_LOG_INFO,
                         "Initialized OpenGL-based API.");
    return OkStatus();
  }

  TfLiteDelegate delegate_ = {
      reinterpret_cast<void*>(this),  // .data_
      DelegatePrepare,                // .Prepare
      nullptr,                        // .CopyFromBufferHandle
      nullptr,                        // .CopyToBufferHandle
      nullptr,                        // .FreeBufferHandle
      kTfLiteDelegateFlagsNone,       // .flags
  };

  TfLiteGpuDelegateOptionsV2 options_;
  std::unique_ptr<cl::InferenceEnvironment> cl_environment_;
  std::unique_ptr<gl::InferenceEnvironment> gl_environment_;
  std::unique_ptr<InferenceRunner> runner_;
  std::vector<int64_t> input_indices_;
  std::vector<int64_t> output_indices_;
};

inline Delegate* GetDelegate(TfLiteNode* node) {
  return reinterpret_cast<Delegate*>(node->user_data);
}

inline Delegate* GetDelegate(TfLiteDelegate* delegate) {
  return reinterpret_cast<Delegate*>(delegate->data_);
}

TfLiteStatus DelegatePrepare(TfLiteContext* context, TfLiteDelegate* delegate) {
  const TfLiteRegistration kRegistration = {
      // .init
      [](TfLiteContext* context, const char* buffer, size_t) -> void* {
        const auto* params =
            reinterpret_cast<const TfLiteDelegateParams*>(buffer);
        auto* gpu_delegate = GetDelegate(params->delegate);
        // Everything below should happen in prepare function call, but TFLite
        // for whatever reason forbids that.
        const auto status = gpu_delegate->Prepare(context, params);
        if (!status.ok()) {
          context->ReportError(context, "TfLiteGpuDelegate Init: %s",
                               status.error_message().c_str());
          return nullptr;
        }
        return gpu_delegate;
      },
      // .free
      [](TfLiteContext*, void* buffer) -> void {},
      // .prepare
      [](TfLiteContext* context, TfLiteNode* node) -> TfLiteStatus {
        if (!node->user_data) {
          context->ReportError(
              context,
              "TfLiteGpuDelegate Prepare: delegate is not initialized");
          return kTfLiteError;
        }
        // TODO(akulik): tflite tensors are not allocated here either. It would
        // be good to set inputs and outputs only once here instead of setting
        // them every time in .invoke.
        return kTfLiteOk;
      },
      // .invoke
      [](TfLiteContext* context, TfLiteNode* node) -> TfLiteStatus {
        const auto status = GetDelegate(node)->Invoke(context);
        if (!status.ok()) {
          context->ReportError(context, "TfLiteGpuDelegate Invoke: %s",
                               status.error_message().c_str());
          return kTfLiteError;
        }
        return kTfLiteOk;
      },
      nullptr,                // .profiling_string
      0,                      // .builtin_code
      "TfLiteGpuDelegateV2",  // .custom_name
      1,                      // .version
  };
  TfLiteIntArray* ops_to_replace = GetOpsToReplace(context);
  const auto status = context->ReplaceNodeSubsetsWithDelegateKernels(
      context, kRegistration, ops_to_replace, delegate);
  TfLiteIntArrayFree(ops_to_replace);
  return status;
}

}  // namespace
}  // namespace gpu
}  // namespace tflite

TfLiteGpuDelegateOptionsV2 TfLiteGpuDelegateOptionsV2Default() {
  TfLiteGpuDelegateOptionsV2 options;
  options.is_precision_loss_allowed = 0;
  options.inference_preference =
      TFLITE_GPU_INFERENCE_PREFERENCE_FAST_SINGLE_ANSWER;
  return options;
}

TfLiteDelegate* TfLiteGpuDelegateV2Create(
    const TfLiteGpuDelegateOptionsV2* options) {
  auto* gpu_delegate = new tflite::gpu::Delegate(options);
  TFLITE_LOG_PROD_ONCE(tflite::TFLITE_LOG_INFO,
                       "Created TensorFlow Lite delegate for GPU.");
  return gpu_delegate ? gpu_delegate->tflite_delegate() : nullptr;
}

void TfLiteGpuDelegateV2Delete(TfLiteDelegate* delegate) {
  delete tflite::gpu::GetDelegate(delegate);
}
