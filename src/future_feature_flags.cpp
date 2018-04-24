#include "config.h"  // IWYU pragma: keep

#include <wchar.h>
#include "future_feature_flags.h"

/// The set of features applying to this instance.
static features_t global_features;

const features_t &fish_features() { return global_features; }

features_t &mutable_fish_features() { return global_features; }

const features_t::metadata_t features_t::metadata[features_t::flag_count] = {
    {stderr_nocaret, L"stderr-nocaret", L"3.0", L"^ no longer redirects stderr"},
};

const struct features_t::metadata_t *features_t::metadata_for(const wchar_t *name) {
    assert(name && "null flag name");
    for (const auto &md : metadata) {
        if (!wcscmp(name, md.name)) return &md;
    }
    return nullptr;
}
