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

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/c/c_api_internal.h"
#include "tensorflow/lite/experimental/micro/kernels/all_ops_resolver.h"
#include "tensorflow/lite/experimental/micro/micro_utils.h"
#include "tensorflow/lite/experimental/micro/testing/micro_test.h"
#include "tensorflow/lite/experimental/micro/testing/test_utils.h"

namespace tflite {
namespace testing {
namespace {

// Common inputs and outputs.
static const int kInputElements = 16;
static const int kInputShape[] = {4, 2, 2, 4, 1};
static const float kInputData[] = {1, 1, 1, 1, 2, 2, 2, 2,
                                   1, 2, 3, 4, 1, 2, 3, 4};
static const int kFilterElements = 12;
static const int kFilterShape[] = {4, 3, 2, 2, 1};
static const float kFilterData[] = {1, 2, 3, 4, -1, 1, -1, 1, -1, -1, 1, 1};
static const int kBiasElements = 3;
static const int kBiasShape[] = {1, 3};
static const float kBiasData[] = {1, 2, 3};
static const int kOutputElements = 12;
static const int kOutputShape[] = {4, 2, 1, 2, 3};
static const float kGoldenData[] = {18, 2, 5, 18, 2, 5, 17, 4, 3, 37, 4, 3};

template <typename T>
void ValidateConvGoldens(TfLiteTensor* tensors, int tensors_size,
                         const T* expected_output_data, T* output_data,
                         int output_length, TfLiteFusedActivation activation,
                         float tolerance = 1e-5) {
  TfLiteContext context;
  PopulateContext(tensors, tensors_size, &context);

  ::tflite::ops::micro::AllOpsResolver resolver;

  const TfLiteRegistration* registration =
      resolver.FindOp(tflite::BuiltinOperator_CONV_2D, 1);

  TF_LITE_MICRO_EXPECT_NE(nullptr, registration);

  TfLiteConvParams builtin_data = {
      kTfLitePaddingValid,  // padding
      2,                    // stride_width
      2,                    // stride_height
      1,                    // dilation_width_factor
      1,                    // dilation_height_factor
      activation,           // activation
  };

  const char* init_data = reinterpret_cast<const char*>(&builtin_data);
  size_t init_data_size = 0;
  void* user_data = nullptr;

  if (registration->init) {
    user_data = registration->init(&context, init_data, init_data_size);
  }

  int inputs_array_data[] = {3, 0, 1, 2};
  TfLiteIntArray* inputs_array = IntArrayFromInts(inputs_array_data);
  int outputs_array_data[] = {1, 3};
  TfLiteIntArray* outputs_array = IntArrayFromInts(outputs_array_data);
  int temporaries_array_data[] = {0};
  TfLiteIntArray* temporaries_array = IntArrayFromInts(temporaries_array_data);

  TfLiteNode node;
  node.inputs = inputs_array;
  node.outputs = outputs_array;
  node.temporaries = temporaries_array;
  node.user_data = user_data;
  node.builtin_data = reinterpret_cast<void*>(&builtin_data);
  node.custom_initial_data = nullptr;
  node.custom_initial_data_size = 0;
  node.delegate = nullptr;

  if (registration->prepare) {
    TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, registration->prepare(&context, &node));
  }
  TF_LITE_MICRO_EXPECT_NE(nullptr, registration->invoke);
  TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, registration->invoke(&context, &node));

  if (registration->free) {
    registration->free(&context, user_data);
  }

