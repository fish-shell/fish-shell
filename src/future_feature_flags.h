// Flags to enable upcoming features
#ifndef FISH_FUTURE_FEATURE_FLAGS_H
#define FISH_FUTURE_FEATURE_FLAGS_H

#include <assert.h>

class features_t {
public:
    /// The list of flags.
    enum flag_t {
        /// Whether ^ is supported for stderr redirection.
        stderr_nocaret,

        /// The number of flags.
        flag_count
    };

    /// Return whether a flag is set.
    bool test(flag_t f) const {
        assert(f >= 0 && f < flag_count && "Invalid flag");
        return values[f];
    }

    /// Set a flag.
    void set(flag_t f, bool value) {
        assert(f >= 0 && f < flag_count && "Invalid flag");
        values[f] = value;
    }

    /// Metadata about feature flags.
    struct metadata_t {
        /// The flag itself.
        features_t::flag_t flag;

        /// User-presentable short name of the feature flag.
        const wchar_t *name;

        /// Comma-separated list of feature groups.
        const wchar_t *groups;

        /// User-presentable description of the feature flag.
        const wchar_t *description;
    };

    /// The metadata, indexed by flag.
    static const metadata_t metadata[flag_count];

    /// Return the metadata for a particular name, or nullptr if not found.
    static const struct metadata_t *metadata_for(const wchar_t *name);

   private:
    /// Values for the flags.
    bool values[flag_count] = {};
};

/// Return the global set of features for fish. This is const to prevent accidental mutation.
const features_t &fish_features();

/// Return the global set of features for fish, but mutable. In general fish features should be set
/// at startup only.
features_t &mutable_fish_features();

#endif
