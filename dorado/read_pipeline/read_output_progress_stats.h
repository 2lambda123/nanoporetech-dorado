#pragma once

#include "utils/stats.h"

#include <chrono>
#include <mutex>

namespace dorado {

class ReadOutputProgressStats {
    using progress_clock = std::chrono::steady_clock;
    const std::chrono::seconds m_interval_duration;
    const std::size_t m_num_input_files;

    std::mutex m_mutex;
    progress_clock::time_point m_monitoring_start_time;
    progress_clock::time_point m_interval_start;
    progress_clock::time_point m_interval_end;
    progress_clock::time_point m_next_report_time;
    progress_clock::time_point m_last_stats_completed_time;

    std::size_t m_previous_stats_total{};
    std::size_t m_interval_previous_stats_total{};
    std::size_t m_interval_start_count{};

    std::size_t m_num_files_where_readcount_known{};
    std::size_t m_total_known_readcount{};
    float m_estimated_num_reads_per_file{};

    void report_stats(const std::size_t current_reads_written_count);

    std::size_t get_adjusted_estimated_total_reads(std::size_t current_reads_count);

    bool is_completed() const;

public:
    ReadOutputProgressStats(std::chrono::seconds interval_duration, std::size_t num_input_files);
    ~ReadOutputProgressStats();

    void update_stats(const stats::NamedStats& stats);

    // Called to indicate the current stats collection has completed.
    // There may be new stats but their counters will be reset to zero.
    // E.g. happens in `dorado aligner` when reading from multiple input files
    // a new pipeline and HtsWriter is created for each input file
    void notify_stats_completed(const stats::NamedStats& stats);

    void update_reads_per_file_estimate(std::size_t num_reads_in_file);
};

}  // namespace dorado