  for (int i = 0; i < output_length; ++i) {
    TF_LITE_MICRO_EXPECT_EQ(expected_output_data[i], output_data[i]);
  }
}

void TestConvFloat(const int* input_dims_data, const float* input_data,
                   const int* filter_dims_data, const float* filter_data,
                   const int* bias_dims_data, const float* bias_data,
                   const int* output_dims_data,
                   const float* expected_output_data, float* output_data,
                   TfLiteFusedActivation activation) {
  TfLiteIntArray* input_dims = IntArrayFromInts(input_dims_data);
  TfLiteIntArray* filter_dims = IntArrayFromInts(filter_dims_data);
  TfLiteIntArray* bias_dims = IntArrayFromInts(bias_dims_data);
  TfLiteIntArray* output_dims = IntArrayFromInts(output_dims_data);
  const int output_dims_count = ElementCount(*output_dims);
  constexpr int inputs_size = 3;
  constexpr int outputs_size = 1;
  constexpr int tensors_size = inputs_size + outputs_size;
  TfLiteTensor tensors[tensors_size] = {
      CreateFloatTensor(input_data, input_dims, "input_tensor"),
      CreateFloatTensor(filter_data, filter_dims, "filter_tensor"),
      CreateFloatTensor(bias_data, bias_dims, "bias_tensor"),
      CreateFloatTensor(output_data, output_dims, "output_tensor"),
  };

  ValidateConvGoldens(tensors, tensors_size, expected_output_data, output_data,
                      output_dims_count, activation);
}

void TestConvQuantizedPerLayer(
    const int* input_dims_data, const float* input_data,
    uint8_t* input_quantized, float input_scale, const int* filter_dims_data,
    const float* filter_data, uint8_t* filter_quantized, float filter_scale,
    const int* bias_dims_data, const float* bias_data, int32_t* bias_quantized,
    const int* output_dims_data, const float* expected_output_data,
    uint8_t* expected_output_quantized, uint8_t* output_data,
    float output_scale, TfLiteFusedActivation activation) {
  TfLiteIntArray* input_dims = IntArrayFromInts(input_dims_data);
  TfLiteIntArray* filter_dims = IntArrayFromInts(filter_dims_data);
  TfLiteIntArray* bias_dims = IntArrayFromInts(bias_dims_data);
  TfLiteIntArray* output_dims = IntArrayFromInts(output_dims_data);
  const int output_dims_count = ElementCount(*output_dims);

  tflite::AsymmetricQuantize(expected_output_data, expected_output_quantized,
                             output_dims_count, output_scale, 128);

  constexpr int inputs_size = 3;
  constexpr int outputs_size = 1;
  constexpr int tensors_size = inputs_size + outputs_size;
  TfLiteTensor tensors[tensors_size] = {
      CreateQuantizedTensor(input_data, input_quantized, input_dims,
                            input_scale, 128, "input_tensor"),
      CreateQuantizedTensor(filter_data, filter_quantized, filter_dims,
                            filter_scale, 128, "filter_tensor"),
      CreateQuantizedBiasTensor(bias_data, bias_quantized, bias_dims,
                                input_scale, filter_scale, "bias_tensor"),
      CreateQuantizedTensor(output_data, output_dims, output_scale, 128,
                            "output_tensor")};

  // TODO(njeff): Affine Quantization Params should be set on tensor creation.
  float filter_scales[] = {1, filter_scale};
  int filter_zero_points[] = {1, 128};
  TfLiteAffineQuantization filter_quant = {
      FloatArrayFromFloats(filter_scales),
      IntArrayFromInts(filter_zero_points)};
  tensors[1].quantization = {kTfLiteAffineQuantization, &filter_quant};

  ValidateConvGoldens(tensors, tensors_size, expected_output_quantized,
                      output_data, output_dims_count, activation);
}

void TestConvQuantizedPerChannel(
    const int* input_dims_data, const float* input_data,
    int8_t* input_quantized, float input_scale, const int* filter_dims_data,
    const float* filter_data, int8_t* filter_data_quantized,
    const int* bias_dims_data, const float* bias_data,
    int32_t* bias_data_quantized, float* bias_scales, int* bias_zero_points,
    const int* output_dims_data, const float* expected_output_data,
    int8_t* expected_output_data_quantized, int8_t* output_data,
    float output_scale, TfLiteFusedActivation activation) {
  TfLiteIntArray* input_dims = IntArrayFromInts(input_dims_data);
  TfLiteIntArray* filter_dims = IntArrayFromInts(filter_dims_data);
  TfLiteIntArray* bias_dims = IntArrayFromInts(bias_dims_data);
  TfLiteIntArray* output_dims = IntArrayFromInts(output_dims_data);
  const int output_dims_count = ElementCount(*output_dims);

  int filter_zero_points[5];
  float filter_scales[5];
  TfLiteAffineQuantization filter_quant;
  TfLiteAffineQuantization bias_quant;
  TfLiteTensor input_tensor = CreateQuantizedTensor(
      input_data, input_quantized, input_dims, input_scale, 0, "input_tensor");
  TfLiteTensor filter_tensor = CreateSymmetricPerChannelQuantizedTensor(
      filter_data, filter_data_quantized, filter_dims, filter_scales,
      filter_zero_points, &filter_quant, 0 /* quantized dimension */,
      "filter_tensor");
  TfLiteTensor bias_tensor = CreatePerChannelQuantizedBiasTensor(
      bias_data, bias_data_quantized, bias_dims, input_scale, &filter_scales[1],
      bias_scales, bias_zero_points, &bias_quant, 0, "bias_tensor");
  TfLiteTensor output_tensor =
      CreateQuantizedTensor(output_data, output_dims, output_scale,
                            0 /* quantized dimension */, "output_tensor");

  // TODO(njeff): Affine Quantization Params should be set on tensor creation.
  float input_scales[] = {1, input_scale};
  int input_zero_points[] = {1, 128};
  TfLiteAffineQuantization input_quant = {FloatArrayFromFloats(input_scales),
                                          IntArrayFromInts(input_zero_points)};
  input_tensor.quantization = {kTfLiteAffineQuantization, &input_quant};

  float output_scales[] = {1, output_scale};
  int output_zero_points[] = {1, 128};
  TfLiteAffineQuantization output_quant = {
      FloatArrayFromFloats(output_scales),
      IntArrayFromInts(output_zero_points)};
  output_tensor.quantization = {kTfLiteAffineQuantization, &output_quant};

  constexpr int inputs_size = 3;
  constexpr int outputs_size = 1;
  constexpr int tensors_size = inputs_size + outputs_size;
  TfLiteTensor tensors[tensors_size] = {
      input_tensor,
      filter_tensor,
      bias_tensor,
      output_tensor,
  };

  tflite::AsymmetricQuantize(expected_output_data,
                             expected_output_data_quantized, output_dims_count,
                             output_scale);
  ValidateConvGoldens(tensors, tensors_size, expected_output_data_quantized,
                      output_data, output_dims_count, activation);
}

}  // namespace
}  // namespace testing
}  // namespace tflite

