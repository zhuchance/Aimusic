// RUN: tf-opt %s -split-input-file -verify-diagnostics | FileCheck %s --dump-input=fail

// Tests for TensorFlow ops with custom verifiers.

// TODO(hinsu): Remove tests for ops without custom verifiers. These tests were
// added along with manual op definition and are obsolete now that the op
// definitions are auto-generated.
// TODO(hinsu): Move attribute and type tests to types.mlir file.

//===--------------------------------------------------------------------===//
//  Test TF opaque attributes
//===--------------------------------------------------------------------===//

// CHECK-LABEL: func @opaquetensorattr
func @opaquetensorattr() -> () {
^bb0:
// CHECK: "opaqueIntTensor"() {bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4xi32>} : () -> ()
  "opaqueIntTensor"(){bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4xi32>} : () -> ()
// CHECK: "opaqueFloatTensor"() {bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4xf32>} : () -> ()
  "opaqueFloatTensor"(){bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4xf32>} : () -> ()
// CHECK: "opaqueStringTensor"() {bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4x!tf.string>} : () -> ()
  "opaqueStringTensor"(){bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4x!tf.string>} : () -> ()
// CHECK: "opaqueResourceTensor"() {bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4x!tf.resource>} : () -> ()
  "opaqueResourceTensor"(){bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4x!tf.resource>} : () -> ()
  return
}

//===--------------------------------------------------------------------===//
//  Test raw TF operations (_tf.*)
//===--------------------------------------------------------------------===//

