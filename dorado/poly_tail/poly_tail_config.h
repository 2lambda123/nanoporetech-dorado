#pragma once

#include <istream>
#include <string>

namespace dorado::poly_tail {

struct PolyTailConfig {
    std::string front_primer = "TTTCTGTTGGTGCTGATATTGCTTT";                          // SSP
    std::string rear_primer = "ACTTGCCTGTCGCTCTATCTTCAGAGGAGAGTCCGCCGCCCGCAAGTTTT";  // VNP
    std::string rc_front_primer;
    std::string rc_rear_primer;
    std::string plasmid_front_flank;
    std::string plasmid_rear_flank;
    std::string rc_plasmid_front_flank;
    std::string rc_plasmid_rear_flank;
    int plasmid_flank_threshold = 10;
    bool is_plasmid = false;
    int tail_interrupt_length = 0;
    int min_base_count = 10;
};

// Prepare the PolyA configuration struct. If a configuration
// file is available, parse it to extract parameters. Otherwise
// prepare the default configuration.
PolyTailConfig prepare_config(const std::string* const config_file);

// Overloaded function that parses the configuration passed
// in as an input stream.
PolyTailConfig prepare_config(std::istream& is);

}  // namespace dorado::poly_tail
