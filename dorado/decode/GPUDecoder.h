#pragma once

#include "Decoder.h"

#include <torch/torch.h>

namespace dorado {

class GPUDecoder : Decoder {
public:
    explicit GPUDecoder(float score_clamp_val = 0) : m_score_clamp_val(score_clamp_val) {}

    std::vector<DecodedChunk> beam_search(const torch::Tensor& scores,
                                          int num_chunks,
                                          const DecoderOptions& options) final;
    constexpr static torch::ScalarType dtype = torch::kF16;

    // We split beam_search into two parts, the first one running on the GPU and the second
    // one on the CPU. While the second part is running we can submit more commands to the GPU
    // on another thread.
    torch::Tensor gpu_part(torch::Tensor scores, int num_chunks, DecoderOptions options);
    std::vector<DecodedChunk> cpu_part(torch::Tensor moves_sequence_qstring_cpu);

    float m_score_clamp_val;
};

}  // namespace dorado
