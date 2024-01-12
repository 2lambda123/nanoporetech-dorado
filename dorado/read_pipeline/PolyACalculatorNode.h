#pragma once

#include "read_pipeline/MessageSink.h"
#include "utils/stats.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace dorado {

class PolyACalculatorNode : public MessageSink {
public:
    PolyACalculatorNode(size_t num_worker_threads, bool is_rna, size_t max_reads);
    ~PolyACalculatorNode() { terminate_impl(); }
    std::string get_name() const override { return "PolyACalculator"; }
    stats::NamedStats sample_stats() const override;
    void terminate(const FlushOptions &) override { terminate_impl(); };
    void restart() override { start_input_processing(&PolyACalculatorNode::input_thread_fn, this); }

private:
    void terminate_impl();
    void input_thread_fn();

    const bool m_is_rna;
    std::atomic<size_t> total_tail_lengths_called{0};
    std::atomic<int> num_called{0};
    std::atomic<int> num_not_called{0};

    std::mutex m_mutex;
    std::map<int, int> tail_length_counts;
};

}  // namespace dorado
