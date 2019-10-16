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

%include "tensorflow/python/platform/base.i"

%{

#include "mlir/Pass/PassRegistry.h"
#include "mlir/Pass/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/import_model.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/error_util.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/import_utils.h"

namespace tensorflow {
namespace swig {

// Simple wrapper to support tf.mlir.experimental.convert_graph_def.
// Load a .pbptx, convert to MLIR, and (optionally) optimize the module before
// returning it as a string.
// This is an early experimental API, ideally we should return a wrapper object
// around a Python binding to the MLIR module.
string ImportGraphDef(const string &proto, const string &pass_pipeline, TF_Status* status) {
  GraphDef graphdef;
  auto s = tensorflow::LoadProtoFromBuffer(proto, &graphdef);
  if (!s.ok()) {
    Set_TF_Status_from_Status(status, s);
    return "// error";
  }
  GraphDebugInfo debug_info;
  GraphImportConfig specs;
  mlir::MLIRContext context;
  auto module = ConvertGraphdefToMlir(graphdef, debug_info, specs, &context);
  if (!module.ok()) {
    Set_TF_Status_from_Status(status, module.status());
    return "// error";
  }

  // Run the pass_pipeline on the module if not empty.
  if (!pass_pipeline.empty()) {
    mlir::PassManager pm(&context);
    std::string error;
    llvm::raw_string_ostream error_stream(error);
    if (failed(mlir::parsePassPipeline(pass_pipeline, pm, error_stream))) {
      TF_SetStatus(status, TF_INVALID_ARGUMENT,
                   ("Invalid pass_pipeline: " + error_stream.str()).c_str());
      return "// error";
    }

    mlir::StatusScopedDiagnosticHandler statusHandler(&context);
    if (failed(pm.run(*module.ValueOrDie()))) {
      Set_TF_Status_from_Status(status, statusHandler.ConsumeStatus());
      return "// error";
    }
  }
  return MlirModuleToString(*module.ConsumeValueOrDie());
}

// Load a SavedModel and return a textual MLIR string corresponding to it.
//
// Args:
//   saved_model_path: File path from which to load the SavedModel.
//   exported_names_str: Comma-separated list of names to export.
//                       Empty means "export all".
//
// Returns:
//   A string of textual MLIR representing the raw imported SavedModel.
string ExperimentalConvertSavedModelToMlir(
    const string &saved_model_path,
    const string &exported_names_str,
    bool show_debug_info,
    TF_Status* status) {
  // Load the saved model into a SavedModelBundle.

  // TODO(silvasean): Add support for tags, if needed.
  // The default "serve" tag seems to be enough.
  std::unordered_set<string> tags;
  tags.insert("serve");
  SessionOptions session_options;
  RunOptions run_options;
  tensorflow::SavedModelBundle bundle;
  auto load_status = LoadSavedModel(session_options, run_options,
                                    saved_model_path, tags, &bundle);
  if (!load_status.ok()) {
    Set_TF_Status_from_Status(status, load_status);
    return "// error";
  }

  // Convert the SavedModelBundle to an MLIR module.

  std::vector<string> exported_names =
      absl::StrSplit(exported_names_str, ',', absl::SkipEmpty());
  mlir::MLIRContext context;
  auto module_or = ConvertSavedModelToMlir(bundle, &context,
      absl::Span<std::string>(exported_names));
  if (!module_or.status().ok()) {
    Set_TF_Status_from_Status(status, module_or.status());
    return "// error";
  }

  return MlirModuleToString(*module_or.ConsumeValueOrDie(), show_debug_info);
}

}  // namespace swig
}  // namespace tensorflow

%}

%ignoreall

%unignore tensorflow;
%unignore tensorflow::swig;
%unignore tensorflow::swig::ImportGraphDef;
%unignore tensorflow::swig::ExperimentalConvertSavedModelToMlir;

// Wrap this function
namespace tensorflow {
namespace swig {
static string ImportGraphDef(const string &graphdef,
                             const string &pass_pipeline,
                             TF_Status* status);
static string ExperimentalConvertSavedModelToMlir(
    const string &saved_model_path,
    const string &exported_names,
    bool show_debug_info,
    TF_Status* status);
}  // namespace swig
}  // namespace tensorflow

%insert("python") %{
def import_graphdef(graphdef, pass_pipeline):
  return ImportGraphDef(str(graphdef).encode('utf-8'), pass_pipeline.encode('utf-8')).decode('utf-8');

def experimental_convert_saved_model_to_mlir(saved_model_path,
                                             exported_names,
                                             show_debug_info):
  return ExperimentalConvertSavedModelToMlir(
    str(saved_model_path).encode('utf-8'),
    str(exported_names).encode('utf-8'),
    show_debug_info
  ).decode('utf-8');
%}

%unignoreall