TF_LITE_MICRO_TESTS_BEGIN

TF_LITE_MICRO_TEST(SimpleTestFloat) {
  float output_data[tflite::testing::kOutputElements];

  tflite::testing::TestConvFloat(
      tflite::testing::kInputShape, tflite::testing::kInputData,
      tflite::testing::kFilterShape, tflite::testing::kFilterData,
      tflite::testing::kBiasShape, tflite::testing::kBiasData,
      tflite::testing::kOutputShape, tflite::testing::kGoldenData, output_data,
      kTfLiteActNone);
}

TF_LITE_MICRO_TEST(InputAndFilterSameWidthHeight) {
  const int output_dims_count = 2;
  float output_data[output_dims_count];

  const int kFilterShape[] = {4, 1, 2, 4, 1};
  const float filter_values[] = {1, 2, 3, 4, -1, -1, 1, 1};
  const int kBiasShape[] = {1, 1};
  const float bias_values[] = {0};
  const int kOutputShape[] = {4, 2, 1, 1, 1};
  const float expected_output[] = {10, 34};

  tflite::testing::TestConvFloat(
      tflite::testing::kInputShape, tflite::testing::kInputData, kFilterShape,
      filter_values, kBiasShape, bias_values, kOutputShape, expected_output,
      output_data, kTfLiteActNone);
}

TF_LITE_MICRO_TEST(SimpleTestQuantized) {
  const int output_dims_count = 12;
  uint8_t output_data[output_dims_count];

  const float input_scale = 0.5f;
  const float filter_scale = 0.5f;
  const float output_scale = 1.0f;

  uint8_t input_quantized[tflite::testing::kInputElements];
  uint8_t filter_quantized[tflite::testing::kFilterElements];
  int32_t bias_quantized[tflite::testing::kBiasElements];
  uint8_t golden_quantized[tflite::testing::kOutputElements];

  tflite::testing::TestConvQuantizedPerLayer(
      tflite::testing::kInputShape, tflite::testing::kInputData,
      input_quantized, input_scale, tflite::testing::kFilterShape,
      tflite::testing::kFilterData, filter_quantized, filter_scale,
      tflite::testing::kBiasShape, tflite::testing::kBiasData, bias_quantized,
      tflite::testing::kOutputShape, tflite::testing::kGoldenData,
      golden_quantized, output_data, output_scale, kTfLiteActNone);
}

TF_LITE_MICRO_TEST(SimpleTestQuantizedPerChannel) {
  const int output_dims_count = 12;
  int8_t output_data[output_dims_count];

  const float input_scale = 0.5f;
  const float bias_scale = 0.5f;
  const float output_scale = 1.0f;

  int8_t input_quantized[tflite::testing::kInputElements];
  int8_t filter_quantized[tflite::testing::kFilterElements];
  int32_t bias_quantized[tflite::testing::kBiasElements];
  int8_t golden_quantized[tflite::testing::kOutputElements];
  int zero_points[tflite::testing::kBiasElements + 1];
  float scales[tflite::testing::kBiasElements + 1];

  tflite::testing::TestConvQuantizedPerChannel(
      tflite::testing::kInputShape, tflite::testing::kInputData,
      input_quantized, input_scale, tflite::testing::kFilterShape,
      tflite::testing::kFilterData, filter_quantized,
      tflite::testing::kBiasShape, tflite::testing::kBiasData, bias_quantized,
      scales, zero_points, tflite::testing::kOutputShape,
      tflite::testing::kGoldenData, golden_quantized, output_data, output_scale,
      kTfLiteActNone);
}

TF_LITE_MICRO_TESTS_END
