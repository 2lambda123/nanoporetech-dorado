#pragma once

#include <torch/torch.h>

#include <filesystem>
#include <vector>

namespace dorado {

struct CRFModelConfig;

std::vector<torch::Tensor> load_crf_model_weights(const std::filesystem::path& dir,
                                                  bool decomposition,
                                                  bool bias);

torch::nn::ModuleHolder<torch::nn::AnyModule> load_crf_model(const CRFModelConfig& model_config,
                                                             const torch::TensorOptions& options);
}  // namespace dorado