// Test of parsing !tf.resource type
// CHECK-LABEL: func @testTFResource(%arg0: !tf.resource)
func @testTFResource(!tf.resource) -> () {
^bb0(%arg0: !tf.resource):
  // CHECK: %0:2 = "_tf.Const"() {device = "", dtype = "tfdtype$DT_Resource", name = "Const"} : () -> (tensor<*x!tf.resource>, !_tf.control)
  %0:2 = "_tf.Const"() {device = "", name = "Const", dtype = "tfdtype$DT_Resource"} : () -> (tensor<*x!tf.resource>, !_tf.control)
  // CHECK: %1 = "_tf.AssignAddVariableOp"(%arg0, %0#0) {device = "", name = "AssignAddVariableOp"} : (!tf.resource, tensor<*x!tf.resource>) -> !_tf.control
  %1 = "_tf.AssignAddVariableOp"(%arg0, %0#0) {device = "", name = "AssignAddVariableOp"} : (!tf.resource, tensor<*x!tf.resource>) -> !_tf.control
  return
}

// Test of parsing !tf.variant type
// CHECK-LABEL: func @testTFVariant(%arg0: tensor<*x!tf.variant>)
func @testTFVariant(tensor<*x!tf.variant>) -> () {
^bb0(%arg0: tensor<*x!tf.variant>):
  // CHECK: %0:2 = "_tf.Const"() {device = "", dtype = "tfdtype$DT_VARIANT", name = "Const"} : () -> (!tf.variant, !_tf.control)
  %0:2 = "_tf.Const"() {device = "", name = "Const", dtype = "tfdtype$DT_VARIANT"} : () -> (!tf.variant, !_tf.control)
  // CHECK: %1 = "_tf.AssignAddVariableOp"(%arg0, %0#0) {device = "", name = "AssignAddVariableOp"} : (tensor<*x!tf.variant>, !tf.variant) -> !_tf.control
  %1 = "_tf.AssignAddVariableOp"(%arg0, %0#0) {device = "", name = "AssignAddVariableOp"} : (tensor<*x!tf.variant>, !tf.variant) -> !_tf.control
  return
}

// Test of parsing tf_complex type
// CHECK-LABEL: func @testTFComplex(%arg0: tensor<*x!tf.complex64>, %arg1: tensor<*x!tf.complex128>)
func @testTFComplex(tensor<*x!tf.complex64>, tensor<*x!tf.complex128>) -> (!tf.complex64, !tf.complex128) {
^bb0(%arg0: tensor<*x!tf.complex64>, %arg1: tensor<*x!tf.complex128>):
  // CHECK: %0:2 = "_tf.Const"() {device = "", dtype = "tfdtype$DT_COMPLEX64", name = "Const"} : () -> (!tf.complex64, !_tf.control)
  %0:2 = "_tf.Const"() {device = "", name = "Const", dtype = "tfdtype$DT_COMPLEX64"} : () -> (!tf.complex64, !_tf.control)
  // CHECK: %1:2 = "_tf.Const"() {device = "", dtype = "tfdtype$DT_COMPLEX128", name = "Const"} : () -> (!tf.complex128, !_tf.control)
  %1:2 = "_tf.Const"() {device = "", name = "Const", dtype = "tfdtype$DT_COMPLEX128"} : () -> (!tf.complex128, !_tf.control)
  // CHECK: %2:2 = "_tf.AssignAddVariableOp"(%arg0, %0#0) {device = "", name = "AssignAddVariableOp"} : (tensor<*x!tf.complex64>, !tf.complex64) -> (!tf.complex64, !_tf.control)
  %2:2 = "_tf.AssignAddVariableOp"(%arg0, %0#0) {device = "", name = "AssignAddVariableOp"} : (tensor<*x!tf.complex64>, !tf.complex64) -> (!tf.complex64, !_tf.control)
  // CHECK: %3:2 = "_tf.AssignAddVariableOp"(%arg1, %1#0) {device = "", name = "AssignAddVariableOp"} : (tensor<*x!tf.complex128>, !tf.complex128) -> (!tf.complex128, !_tf.control)
  %3:2 = "_tf.AssignAddVariableOp"(%arg1, %1#0) {device = "", name = "AssignAddVariableOp"} : (tensor<*x!tf.complex128>, !tf.complex128) -> (!tf.complex128, !_tf.control)
  return %2#0, %3#0 : !tf.complex64, !tf.complex128
}

//===--------------------------------------------------------------------===//
//  Test TF operations (tf.*)
//===--------------------------------------------------------------------===//

// -----

// CHECK-LABEL: func @testIdentity
func @testIdentity(%arg0: tensor<4x2x!tf.stringref>) -> tensor<4x2x!tf.string> {
  %0 = "tf.Identity"(%arg0) : (tensor<4x2x!tf.stringref>) -> tensor<4x2x!tf.string>
  return %0 : tensor<4x2x!tf.string>
}

// -----

// CHECK-LABEL: func @testBitcast
func @testBitcast(%arg0: tensor<3x4x!tf.uint16>) -> tensor<3x4x!tf.quint16> {
  %0 = "tf.Bitcast"(%arg0) : (tensor<3x4x!tf.uint16>) -> tensor<3x4x!tf.quint16>
  return %0 : tensor<3x4x!tf.quint16>
}

// -----

// CHECK-LABEL: func @testReverseV2
func @testReverseV2(%arg0: tensor<2x4x3x!tf.uint8>, %arg1: tensor<1xi32>) -> tensor<2x4x3x!tf.uint8> {
  %0 = "tf.ReverseV2"(%arg0, %arg1) : (tensor<2x4x3x!tf.uint8>, tensor<1xi32>) -> tensor<2x4x3x!tf.uint8>
  return %0 :  tensor<2x4x3x!tf.uint8>
}

// -----

func @testIdentityWrongType(%arg0: tensor<4x2x!tf.string>) -> tensor<4x2x!tf.stringref> {
  // expected-error @+1 {{requires all operands to be either same as or ref type of results}}
  %0 = "tf.Identity"(%arg0) : (tensor<4x2x!tf.string>) -> tensor<4x2x!tf.stringref>
  return %0 : tensor<4x2x!tf.stringref>
}

// -----

// TODO(hinsu): Move this to MLIR core once the test dialect have a custom type.

// Check that broadcastable trait accepts TF specific element type
// CHECK-LABEL: func @testAdd
func @testAdd(%arg0: tensor<4x2x!tf.string>, %arg1: tensor<2x!tf.string>) -> tensor<4x2x!tf.string> {
  %0 = "tf.Add"(%arg0, %arg1) : (tensor<4x2x!tf.string>, tensor<2x!tf.string>) -> tensor<4x2x!tf.string>
  return %0 : tensor<4x2x!tf.string>
}

// -----

// Valid BiasAdd operation.
func @testBiasAdd(%arg0: tensor<2x3x5x7xf32>, %arg1: tensor<7xf32>) -> tensor<2x3x5x7xf32> {
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NHWC"} : (tensor<2x3x5x7xf32>, tensor<7xf32>) -> tensor<2x3x5x7xf32>
  return %0 : tensor<2x3x5x7xf32>
}

// -----

func @testBiasAddNoDataFormatOk(tensor<1x32x32x16xf32>, tensor<16xf32>) -> tensor<1x32x32x16xf32> {
^bb0(%arg0: tensor<1x32x32x16xf32>, %arg1: tensor<16xf32>):
  %0 = "tf.BiasAdd"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT"}: (tensor<1x32x32x16xf32>, tensor<16xf32>) -> tensor<1x32x32x16xf32>
  return %0 : tensor<1x32x32x16xf32>
}

// -----

func @testBiasAddWrongDataFormat(tensor<1x32x32x16xf32>, tensor<16xf32>) -> tensor<1x32x32x16xf32> {
^bb0(%arg0: tensor<1x32x32x16xf32>, %arg1: tensor<16xf32>):
  // expected-error @+1 {{attribute 'data_format' failed to satisfy constraint: 'NHWC' or 'NCHW' convnet data format}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "HWCN"} : (tensor<1x32x32x16xf32>, tensor<16xf32>) -> tensor<1x32x32x16xf32>
  return %0 : tensor<1x32x32x16xf32>
}

// -----

func @testBiasAdd(%arg0: tensor<3xf32>, %arg1: tensor<3xf32>) -> tensor<3xf32> {
  // expected-error @+1 {{requires value operand to have rank at least two with `NHWC` data format}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NHWC"} : (tensor<3xf32>, tensor<3xf32>) -> tensor<3xf32>
  return %0 : tensor<3xf32>
}

// -----

func @testBiasAdd(%arg0: tensor<2x3xf32>, %arg1: tensor<3xf32>) -> tensor<2x3xf32> {
  // expected-error @+1 {{requires value operand to have rank at least three with `NCHW` data format}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NCHW"} : (tensor<2x3xf32>, tensor<3xf32>) -> tensor<2x3xf32>
  return %0 : tensor<2x3xf32>
}

// -----

func @testBiasAdd(%arg0: tensor<2x3x5x7xf32>, %arg1: tensor<5x7xf32>) -> tensor<2x3x5x7xf32> {
  // expected-error @+1 {{requires bias operand to have rank exactly one}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NHWC"} : (tensor<2x3x5x7xf32>, tensor<5x7xf32>) -> tensor<2x3x5x7xf32>
  return %0 : tensor<2x3x5x7xf32>
}

// -----

func @testBiasAdd(%arg0: tensor<2x3x5x7xf32>, %arg1: tensor<5xf32>) -> tensor<2x3x5x7xf32> {
  // expected-error @+1 {{requires channel dimension and feature dimension to match; found 7 and 5, respectively}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NHWC"} : (tensor<2x3x5x7xf32>, tensor<5xf32>) -> tensor<2x3x5x7xf32>
  return %0 : tensor<2x3x5x7xf32>
}

// -----

// Test valid tf.BroadcastTo
// CHECK-LABEL: func @testBroadcastTo(%arg0: tensor<16xf32>)
func @testBroadcastTo(%arg0: tensor<16xf32>) -> tensor<16x16x16x16xf32> {
  %cst = constant dense<16> : tensor<4xi32>
  %0 = "tf.BroadcastTo"(%arg0, %cst) : (tensor<16xf32>, tensor<4xi32>) -> tensor<16x16x16x16xf32>
  return %0 : tensor<16x16x16x16xf32>
}

// -----

// Test valid tf.LeakyRelu
// CHECK-LABEL: func @testLeakyRelu(%arg0: tensor<16xf32>)
func @testLeakyRelu(tensor<16xf32>) -> tensor<16xf32> {
^bb0(%arg0: tensor<16xf32>):
  %0 = "tf.LeakyRelu"(%arg0) {alpha = 0.2 : f32} : (tensor<16xf32>) -> tensor<16xf32>
  return %0 : tensor<16xf32>
}

// -----
func @testLeakyWrongAlphaType(tensor<16xf32>) -> tensor<16xf32> {
^bb0(%arg0: tensor<16xf32>):
  // expected-error @+1 {{attribute 'alpha' failed to satisfy constraint: 32-bit float}}
  %0 = "tf.LeakyRelu"(%arg0) {alpha = 1: i32}: (tensor<16xf32>) -> tensor<16xf32>
  return %0 : tensor<16xf32>
}

// -----

// CHECK-LABEL: func @testMul
func @testMul(%arg0: tensor<2x!tf.uint16>) -> (tensor<2x!tf.uint16>) {
  %0 = "tf.Mul"(%arg0, %arg0) {T = "tfdtype$DT_UINT16", device = "/device:CPU:0", name = "Mul"} : (tensor<2x!tf.uint16>, tensor<2x!tf.uint16>) -> tensor<2x!tf.uint16>
  return %0 : tensor<2x!tf.uint16>
}

// -----

// CHECK-LABEL: func @testReshape(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<10000xf32>, %arg3: tensor<*xi32>)
func @testReshape(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<10000xf32>, %arg3: tensor<*xi32>) -> (tensor<100x100xf32>, tensor<*xf32>, tensor<10000xf32>, tensor<100x100xf32>, tensor<*xf32>, tensor<*xf32>) {
  %shape1 = constant dense<100> : tensor<2xi32>
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<*xf32>, tensor<2xi32>) -> (tensor<100x100xf32>)
  %shape2 = "tf.Shape"(%arg0) {device = "", name = "Shape", T = "tfdtype$DT_FLOAT", out_type = "tfdtype$DT_INT32"} : (tensor<*xf32>) -> (tensor<?xi32>)
  %r2 = "tf.Reshape"(%arg1, %shape2) {device = "", name = "Reshape_1", T = "tfdtype$DT_FLOAT", Tshape = "tfdtype$DT_INT32"} : (tensor<*xf32>, tensor<?xi32>) -> (tensor<*xf32>)
  %r3 = "tf.Reshape"(%arg2, %shape1) {device = "", name = "Reshape_1", T = "tfdtype$DT_FLOAT", Tshape = "tfdtype$DT_INT32"} : (tensor<10000xf32>, tensor<2xi32>) -> (tensor<10000xf32>)
  %shape3 = constant dense<[-1, 100]> : tensor<2xi32>
  %r4 = "tf.Reshape"(%arg2, %shape3) {device = "", name = "Reshape_1", T = "tfdtype$DT_FLOAT", Tshape = "tfdtype$DT_INT32"} : (tensor<10000xf32>, tensor<2xi32>) -> (tensor<100x100xf32>)
  %r5 = "tf.Reshape"(%arg0, %arg3) {T = "tfdtype$DT_FLOAT", Tshape = "tfdtype$DT_INT32"} : (tensor<*xf32>, tensor<*xi32>) -> (tensor<*xf32>)
  %r6 = "tf.Reshape"(%arg2, %arg3) {T = "tfdtype$DT_FLOAT", Tshape = "tfdtype$DT_INT32"} : (tensor<10000xf32>, tensor<*xi32>) -> (tensor<*xf32>)
  return %r1, %r2, %r3, %r4, %r5, %r6: tensor<100x100xf32>, tensor<*xf32>, tensor<10000xf32>, tensor<100x100xf32>, tensor<*xf32>, tensor<*xf32>
}

// -----
// tf.Reshape with incorrect type.
func @testReshape(tensor<*xf32>, tensor<*xf32>) -> (tensor<100x100xf32>) {
^bb0(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>):
  %shape1 = constant dense<100.> : tensor<2xf32>
  // expected-error @+1 {{must be tensor of 32/64-bit integer values}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<*xf32>, tensor<2xf32>) -> (tensor<100x100xf32>)
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with incorrect element number.
func @testReshape(%arg0: tensor<10x10x10xf32>) -> tensor<100x100xf32> {
  %shape1 = constant dense<100> : tensor<2xi32>
  // expected-error @+1 {{mismatch in tensor elements and shape implied elements}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10xf32>, tensor<2xi32>) -> (tensor<100x100xf32>)
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with more than one -1 in the shape.
func @testReshape(%arg0: tensor<10x10x10xf32>) -> tensor<100x100xf32> {
  %shape1 = constant dense<-1> : tensor<2xi32>
  // expected-error @+1 {{more than one component of shape are -1}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10xf32>, tensor<2xi32>) -> (tensor<100x100xf32>)
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with -1 in the shape can't infer the dimension.
func @testReshape(%arg0: tensor<10x10x10xf32>) -> tensor<100x100xf32> {
  %shape1 = constant dense<[101, -1]> : tensor<2xi32>
  // expected-error @+1 {{one component of shape is -1 but couldn't infer the dimension}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10xf32>, tensor<2xi32>) -> (tensor<100x100xf32>)
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with a first operand that has non-static shape.
func @testReshape(%arg0: tensor<10x10x?xf32>) -> tensor<10x10xf32> {
  %shape1 = constant dense<[10, 10]> : tensor<2xi32>
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x?xf32>, tensor<2xi32>) -> (tensor<10x10xf32>)
  return %r1 : tensor<10x10xf32>
}

// -----

// CHECK-LABEL: func @testValidAvgPool
func @testValidAvgPool(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

// CHECK-LABEL: func @testAvgPoolMissingDataFormatOk
func @testAvgPoolMissingDataFormatOk(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongDataType(tensor<1x7x7x16xi32>) -> tensor<1x1x1x16xi32> {
^bb0(%arg0: tensor<1x7x7x16xi32>):
  // expected-error @+1 {{must be tensor of floating-point values}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_INT", data_format = "NHWC", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xi32>) -> tensor<1x1x1x16xi32>
  return %0 : tensor<1x1x1x16xi32>
}

// -----

func @testAvgPoolWrongDataFormat(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'data_format' failed to satisfy constraint: 'NHWC' or 'NCHW' convnet data format}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "HWCN", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolNoKsize(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{requires attribute 'ksize'}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongKsizeCount(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'ksize' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongKsizeType(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{'ksize' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7.5, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----
func @testAvgPoolWrongKsizeIntType(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{'ksize' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1 : i32, 7 : i32, 7 : i32, 1 : i32], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolNoPadding(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{requires attribute 'padding'}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongPadding(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'padding' failed to satisfy constraint: string attribute whose value is SAME, or VALID}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "MAGIC", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolNoStrides(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{requires attribute 'strides'}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "VALID"} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongStridesCount(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'strides' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongStridesType(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'strides' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "VALID", strides = ["1", "1", "1", "1"]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

// CHECK-LABEL: func @testValidConv2D
func @testValidConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

// CHECK-LABEL: func @testValidDynamicConv2D
func @testValidDynamicConv2D(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>) -> tensor<*xf32> {
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// CHECK-LABEL: func @testValidConv3D
func @testValidConv3D(%arg0: tensor<256x32x32x32x3xf32>, %arg1: tensor<3x3x3x3x16xf32>) -> tensor<256x30x30x30x16xf32> {
  %0 = "tf.Conv3D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1, 1]} : (tensor<256x32x32x32x3xf32>, tensor<3x3x3x3x16xf32>) -> tensor<256x30x30x30x16xf32>
  return %0 : tensor<256x30x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires operands to be 4D tensor}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv3D(%arg0: tensor<256x32x32x32x3xf32>, %arg1: tensor<3x3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires result to be 5D tensor}}
  %0 = "tf.Conv3D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1, 1]} : (tensor<256x32x32x32x3xf32>, tensor<3x3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x2x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires the number of input channels to be divisible by the number of filter input channels; found 3 and 2, respectively}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x2x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires attribute 'explicit_paddings'}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "EXPLICIT", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires explicit_paddings attribute length to be 8; actual length 4}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "EXPLICIT", strides = [1, 1, 1, 1], explicit_paddings = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires non negative explicit paddings}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "EXPLICIT", strides = [1, 1, 1, 1], explicit_paddings = [0, 0, 1, -1, 1, -1, 0, 0]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires strides attribute length to be 4}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires positive strides}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [0, 1, 1, 0]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires dilations attribute length to be 4}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1], dilations = [1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires positive dilations}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1], dilations = [1, 1, 0, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testMaxPoolGrad(%orig_input: tensor<f32>, %orig_output: tensor<10x12x12x64xf32>, %grad: tensor<10x12x12x64xf32>) -> tensor<10x24x24x64xf32> {
  // expected-error @+1 {{requires orig_input to be rank 4}}
  %result = "tf.MaxPoolGrad"(%orig_input, %orig_output, %grad) {
     data_format = "NHWC",
     ksize = [1, 2, 2, 1],
     padding = "VALID",
     strides = [1, 2, 2, 1]
  } : (tensor<f32>, tensor<10x12x12x64xf32>, tensor<10x12x12x64xf32>) -> tensor<10x24x24x64xf32>
  return %result : tensor<10x24x24x64xf32>
}

// -----

func @testMaxPoolGrad(%orig_input: tensor<10x24x24x64xf32>, %orig_output: tensor<12x12x64xf32>, %grad: tensor<10x12x12x64xf32>) -> tensor<10x24x24x64xf32> {
  // expected-error @+1 {{requires orig_output to be rank 4}}
  %result = "tf.MaxPoolGrad"(%orig_input, %orig_output, %grad) {
     data_format = "NHWC",
     ksize = [1, 2, 2, 1],
     padding = "VALID",
     strides = [1, 2, 2, 1]
  } : (tensor<10x24x24x64xf32>, tensor<12x12x64xf32>, tensor<10x12x12x64xf32>) -> tensor<10x24x24x64xf32>
  return %result : tensor<10x24x24x64xf32>
}

// -----

func @testMaxPoolGrad(%orig_input: tensor<10x24x24x64xf32>, %orig_output: tensor<10x12x12x64xf32>, %grad: tensor<12x12x64xf32>) -> tensor<10x24x24x64xf32> {
  // expected-error @+1 {{requires grad to be rank 4}}
  %result = "tf.MaxPoolGrad"(%orig_input, %orig_output, %grad) {
     data_format = "NHWC",
     ksize = [1, 2, 2, 1],
     padding = "VALID",
     strides = [1, 2, 2, 1]
  } : (tensor<10x24x24x64xf32>, tensor<10x12x12x64xf32>, tensor<12x12x64xf32>) -> tensor<10x24x24x64xf32>
  return %result : tensor<10x24x24x64xf32>
}

// -----

// CHECK-LABEL: func @testValidDepthwiseConv2dNative
func @testValidDepthwiseConv2dNative(tensor<256x32x32x3xf32>, tensor<3x3x3x4xf32>) -> tensor<256x30x30x12xf32> {
^bb0(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x4xf32>) :
  %0 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {device = "", name = "MobilenetV2/expanded_conv/depthwise/depthwise", T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x4xf32>) -> tensor<256x30x30x12xf32>
  return %0 : tensor<256x30x30x12xf32>
}

// -----

// Test valid tf.FakeQuantWithMinMaxArgs
// CHECK-LABEL: func @testValidFakeQuantWithMinMaxArgs
func @testValidFakeQuantWithMinMaxArgs(tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>):
  %0 = "tf.FakeQuantWithMinMaxArgs"(%arg0) {min = -1.0 : f32, max = 1.0 : f32, num_bits = 3} : (tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxArgs
func @testInvalidFakeQuantWithMinMaxArgsWrongAttr(tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>):
  // expected-error @+1 {{requires num_bits to be between 2 and 16, inclusive}}
  %0 = "tf.FakeQuantWithMinMaxArgs"(%arg0) {min = -1.0 : f32, max = 1.0 : f32, num_bits = 0} : (tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test valid tf.FakeQuantWithMinMaxVars
// CHECK-LABEL: func @testValidFakeQuantWithMinMaxVars
func @testValidFakeQuantWithMinMaxVars(tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<f32>):
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongAttr(tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<f32>):
  // expected-error @+1 {{requires num_bits to be between 2 and 16, inclusive}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {min = -1.0 : f32, max = 1.0 : f32, num_bits = 0} : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongMinRank(tensor<8x8x8x8xf32>, tensor<1xf32>, tensor<2xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<1xf32>, %arg2: tensor<2xf32>):
  // expected-error @+1 {{requires min to be a 0d float tensor}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<1xf32>, tensor<2xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongMaxRank(tensor<8x8x8x8xf32>, tensor<f32>, tensor<2xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<2xf32>):
  // expected-error @+1 {{requires max to be a 0d float tensor}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<2xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongMinType(tensor<8x8x8x8xf32>, tensor<i32>, tensor<i32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<i32>, %arg2: tensor<i32>):
  // expected-error @+1 {{op operand #1 must be tensor of 32-bit float values}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<i32>, tensor<i32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongMaxType(tensor<8x8x8x8xf32>, tensor<f32>, tensor<i32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<i32>):
  // expected-error @+1 {{op operand #2 must be tensor of 32-bit float values}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<i32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test valid tf.FakeQuantWithMinMaxVarsPerChannel
// CHECK-LABEL: func @FakeQuantWithMinMaxVarsPerChannel
func @FakeQuantWithMinMaxVarsPerChannel(tensor<1x2x3x8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<1x2x3x8xf32> {
^bb0(%arg0: tensor<1x2x3x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>):
  %0 = "tf.FakeQuantWithMinMaxVarsPerChannel"(%arg0, %arg1, %arg2) : (tensor<1x2x3x8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<1x2x3x8xf32>
  return %0 : tensor<1x2x3x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVarsPerChannel
func @FakeQuantWithMinMaxVarsPerChannel_ranked_inputs(tensor<f32>, tensor<8xf32>, tensor<8xf32>) -> tensor<f32> {
^bb0(%arg0: tensor<f32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>):
  // expected-error @+1 {{requires inputs to be at least 1d float tensor}}
  %0 = "tf.FakeQuantWithMinMaxVarsPerChannel"(%arg0, %arg1, %arg2) : (tensor<f32>, tensor<8xf32>, tensor<8xf32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVarsPerChannel
func @FakeQuantWithMinMaxVarsPerChannel_mismatch_min_max(tensor<1x2x3x8xf32>, tensor<1xf32>, tensor<8xf32>) -> tensor<1x2x3x8xf32> {
^bb0(%arg0: tensor<1x2x3x8xf32>, %arg1: tensor<1xf32>, %arg2: tensor<8xf32>):
  // expected-error @+1 {{requires min and max to have same size as last dimension of inputs}}
  %0 = "tf.FakeQuantWithMinMaxVarsPerChannel"(%arg0, %arg1, %arg2) : (tensor<1x2x3x8xf32>, tensor<1xf32>, tensor<8xf32>) -> tensor<1x2x3x8xf32>
  return %0 : tensor<1x2x3x8xf32>
}

// -----

// Test valid tf.FusedBatchNorm
// CHECK-LABEL: func @testFusedBatchNorm
func @testFusedBatchNorm(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongXType(tensor<8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // expected-error @+1 {{requires x to be a 4D float tensor}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8xf32>
}

// -----

// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongScaleType(tensor<8x8x8x8xf32>, tensor<8xi32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xi32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // expected-error @+1 {{operand #1 must be tensor of 32-bit float values}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xi32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xi32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongOffsetType(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<2x8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<2x8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // expected-error @+1 {{requires offset to be a 1D float tensor}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<2x8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<2x8xf32>, tensor<8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----
// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongMeanType(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<?x8xf32>, tensor<8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<?x8xf32>, %arg4: tensor<8xf32>):
  // expected-error @+1 {{requires mean to be a 1D float tensor}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<?x8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<?x8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----
// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongVarianceType(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<*xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<*xf32>):
  // expected-error @+1 {{requires variance to be a 1D float tensor}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<*xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<*xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----
func @testIfThen(tensor<*xf32>) -> tensor<*xf32>
func @testIfElse(tensor<*xf32>) -> tensor<*xf32>

// Test valid tf.If operation
// CHECK-LABEL: func @testValidIfOp
func @testValidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen, else_branch = @testIfElse, is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func @testIfThen(f32) -> f32
func @testIfElse(f32) -> f32

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, f32) -> f32 {
^bb0(%arg0: tensor<i1>, %arg1: f32):
  // expected-error @+1 {{operand #1 must be tensor of tf.dtype values}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, f32) -> f32

  return %1 : f32
}

// -----

func @testIfElse(tensor<2xf32>) -> tensor<2xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  // expected-error @+1 {{requires attribute 'then_branch'}}
  %1 = "tf.If"(%arg0, %arg1) {
    else_branch = @testIfElse, is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func @testIfThen(tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
func @testIfElse(tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  // expected-error @+1 {{branches should have 1 inputs}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func @testIfThen(tensor<*xf16>) -> tensor<*xf32>
func @testIfElse(tensor<*xf32>) -> tensor<*xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  // expected-error @+1 {{then branch input type tensor<*xf16> is incompatible with operand type tensor<2xf32>}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func @testIfThen(tensor<2xf32>) -> tensor<*xf32>
func @testIfElse(tensor<3xf32>) -> tensor<*xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<*xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<*xf32>):
  // expected-error @+1 {{branches inputs have incompatible types tensor<2xf32> and tensor<3xf32>}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<*xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func @testIfThen(tensor<*xf32>) -> tensor<*xf32>
func @testIfElse(tensor<*xf32>) -> tensor<3xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<*xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<*xf32>):
  // expected-error @+1 {{else branch result type tensor<3xf32> is incompatible with op result type tensor<2xf32>}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<*xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

// Test valid tf.Softmax
// CHECK-LABEL: func @testSoftmax
func @testSoftmax(tensor<8x16xf32>) -> tensor<8x16xf32> {
^bb0(%arg0: tensor<8x16xf32>):
  %0 = "tf.Softmax"(%arg0) {T = "tfdtype$DT_FLOAT"} : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
}

// -----

// Test invalid tf.Softmax
func @testSoftmax(%arg0 : tensor<f32>) -> tensor<f32> {
  // expected-error @+1 {{requires operand to have rank at least 1}}
  %0 = "tf.Softmax"(%arg0) {T = "tfdtype$DT_FLOAT"} : (tensor<f32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

func @testWhileCond(tensor<*xf32>) -> (tensor<i1>)
func @testWhileBody(tensor<*xf32>) -> (tensor<*xf32>)

// Test valid 'While' operation
// CHECK-LABEL: func @testWhileResult
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----
func @testWhileUndefinedCond(%arg0: tensor<i1>, %arg1: tensor<f32>) -> tensor<f32> {
  // expected-error @+1 {{cond refers to an undefined function : undefined_func}}
  %0 = "tf.While"(%arg0, %arg1) {cond = @undefined_func, body = @body, is_stateless = false} : (tensor<i1>, tensor<f32>) -> (tensor<f32>)
  return %0 : tensor<f32>
}

func @body(%arg0: tensor<i1>, %arg1: tensor<f32>) -> tensor<f32>

// -----
func @testWhileUndefinedBody(%arg0: tensor<i1>, %arg1: tensor<f32>) -> tensor<f32> {
  // expected-error @+1 {{body refers to an undefined function : undefined_func}}
  %0 = "tf.While"(%arg0, %arg1) {cond = @cond, body = @undefined_func, is_stateless = false} : (tensor<i1>, tensor<f32>) -> (tensor<f32>)
  return %0 : tensor<f32>
}

func @cond(%arg0: tensor<i1>, %arg1: tensor<f32>) -> tensor<i1>

// -----

func @testWhileCond(tensor<*xf32>) -> ()
func @testWhileBody(tensor<*xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{requires cond function to have exactly one result}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func @testWhileCond(tensor<*xf32>) -> (tensor<i1>)
func @testWhileBody(tensor<*xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xi32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{operand type tensor<*xf32> is incompatible with result type}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xi32>)

  return %1 : tensor<*xi32>
}

// -----

func @testWhileCond(tensor<*xi32>) -> (tensor<i1>)
func @testWhileBody(tensor<*xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{operand type tensor<*xf32> is incompatible with cond function input type}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func @testWhileCond(tensor<*xf32>) -> (tensor<i1>)
func @testWhileBody(tensor<*xf32>, tensor<*xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{requires the number of operands to be equal to the number of body function inputs. Found 1 and 2, respectively}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func @testWhileCond(tensor<*xf32>) -> (tensor<i1>)
func @testWhileBody(tensor<*xf32>) -> (tensor<*xi32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{body function result type tensor<*xi32> is incompatible with result type}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func @testWhileCond(tensor<3xf32>) -> (tensor<i1>)
func @testWhileBody(tensor<4xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{cond function input type tensor<3xf32> is incompatible with body function input type}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

// CHECK-LABEL: func @testValidShape
func @testValidShape(tensor<1x32x32x16xf32>, tensor<*xf32>) -> (tensor<4xi32>, tensor<?xi32>) {
^bb0(%arg0: tensor<1x32x32x16xf32>, %arg1: tensor<*xf32>):
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<1x32x32x16xf32>) -> tensor<4xi32>
  %1 = "tf.Shape"(%arg1) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<*xf32>) -> tensor<?xi32>
  return %0, %1 : tensor<4xi32>, tensor<?xi32>
}

// -----

func @testShapeWrongResultElemType(%arg0: tensor<1x32x32x16xf32>) -> tensor<4xf32> {
  // expected-error @+1 {{result #0 must be tensor of 32/64-bit integer values}}
  %0 = "tf.Shape"(%arg0) : (tensor<1x32x32x16xf32>) -> tensor<4xf32>
  return %0 : tensor<4xf32>
}

// -----

func @testShapeWrongResultDim(tensor<1x32x32x16xf32>) -> tensor<*xi32> {
^bb0(%arg0: tensor<1x32x32x16xf32>):
  // expected-error @+1 {{requires 1D type for result}}
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<1x32x32x16xf32>) -> tensor<*xi32>
  return %0 : tensor<*xi32>
}

// -----

func @testShapeMismatchDim(tensor<1x32x32x16xf32>) -> tensor<2xi32> {
^bb0(%arg0: tensor<1x32x32x16xf32>):
  // expected-error @+1 {{requires dimension size of result to match rank of operand}}
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<1x32x32x16xf32>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

func @testShapeWrongResultDimDynamic(tensor<*xf32>) -> tensor<2xi32> {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{requires dynamic shape result for unranked operand}}
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<*xf32>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

// CHECK-LABEL: func @testValidShapeN
func @testValidShapeN(%arg0 : tensor<1x32x32x16xf32>, %arg1 : tensor<*xf32>) -> (tensor<4xi32>, tensor<?xi32>) {
  // CHECK-NEXT: "tf.ShapeN"
  %0:2 = "tf.ShapeN"(%arg0, %arg1) {N = 2 : i64} : (tensor<1x32x32x16xf32>, tensor<*xf32>) -> (tensor<4xi32>, tensor<?xi32>)
  return %0#0, %0#1 : tensor<4xi32>, tensor<?xi32>
}

// -----

func @testShapeNWrongResultElemType(%arg0: tensor<1x32x32x16xf32>) -> tensor<4xf32> {
  // expected-error @+1 {{result #1 must be tensor of 32/64-bit integer values}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0) {N = 2 : i64} : (tensor<1x32x32x16xf32>, tensor<1x32x32x16xf32>) -> (tensor<4xi32>, tensor<4xf32>)
  return %0#1 : tensor<4xf32>
}

// -----

func @testShapeNWrongResultDim(tensor<1x32x32x16xf32>) -> tensor<*xi32> {
^bb0(%arg0: tensor<1x32x32x16xf32>):
  // expected-error @+1 {{requires 1D type for result #1}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0) {N = 2 : i64} : (tensor<1x32x32x16xf32>, tensor<1x32x32x16xf32>) -> (tensor<4xi32>, tensor<*xi32>)
  return %0#1 : tensor<*xi32>
}

// -----

func @testShapeNMismatchDim(tensor<1x32x32x16xf32>) -> tensor<2xi32> {
^bb0(%arg0: tensor<1x32x32x16xf32>):
  // expected-error @+1 {{requires dimension size of result #1 to match rank of operand #1}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0) {N = 2 : i64} : (tensor<1x32x32x16xf32>, tensor<1x32x32x16xf32>) -> (tensor<4xi32>, tensor<2xi32>)
  return %0#1 : tensor<2xi32>
}

// -----

func @testShapeNWrongResultDimDynamic(tensor<*xf32>) -> tensor<2xi32> {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{requires dynamic shape result #1 for unranked operand #1}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0) {N = 2 : i64} : (tensor<*xf32>, tensor<*xf32>) -> (tensor<?xi32>, tensor<2xi32>)
  return %0#1 : tensor<2xi32>
}

// -----

func @testShapeNWrongNumOperands(tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{requires 3 operand(s), got 2 operand(s)}}
  %0:3 = "tf.ShapeN"(%arg0, %arg0) {N = 3 : i64} : (tensor<*xf32>, tensor<*xf32>) -> (tensor<?xi32>, tensor<?xi32>, tensor<?xi32>)
  return
}

// -----

func @testShapeNWrongNumResults(tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{requires 3 result(s), got 2 result(s)}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0, %arg0) {N = 3 : i64} : (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>) -> (tensor<?xi32>, tensor<?xi32>)
  return
}

// -----

// Test invalid tf.Const
func @testConst() -> tensor<f32> {
  // expected-error @+1 {{attribute 'value' failed to satisfy constraint: constant vector/tensor}}
  %0 = "tf.Const"() {T = "tfdtype$DT_FLOAT", value = 1.0 : f32} : () -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

// Test valid tf.Transpose
// CHECK-LABEL: testTranspose
func @testTranspose(tensor<2x3xf32>) -> tensor<3x2xf32> {
^bb0(%arg0: tensor<2x3xf32>):
  %cst = constant dense<[1, 0]> : tensor<2xi32>
  %0 = "tf.Transpose"(%arg0, %cst) {T = "tfdtype$DT_FLOAT", Tperm = "tfdtype$DT_INT32"} : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
}

// -----

// Test invalid tf.Less
func @testLess(tensor<4xi32>, tensor<4xi32>) -> tensor<4xi32> {
^bb0(%arg0: tensor<4xi32>, %arg1: tensor<4xi32>):
  // expected-error @+1 {{op result #0 must be tensor of 1-bit integer values}}
  %0 = "tf.Less"(%arg0, %arg1) : (tensor<4xi32>, tensor<4xi32>) -> tensor<4xi32>
  return %0 : tensor<4xi32>
}

// -----

// Test valid tf.ConcatV2
func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<i32>) -> tensor<?xf32> {
  %0 = "tf.ConcatV2"(%arg, %arg, %axis) {N = 2: i64} : (tensor<8x16xf32>, tensor<8x16xf32>, tensor<i32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

// tf.ConcatV2 with wrong 'axis' element type
func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<f32>) -> tensor<?xf32> {
  // expected-error @+1 {{operand #2 must be tensor of 32/64-bit integer values}}
  %0 = "tf.ConcatV2"(%arg, %arg, %axis) {N = 2: i64} : (tensor<8x16xf32>, tensor<8x16xf32>, tensor<f32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

// tf.ConcatV2 missing required 'axis' operand
func @testConcatV2() -> tensor<?xf32> {
  // expected-error @+1 {{expected 1 or more operands}}
  %0 = "tf.ConcatV2"() {N = 0: i64} : () -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

// tf.ConcatV2 with less than required number of values for the variadic operand
func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<i32>) -> tensor<?xf32> {
  // expected-error @+1 {{attribute 'N' failed to satisfy constraint: 64-bit integer attribute whose minimal value is 2}}
  %0 = "tf.ConcatV2"(%arg, %axis) {N = 1: i64} : (tensor<8x16xf32>, tensor<i32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<i32>) -> tensor<?xf32> {
  // expected-error @+1 {{requires attribute 'N' to match the number of inputs; expected: 2 Found: 3}}
  %0 = "tf.ConcatV2"(%arg, %arg, %axis) {N = 3: i64} : (tensor<8x16xf32>, tensor<8x16xf32>, tensor<i32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

// CHECK-LABEL: testAll
func @testAll(%arg0: tensor<2x2xi1>, %arg1: tensor<i32>) -> tensor<i1> {
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<i32>) -> tensor<i1>
  return %0 : tensor<i1>
}

// -----

// CHECK-LABEL: testAll64
func @testAll64(%arg0: tensor<2x2xi1>, %arg1: tensor<i64>) -> tensor<i1> {
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<i64>) -> tensor<i1>
  return %0 : tensor<i1>
}

// -----

func @testAllFloat(%arg0: tensor<2x2xi1>, %arg1: tensor<f32>) -> tensor<i1> {
  // expected-error @+1 {{'tf.All' op operand #1 must be tensor of 32/64-bit integer values}}
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<f32>) -> tensor<i1>
  return %0 : tensor<i1>
}

// -----

func @testAllI32(%arg0: tensor<2x2xi32>, %arg1: tensor<f32>) -> tensor<i32> {
  // expected-error @+1 {{'tf.All' op operand #0 must be tensor of 1-bit integer values}}
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi32>, tensor<f32>) -> tensor<i32>
  return %0 : tensor<i32>
}

// -----

func @testEqualOpIncompatibleShapeTrue(%x: tensor<5xf32>, %y: tensor<4xf32>) -> tensor<5xi1> {
  // expected-error @+1 {{operands don't have broadcast-compatible shapes}}
  %0 = "tf.Equal"(%x, %y) {incompatible_shape_error = true} : (tensor<5xf32>, tensor<4xf32>) -> tensor<5xi1>
  return %0 : tensor<5xi1>
}

// -----

// CHECK-LABEL: testEqualOpIncompatibleShapeFalse
func @testEqualOpIncompatibleShapeFalse(%x: tensor<5xf32>, %y: tensor<4xf32>) -> tensor<*xi1> {
  %0 = "tf.Equal"(%x, %y) {incompatible_shape_error = false} : (tensor<5xf32>, tensor<4xf32>) -> tensor<*xi1>
  return %0 : tensor<*xi1>
}

// -----

func @testNotEqualOpIncompatibleShapeTrue(%x: tensor<5xf32>, %y: tensor<4xf32>) -> tensor<5xi1> {
  // expected-error @+1 {{operands don't have broadcast-compatible shapes}}
  %0 = "tf.NotEqual"(%x, %y) {incompatible_shape_error = true} : (tensor<5xf32>, tensor<4xf32>) -> tensor<5xi1>
  return %0 : tensor<5xi1>
}

// -----

// CHECK-LABEL: testNotEqualOpIncompatibleShapeFalse
func @testNotEqualOpIncompatibleShapeFalse(%x: tensor<5xf32>, %y: tensor<4xf32>) -> tensor<*xi1> {
  %0 = "tf.NotEqual"(%x, %y) {incompatible_shape_error = false} : (tensor<5xf32>, tensor<4xf32>) -> tensor<*xi1>
  return %0 : tensor<*xi1>
}

// -----

func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<1x1xi32>) -> tensor<*xf32> { // expected-error @+1 {{requires axis to be of scalar type (or vector type for older versions)}}
  %0 = "tf.ConcatV2"(%arg, %arg, %axis) {N = 2: i64} : (tensor<8x16xf32>, tensor<8x16xf32>, tensor<1x1xi32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<1x1xi32>) -> tensor<*xf32> {
  // expected-error @+1 {{requires axis to be of scalar type (or vector type for older versions)}}
  %0 = "tf.Concat"(%axis, %arg, %arg) {N = 2: i64} : (tensor<1x1xi32>, tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testConcatV2(%arg0: tensor<8x16xf32>, %arg1: tensor<8xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  // expected-error @+1 {{operand type 'tensor<8xf32>' is not compatible with preceding operands; expected rank: 2}}
  %0 = "tf.ConcatV2"(%arg0, %arg1, %axis) {N = 2: i64} : (tensor<8x16xf32>, tensor<8xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// Valid Concat operation with concat axis 1 or -1.
func @testConcatV2(%arg0: tensor<8x16xf32>, %arg1: tensor<8x8xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  %0 = "tf.ConcatV2"(%arg0, %arg1, %axis) {N = 2: i64} : (tensor<8x16xf32>, tensor<8x8xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testConcatV2(%arg0: tensor<8x16xf32>, %arg1: tensor<16x8xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  // expected-error @+1 {{operand type 'tensor<16x8xf32>' is not compatible with preceding operands; expected dimension at index 1: 16}}
  %0 = "tf.ConcatV2"(%arg0, %arg1, %axis) {N = 2: i64} : (tensor<8x16xf32>, tensor<16x8xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// Valid Concat operation with concat axis 1 or -1.
func @testConcatV2(%arg0: tensor<8x8xf32>, %arg1: tensor<?x4xf32>, %arg2: tensor<*xf32>, %arg3: tensor<8x?xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  %0 = "tf.ConcatV2"(%arg0, %arg1, %arg2, %arg3, %axis) {N = 4: i64} : (tensor<8x8xf32>, tensor<?x4xf32>, tensor<*xf32>, tensor<8x?xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// Valid Pack operation.
func @testPack(%arg0: tensor<4x8xf32>, %arg1: tensor<4x8xf32>) -> tensor<*xf32> {
  %0 = "tf.Pack"(%arg0, %arg1) {axis = 1 : i64, N = 2: i64} : (tensor<4x8xf32>, tensor<4x8xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testPack(%arg0: tensor<4x8xf32>, %arg1: tensor<4x8xf32>) -> tensor<*xf32> {
  // expected-error @+1 {{requires attribute 'N' to match the number of inputs; expected: 2 Found: 1}}
  %0 = "tf.Pack"(%arg0, %arg1) {axis = 1 : i64, N = 1: i64} : (tensor<4x8xf32>, tensor<4x8xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testPack(%arg0: tensor<4x8xf32>, %arg1: tensor<4x2xf32>) -> tensor<*xf32> {
  // expected-error @+1 {{operand type 'tensor<4x2xf32>' is not compatible with preceding operands; expected dimension at index 1: 8}}
  %0 = "tf.Pack"(%arg0, %arg1) {axis = 1 : i64, N = 2: i64} : (tensor<4x8xf32>, tensor<4x2xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testPack(%arg0: tensor<4x8xf32>, %arg1: tensor<4x8xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  // expected-error @+1 {{attribute 'axis' should be within range [-3, 3); actual value: 3}}
  %0 = "tf.Pack"(%arg0, %arg1) {axis = 3 : i64, N = 2: i64} : (tensor<4x8xf32>, tensor<4x8xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// Valid slice operation.
func @testSlice(%arg0: tensor<3x4xi32>, %arg1: tensor<2xi64>) -> tensor<1x4xi32> {
  %sizes = "tf.Const"() {value = dense<[1, 4]> : tensor<2xi64>} : () -> (tensor<2xi64>)
  %0 = "tf.Slice"(%arg0, %arg1, %sizes) : (tensor<3x4xi32>, tensor<2xi64>, tensor<2xi64>) -> tensor<1x4xi32>
  return %0 : tensor<1x4xi32>
}

// -----

func @testSlice_begin_2d(%arg0: tensor<4xi32>, %begins: tensor<2x2xi64>) -> tensor<3xi32> {
  %sizes = "tf.Const"() {value = dense<[1]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires begin operand to be 1D tensor}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<2x2xi64>, tensor<1xi64>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

func @testSlice_size_two_much_elements(%arg0: tensor<4xi32>) -> tensor<3xi32> {
  %begins = "tf.Const"() {value = dense<[1]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  %sizes = "tf.Const"() {value = dense<[1, 2]> : tensor<2xi64>} : () -> (tensor<2xi64>)
  // expected-error @+1 {{requires begin and size operands to have the same number of elements}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<2xi64>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

func @testSlice_begin_negative(%arg0: tensor<4xi32>) -> tensor<2xi32> {
  %begins = "tf.Const"() {value = dense<[-1]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  %sizes = "tf.Const"() {value = dense<[2]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires 0 <= begin[i] <= begin[i] + size[i] <= Di}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<1xi64>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

func @testSlice_begin_out_of_bound(%arg0: tensor<4xi32>) -> tensor<2xi32> {
  %begins = "tf.Const"() {value = dense<[4]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  %sizes = "tf.Const"() {value = dense<[2]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires 0 <= begin[i] <= begin[i] + size[i] <= Di}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<1xi64>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

// Valid StridedSlice operation.
func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<2xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>) -> tensor<?x?xf32> {
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<2xi64>, tensor<2xi64>, tensor<2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<i64>, %end: tensor<i64>, %strides: tensor<i64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{requires begin, end and strides to be 1D tensors}}
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<i64>, tensor<i64>, tensor<i64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<32xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{with less than 32 elements}}
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<32xi64>, tensor<2xi64>, tensor<2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<?xi64>, %end: tensor<3xi64>, %strides: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{to have the same number of elements}}
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<?xi64>, tensor<3xi64>, tensor<2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>) -> tensor<?x?xf32> {
  %begin = "tf.Const"() { value = dense<[0, 0]> : tensor<2xi64> } : () -> tensor<?xi64>
  %end = "tf.Const"() { value = dense<[5, 10]> : tensor<2xi64> } : () -> tensor<?xi64>
  %strides = "tf.Const"() { value = dense<[2, 3, 4]> : tensor<3xi64> } : () -> tensor<?xi64>

  // expected-error @+1 {{to have the same number of elements}}
  %1 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<?xi64>, tensor<?xi64>, tensor<?xi64>) -> tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<2xi32>, %end: tensor<2xi32>) -> tensor<?x?xf32> {
  %strides = "tf.Const"() { value = dense<[2, 0]> : tensor<2xi32> } : () -> tensor<2xi32>

  // expected-error @+1 {{requires non-zero strides}}
  %1 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<?x?xf32>
  return %1 : tensor<?x?xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<i32>, %on_value: tensor<f32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %on_value: tensor<f32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  %depth = "tf.Const"() { value = dense<-5> : tensor<i64> } : () -> tensor<i32>
  // expected-error @+1 {{depth must be non-negative}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<2xi32>, %on_value: tensor<f32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  // expected-error @+1 {{requires depth to be a scalar}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<2xi32>, tensor<f32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<i32>, %on_value: tensor<2xf32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  // expected-error @+1 {{requires on_value to be a scalar}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<2xf32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<i32>, %on_value: tensor<f32>, %off_value: tensor<2xf32>) -> tensor<3x5xf32> {
  // expected-error @+1 {{requires off_value to be a scalar}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<2xf32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<i32>, %on_value: tensor<f32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  // expected-error @+1 {{expected axis (-2) to be -1 or between [0, 1]}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -2 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}
