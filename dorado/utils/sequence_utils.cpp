#include "sequence_utils.h"

#include "simd.h"
#include "types.h"

#include <edlib.h>
#include <minimap.h>
#include <nvtx3/nvtx3.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <iterator>
#include <numeric>
#include <vector>

namespace {

#if ENABLE_AVX2_IMPL
__attribute__((target("default")))
#endif
std::string
reverse_complement_impl(const std::string& sequence) {
    if (sequence.empty()) {
        return {};
    }

    const auto num_bases = sequence.size();
    std::string rev_comp_sequence;
    rev_comp_sequence.resize(num_bases);

    // Compile-time constant lookup table.
    static constexpr auto kComplementTable = [] {
        std::array<char, 256> a{};
        // Valid input will only touch the entries set here.
        a['A'] = 'T';
        a['T'] = 'A';
        a['C'] = 'G';
        a['G'] = 'C';
        return a;
    }();

    // Run every template base through the table, reading in reverse order.
    const char* template_ptr = &sequence[num_bases - 1];
    char* complement_ptr = &rev_comp_sequence[0];
    for (size_t i = 0; i < num_bases; ++i) {
        const auto template_base = *template_ptr--;
        *complement_ptr++ = kComplementTable[template_base];
    }
    return rev_comp_sequence;
}

#if ENABLE_AVX2_IMPL
// AVX2 implementation that does in-register lookups of 32 bases at once, using
// PSHUFB. On strings with over several thousand bases this was measured to be about 10x the speed
// of the default implementation on Skylake.
__attribute__((target("avx2"))) std::string reverse_complement_impl(const std::string& sequence) {
    const auto len = sequence.size();
    std::string rev_comp_sequence;
    rev_comp_sequence.resize(len);

    // Maps from lower 4 bits of template base ASCII to complement base ASCII.
    // It happens that the low 4 bits of A, C, G and T ASCII encodings are unique, and
    // these are the only bits the PSHUFB instruction we use cares about (aside from the high
    // bit, which won't be set for valid input).
    // 'A' & 0xf = 1
    // 'C' & 0xf = 3
    // 'T' & 0xf = 4
    // 'G' & 0xf = 7
    const __m256i kComplementTable =
            _mm256_setr_epi8(0, 'T', 0, 'G', 'A', 0, 0, 'C', 0, 0, 0, 0, 0, 0, 0, 0, 0, 'T', 0, 'G',
                             'A', 0, 0, 'C', 0, 0, 0, 0, 0, 0, 0, 0);

    // PSHUFB indices to reverse bytes within a 16 byte AVX lane.  Note that _mm256_set_..
    // intrinsics have a high to low ordering.
    const __m256i kByteReverseTable =
            _mm256_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5,
                            6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

    // Unroll to AVX register size.  Unrolling further would probably help performance.
    static constexpr size_t kUnroll = 32;

    // This starts pointing at the beginning of the first complete 32 byte template chunk
    // that we load -- i.e. the one last in memory.
    const char* template_ptr = &sequence[len - kUnroll];
    char* complement_ptr = &rev_comp_sequence[0];

    // Main vectorised loop: 32 bases per iteration.
    for (size_t chunk_i = 0; chunk_i < len / kUnroll; ++chunk_i) {
        // Load template bases.
        const __m256i template_bases =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(template_ptr));
        // Look up complement bases.
        const __m256i complement_bases = _mm256_shuffle_epi8(kComplementTable, template_bases);
        // Reverse byte order within 16 byte AVX lanes.
        const __m256i reversed_lanes = _mm256_shuffle_epi8(complement_bases, kByteReverseTable);
        // We store reversed lanes in reverse order to reverse 32 bytes overall.
        // We could alternatively use VPERMQ and a 256 bit store, but the shuffle
        // execution port (i.e. port 5 on Skylake) is oversubscribed.
        const __m128i upper_lane = _mm256_extracti128_si256(reversed_lanes, 1);
        const __m128i lower_lane = _mm256_castsi256_si128(reversed_lanes);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(complement_ptr), upper_lane);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(complement_ptr + 16), lower_lane);

        template_ptr -= kUnroll;
        complement_ptr += kUnroll;
    }

    // Loop for final 0-31 chars.
    const size_t remaining_len = len % kUnroll;
    const __m256i kZero = _mm256_setzero_si256();
    template_ptr = &sequence[remaining_len - 1];
    for (size_t i = 0; i < remaining_len; ++i) {
        // Same steps as in the main loop, but char by char, so there's no
        // reversal of byte ordering, and we load/store with scalar instructions.
        const __m256i template_base = _mm256_insert_epi8(kZero, *template_ptr--, 0);
        const __m256i complement_base = _mm256_shuffle_epi8(kComplementTable, template_base);
        *complement_ptr++ = _mm256_extract_epi8(complement_base, 0);
    }

    return rev_comp_sequence;
}
#endif

}  // namespace

