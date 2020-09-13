// Flags to enable upcoming features
#ifndef FISH_FUTURE_FEATURE_FLAGS_H
#define FISH_FUTURE_FEATURE_FLAGS_H

#include <assert.h>

#include <unordered_map>

#include "common.h"

class features_t {
   public:
    /// The list of flags.
    enum flag_t {
        /// Whether ^ is supported for stderr redirection.
        stderr_nocaret,

        /// Whether ? is supported as a glob.
        qmark_noglob,

        /// Whether string replace -r double-unescapes the replacement.
        string_replace_backslash,

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

    /// Parses a comma-separated feature-flag string, updating ourselves with the values.
    /// Feature names or group names may be prefixed with "no-" to disable them.
    /// The special group name "all" may be used for those who like to live on the edge.
    /// Unknown features are silently ignored.
    void set_from_string(const wcstring &str);

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

    /// The singleton shared feature set.
    static features_t global_features;

    features_t();

   private:
    /// Values for the flags.
    bool values[flag_count] = {};
};

/// Return the global set of features for fish. This is const to prevent accidental mutation.
inline const features_t &fish_features() { return features_t::global_features; }

/// Perform a feature test on the global set of features.
inline bool feature_test(features_t::flag_t f) { return fish_features().test(f); }

/// Return the global set of features for fish, but mutable. In general fish features should be set
/// at startup only.
inline features_t &mutable_fish_features() { return features_t::global_features; }

#endif
