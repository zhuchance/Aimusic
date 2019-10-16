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

#ifndef TENSORFLOW_LITE_DELEGATES_GPU_DELEGATE_H_
#define TENSORFLOW_LITE_DELEGATES_GPU_DELEGATE_H_

#include <stdint.h>

#include "tensorflow/lite/c/c_api_internal.h"

#ifdef SWIG
#define TFL_CAPI_EXPORT
#else
#if defined(_WIN32)
#ifdef TFL_COMPILE_LIBRARY
#define TFL_CAPI_EXPORT __declspec(dllexport)
#else
#define TFL_CAPI_EXPORT __declspec(dllimport)
#endif  // TFL_COMPILE_LIBRARY
#else
#define TFL_CAPI_EXPORT __attribute__((visibility("default")))
#endif  // _WIN32
#endif  // SWIG

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Encapsulated precision/compilation/runtime tradeoffs.
enum TfLiteGpuInferencePreference {
  // Delegate will be used only once, therefore, bootstrap/init time should
  // be taken into account.
  TFLITE_GPU_INFERENCE_PREFERENCE_FAST_SINGLE_ANSWER = 0,

  // Prefer maximizing the throughput. Same delegate will be used repeatedly on
  // multiple inputs.
  TFLITE_GPU_INFERENCE_PREFERENCE_SUSTAINED_SPEED = 1,
};

// IMPORTANT: Always use TfLiteGpuDelegateOptionsV2Default() method to create
// new instance of TfLiteGpuDelegateOptionsV2, otherwise every new added option
// may break inference.
typedef struct {
  // When set to zero, computations are carried out in maximal possible
  // precision. Otherwise, the GPU may quantify tensors, downcast values,
  // process in FP16 to increase performance. For most models precision loss is
  // warranted.
  int32_t is_precision_loss_allowed;

  // Preference is defined in TfLiteGpuInferencePreference.
  int32_t inference_preference;
} TfLiteGpuDelegateOptionsV2;

// Populates TfLiteGpuDelegateOptionsV2 as follows:
//   is_precision_loss_allowed = false
//   inference_preference = TFLITE_GPU_INFERENCE_PREFERENCE_FAST_SINGLE_ANSWER
TFL_CAPI_EXPORT TfLiteGpuDelegateOptionsV2 TfLiteGpuDelegateOptionsV2Default();

// Creates a new delegate instance that need to be destroyed with
// TfLiteGpuDelegateV2Delete when delegate is no longer used by TFLite.
//
// This delegate encapsulates multiple GPU-acceleration APIs under the hood to
// make use of the fastest available on a device.
//
// When `options` is set to `nullptr`, then default options are used.
TFL_CAPI_EXPORT TfLiteDelegate* TfLiteGpuDelegateV2Create(
    const TfLiteGpuDelegateOptionsV2* options);

// Destroys a delegate created with `TfLiteGpuDelegateV2Create` call.
TFL_CAPI_EXPORT void TfLiteGpuDelegateV2Delete(TfLiteDelegate* delegate);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // TENSORFLOW_LITE_DELEGATES_GPU_DELEGATE_H_
