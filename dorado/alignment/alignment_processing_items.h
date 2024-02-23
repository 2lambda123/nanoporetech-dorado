#pragma once

#include "read_pipeline/HtsWriter.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace dorado::alignment::cli {

struct AlignmentProcessingInfo {
    AlignmentProcessingInfo() {}
    AlignmentProcessingInfo(std::string input_,
                            std::string output_,
                            dorado::HtsWriter::OutputMode output_mode_)
            : input(std::move(input_)), output(std::move(output_)), output_mode(output_mode_) {}
    std::string input{};
    std::string output{};
    dorado::HtsWriter::OutputMode output_mode{};
};

class AlignmentProcessingItems {
    const std::string m_input_path;
    const std::string m_output_folder;
    bool m_recursive_input;

    std::unordered_multimap<std::string, std::filesystem::path> m_working_paths{};

    std::vector<AlignmentProcessingInfo> m_processing_list{};

    template <class ITER>
    void create_working_file_map();

    template <class ITER>
    void add_all_valid_files();

    bool check_recursive_arg_false();

    bool try_create_output_folder();

    bool check_output_folder_for_input_folder(const std::string& input_folder);

    bool try_add_to_working_files(const std::filesystem::path& input_root,
                                  const std::filesystem::path& input_relative_path);

    void add_to_working_files(const std::filesystem::path& input_relative_path);

    bool initialise_for_stdin();

    bool initialise_for_file();

    bool initialise_for_folder();

public:
    AlignmentProcessingItems(std::string input_path,
                             bool recursive_input,
                             std::string output_folder);

    bool initialise();

    const std::vector<AlignmentProcessingInfo>& get() const { return m_processing_list; }
};

}  // namespace dorado::alignment::cli