namespace dorado::utils {

float mean_qscore_from_qstring(const std::string& qstring, int start_pos) {
    if (qstring.empty()) {
        return 0.0f;
    }

    if (start_pos >= int(qstring.length())) {
        throw std::runtime_error("Mean q-score start position (" + std::to_string(start_pos) +
                                 ") is >= length of qstring (" + std::to_string(qstring.length()) +
                                 ")");
    }

    // Lookup table avoids repeated invocation of std::pow, which
    // otherwise dominates run time of this function.
    // Unfortunately std::pow is not constexpr, so this can't be.
    static const auto kCharToScoreTable = [] {
        std::array<float, 256> a{};
        for (int q = 33; q <= 127; ++q) {
            auto shifted = static_cast<float>(q - 33);
            a[q] = std::pow(10.0f, -shifted / 10.0f);
        }
        return a;
    }();
    float total_error =
            std::accumulate(qstring.cbegin() + start_pos, qstring.cend(), 0.0f,
                            [](float sum, char qchar) { return sum + kCharToScoreTable[qchar]; });
    float mean_error = total_error / static_cast<float>(qstring.size() - start_pos);
    float mean_qscore = -10.0f * std::log10(mean_error);
    return std::clamp(mean_qscore, 1.0f, 50.0f);
}

std::vector<int> sequence_to_ints(const std::string& sequence) {
    NVTX3_FUNC_RANGE();
    std::vector<int> sequence_ints;
    sequence_ints.reserve(sequence.size());
    std::transform(std::begin(sequence), std::end(sequence),
                   std::back_insert_iterator<std::vector<int>>(sequence_ints), &base_to_int);
    return sequence_ints;
}

// Convert a move table to an array of the indices of the start/end of each base in the signal
std::vector<uint64_t> moves_to_map(const std::vector<uint8_t>& moves,
                                   size_t block_stride,
                                   size_t signal_len,
                                   std::optional<size_t> reserve_size) {
    NVTX3_FUNC_RANGE();
    std::vector<uint64_t> seq_to_sig_map;
    if (reserve_size) {
        seq_to_sig_map.reserve(*reserve_size);
    }

    for (size_t i = 0; i < moves.size(); ++i) {
        if (moves[i] == 1) {
            seq_to_sig_map.push_back(i * block_stride);
        }
    }
    seq_to_sig_map.push_back(signal_len);
    return seq_to_sig_map;
}

OverlapResult compute_overlap(std::string query_seq, std::string target_seq) {
    OverlapResult overlap_result = {false, 0, 0, 0, 0};

    // Add mm2 based overlap check.
    mm_idxopt_t m_idx_opt;
    mm_mapopt_t m_map_opt;
    mm_set_opt(0, &m_idx_opt, &m_map_opt);
    mm_set_opt("map-hifi", &m_idx_opt, &m_map_opt);

    std::vector<const char*> seqs = {query_seq.c_str()};
    std::vector<const char*> names = {"query"};
    mm_idx_t* m_index = mm_idx_str(m_idx_opt.w, m_idx_opt.k, 0, m_idx_opt.bucket_bits, 1,
                                   seqs.data(), names.data());
    mm_mapopt_update(&m_map_opt, m_index);

    MmTbufPtr mbuf = MmTbufPtr(mm_tbuf_init());

    int hits = 0;
    mm_reg1_t* reg = mm_map(m_index, int(target_seq.length()), target_seq.c_str(), &hits,
                            mbuf.get(), &m_map_opt, "target");

    mm_idx_destroy(m_index);

    if (hits > 0) {
        int32_t target_start = 0;
        int32_t target_end = 0;
        int32_t query_start = 0;
        int32_t query_end = 0;

        auto best_map = std::max_element(
                reg, reg + hits,
                [](const mm_reg1_t& l, const mm_reg1_t& r) { return l.mapq < r.mapq; });
        target_start = best_map->rs;
        target_end = best_map->re;
        query_start = best_map->qs;
        query_end = best_map->qe;

        overlap_result = {true, target_start, target_end, query_start, query_end};
    }

    for (int i = 0; i < hits; ++i) {
        free(reg[i].p);
    }
    free(reg);

    return overlap_result;
}

// Query is the read that the moves table is associated with. A new moves table will be generated
// Which is aligned to the target sequence.
std::tuple<int, int, std::vector<uint8_t>> realign_moves(std::string query_sequence,
                                                         std::string target_sequence,
                                                         std::vector<uint8_t> moves) {
    auto [is_overlap, query_start, query_end, target_start, target_end] = compute_overlap(
            query_sequence,
            target_sequence);  // We are going to compute the overlap between the two reads

    // TODO sanity check if and why this is needed
    // Now let's perform an alignmnet:

    EdlibAlignConfig align_config = edlibDefaultAlignConfig();
    align_config.task = EDLIB_TASK_PATH;

    auto target_sequence_component =
            target_sequence.substr(target_start, target_end - target_start);
    auto query_sequence_component = query_sequence.substr(query_start, query_end - query_start);

    EdlibAlignResult edlib_result = edlibAlign(
            target_sequence_component.data(), static_cast<int>(target_sequence_component.length()),
            query_sequence_component.data(), static_cast<int>(query_sequence_component.length()),
            align_config);

    // Now that we have the alignment, we need to compute the new move table, by walking along the alignment

    const auto alignment_size =
            static_cast<size_t>(edlib_result.endLocations[0] - edlib_result.startLocations[0]);
    std::vector<unsigned char> alignment;
    alignment.resize(alignment_size);
    std::memcpy(alignment.data(), &edlib_result.alignment[edlib_result.startLocations[0]],
                alignment_size);

    std::vector<uint8_t> new_moves;

    // Let's keep two cursor positions - one for the new move table and one for the old:
    int new_move_cursor = 0;
    int old_move_cursor =
            0;  // Need to update to be the query start. // TODO do we need to worry about the start and end locations.
    // Let's keep two cursor positions - one for the query sequence and one for the target:
    int query_seq_cursor = query_start;
    int target_seq_cursor = target_start;

    int moves_found = 0;

    while (moves_found < moves.size() &&
           moves_found <
                   query_start) {  // TODO - is "query start" zero indexed? need to think about that
        moves_found += moves[old_move_cursor];
        ++old_move_cursor;
    }
    --old_move_cursor;  // We have gone one too far.
    int old_moves_offset = old_move_cursor;

    // First thing to do - let's just print out the alignment line by line so we know it's working.
    for (auto alignment_entry : alignment) {
        if ((alignment_entry == 0) ||
            (alignment_entry ==
             3)) {  //Match or mismatch, need to update the new move table and move the cursor of the old move table.
            new_moves.push_back(1);  // We have a match so we need a 1
            new_move_cursor++;
            old_move_cursor++;

            while (moves[old_move_cursor] == 0) {
                if (old_move_cursor < (new_move_cursor + old_moves_offset)) {
                    old_move_cursor++;
                } else {
                    new_moves.push_back(0);
                    new_move_cursor++;
                    old_move_cursor++;
                }
                // Unless there's a new/old mismatch - in which case we need to catch up by adding 1s. TODO this later.
            }
            // Update the Query and target seq cursors
            query_seq_cursor++;
            target_seq_cursor++;
        } else if (alignment_entry == 1) {  //Insertion to target
            // If we have an insertion in the target, we need to add a 1 to the new move table, and increment the new move table cursor. the old move table cursor and new are now out of sync and need fixing.
            new_moves.push_back(1);
            new_move_cursor++;
            target_seq_cursor++;
        } else if (alignment_entry == 2) {  //Insertion to Query
            // We have a query insertion, all we need to do is add zeros to the new move table to make it up, the signal can be assigned to the leftmost nucleotide in the sequence.
            new_moves.push_back(0);
            new_move_cursor++;
            old_move_cursor++;
            while (moves[old_move_cursor] == 0) {
                new_moves.push_back(0);
                old_move_cursor++;
                new_move_cursor++;
            }
            // Update the Query and target seq cursors
            query_seq_cursor++;
        }
    }

    edlibFreeAlignResult(edlib_result);

    // Need to return:
    // 1. Moves start
    // 2. Target sequence Start
    // 3. Moves end
    // 3. Target sequence end

    return {old_moves_offset, target_start - 1, new_moves};
}

std::vector<uint64_t> move_cum_sums(const std::vector<uint8_t>& moves) {
    std::vector<uint64_t> ans(moves.size(), 0);
    if (!moves.empty()) {
        ans[0] = moves[0];
    }
    for (size_t i = 1, n = moves.size(); i < n; i++) {
        ans[i] = ans[i - 1] + moves[i];
    }
    return ans;
}

// Multiversioned function dispatch doesn't work across the dorado_lib linking
// boundary.  Without this wrapper, AVX machines still only execute the default
// version.
std::string reverse_complement(const std::string& sequence) {
    NVTX3_FUNC_RANGE();
    return reverse_complement_impl(sequence);
}

const std::vector<int> BaseInfo::BASE_IDS = []() {
    std::vector<int> base_ids(256, -1);
    base_ids['A'] = 0;
    base_ids['C'] = 1;
    base_ids['G'] = 2;
    base_ids['T'] = 3;
    return base_ids;
}();

// Utility function for counting number of trailing base sof a particular type
// in a given read.
int count_trailing_chars(const std::string_view adapter, char c) {
    int count = 0;
    for (size_t i = adapter.length() - 1; i >= 0; i--) {
        if (adapter[i] == c) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

}  // namespace dorado::utils
