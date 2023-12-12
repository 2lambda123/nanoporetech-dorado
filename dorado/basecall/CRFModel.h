#pragma once

#include <torch/nn.h>

#include <filesystem>
#include <vector>

namespace dorado {

struct CRFModelConfig;

std::vector<at::Tensor> load_crf_model_weights(const std::filesystem::path& dir,
                                               bool decomposition,
                                               bool bias);

torch::nn::ModuleHolder<torch::nn::AnyModule> load_crf_model(const CRFModelConfig& model_config,
                                                             const at::TensorOptions& options);

size_t auto_calculate_num_runners(const CRFModelConfig& model_config,
                                  size_t batch_size,
                                  float memory_fraction);

}  // namespace dorado
