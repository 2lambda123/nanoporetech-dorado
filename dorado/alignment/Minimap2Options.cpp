#include "Minimap2Options.h"

#include "minimap2_args.h"
#include "minimap2_wrappers.h"

namespace dorado::alignment {

Minimap2IndexOptions::Minimap2IndexOptions() {
    index_options = std::make_shared<minimap2::IdxOptHolder>();
}

Minimap2MappingOptions::Minimap2MappingOptions() {
    mapping_options = std::make_shared<minimap2::MapOptHolder>();
}

auto comparable_index_options(const Minimap2IndexOptions& options) {
    const auto& mm_opts = options.index_options->get();
    return std::tie(mm_opts.k, mm_opts.w, mm_opts.flag, mm_opts.bucket_bits,
                    mm_opts.mini_batch_size, mm_opts.batch_size, options.junc_bed);
}

bool operator<(const Minimap2IndexOptions& left, const Minimap2IndexOptions& right) {
    return comparable_index_options(left) < comparable_index_options(right);
}

bool operator>(const Minimap2IndexOptions& l, const Minimap2IndexOptions& r) { return r < l; }

bool operator<=(const Minimap2IndexOptions& l, const Minimap2IndexOptions& r) { return !(l > r); }

bool operator>=(const Minimap2IndexOptions& l, const Minimap2IndexOptions& r) { return !(l < r); }

bool operator==(const Minimap2IndexOptions& left, const Minimap2IndexOptions& right) {
    return comparable_index_options(left) == comparable_index_options(right);
}

bool operator!=(const Minimap2IndexOptions& l, const Minimap2IndexOptions& r) { return !(l == r); }

auto comparable_mapping_options(const Minimap2MappingOptions& options) {
    const auto& o = options.mapping_options->get();
    return std::tie(o.flag, o.seed, o.sdust_thres, o.max_qlen, o.bw, o.bw_long, o.max_gap,
                    o.max_gap_ref, o.max_frag_len, o.max_chain_skip, o.max_chain_iter, o.min_cnt,
                    o.min_chain_score, o.chain_gap_scale, o.chain_skip_scale, o.rmq_size_cap,
                    o.rmq_inner_dist, o.rmq_rescue_size, o.rmq_rescue_ratio, o.mask_level,
                    o.mask_len, o.pri_ratio, o.best_n, o.alt_drop, o.a, o.b, o.q, o.e, o.q2, o.e2,
                    o.transition, o.sc_ambi, o.noncan, o.junc_bonus, o.zdrop, o.zdrop_inv,
                    o.end_bonus, o.min_dp_max, o.min_ksw_len, o.anchor_ext_len, o.anchor_ext_shift,
                    o.max_clip_ratio, o.rank_min_len, o.rank_frac, o.pe_ori, o.pe_bonus,
                    o.mid_occ_frac, o.q_occ_frac, o.min_mid_occ, o.max_mid_occ, o.mid_occ,
                    o.max_occ, o.max_max_occ, o.occ_dist, o.mini_batch_size, o.max_sw_mat,
                    o.cap_kalloc);
}

bool operator<(const Minimap2MappingOptions& l, const Minimap2MappingOptions& r) {
    return comparable_mapping_options(l) < comparable_mapping_options(r);
}

bool operator>(const Minimap2MappingOptions& l, const Minimap2MappingOptions& r) { return r < l; }

bool operator<=(const Minimap2MappingOptions& l, const Minimap2MappingOptions& r) {
    return !(l > r);
}

bool operator>=(const Minimap2MappingOptions& l, const Minimap2MappingOptions& r) {
    return !(l < r);
}

bool operator==(const Minimap2MappingOptions& l, const Minimap2MappingOptions& r) {
    return comparable_mapping_options(l) == comparable_mapping_options(r);
}

bool operator!=(const Minimap2MappingOptions& l, const Minimap2MappingOptions& r) {
    return !(l == r);
}

bool operator==(const Minimap2Options& l, const Minimap2Options& r) {
    return static_cast<const Minimap2IndexOptions&>(l) == r &&
           static_cast<const Minimap2MappingOptions&>(l) == r;
}

bool operator!=(const Minimap2Options& l, const Minimap2Options& r) { return !(l == r); }

Minimap2Options create_dflt_options() { return minimap2::process_option_string(""); }

Minimap2Options create_preset_options(const std::string& preset) {
    return minimap2::process_option_string("--mm2-preset " + preset);
}

}  // namespace dorado::alignment