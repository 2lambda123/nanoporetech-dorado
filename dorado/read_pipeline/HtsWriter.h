#pragma once
#include "read_pipeline/ReadPipeline.h"
#include "utils/hts_file.h"
#include "utils/stats.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>

struct sam_hdr_t;
struct bam1_t;

namespace dorado {

class HtsWriter : public MessageSink {
public:
    HtsWriter(const std::string& filename, utils::HtsFile::OutputMode mode, size_t threads);
    ~HtsWriter();
    std::string get_name() const override { return "HtsWriter"; }
    stats::NamedStats sample_stats() const override;
    void terminate(const FlushOptions&) override;
    void restart() override { start_input_processing(&HtsWriter::input_thread_fn, this); }

    int set_and_write_header(const sam_hdr_t* header);
    size_t get_total() const { return m_total; }
    size_t get_primary() const { return m_primary; }
    size_t get_unmapped() const { return m_unmapped; }

    static utils::HtsFile::OutputMode get_output_mode(const std::string& mode);

private:
    size_t m_total{0};
    size_t m_primary{0};
    size_t m_unmapped{0};
    size_t m_secondary{0};
    size_t m_supplementary{0};

    std::unique_ptr<utils::HtsFile> m_file;

    void input_thread_fn();
    int write(const bam1_t* record);
    std::unordered_set<std::string> m_processed_read_ids;
    std::atomic<int> m_duplex_reads_written{0};
    std::atomic<int> m_split_reads_written{0};
};

}  // namespace dorado
