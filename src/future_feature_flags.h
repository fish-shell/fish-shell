// Flags to enable upcoming features
#ifndef FISH_FUTURE_FEATURE_FLAGS_H
#define FISH_FUTURE_FEATURE_FLAGS_H

#include <atomic>

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

        /// Whether "&" is not-special if followed by a word character.
        ampersand_nobg_in_token,

        /// The number of flags.
        flag_count
    };

    /// Return whether a flag is set.
    bool test(flag_t f) const {
        assert(f >= 0 && f < flag_count && "Invalid flag");
        return values[f].load(std::memory_order_relaxed);
    }

    /// Set a flag.
    void set(flag_t f, bool value) {
        assert(f >= 0 && f < flag_count && "Invalid flag");
        values[f].store(value, std::memory_order_relaxed);
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

        /// Default flag value.
        const bool default_value;

        /// Whether the value can still be changed or not.
        const bool read_only;
    };

    /// The metadata, indexed by flag.
    static const metadata_t metadata[flag_count];

    /// Return the metadata for a particular name, or nullptr if not found.
    static const struct metadata_t *metadata_for(const wchar_t *name);

    /// The singleton shared feature set.
    static features_t global_features;

    features_t();

    features_t(const features_t &rhs) { *this = rhs; }

    void operator=(const features_t &rhs) {
        for (int i = 0; i < flag_count; i++) {
            flag_t f = static_cast<flag_t>(i);
            this->set(f, rhs.test(f));
        }
    }

   private:
    // Values for the flags.
    // These are atomic to "fix" a race reported by tsan where tests of feature flags and other
    // tests which use them conceptually race.
    std::atomic<bool> values[flag_count]{};
};

/// Return the global set of features for fish. This is const to prevent accidental mutation.
inline const features_t &fish_features() { return features_t::global_features; }

/// Perform a feature test on the global set of features.
inline bool feature_test(features_t::flag_t f) { return fish_features().test(f); }

/// Return the global set of features for fish, but mutable. In general fish features should be set
/// at startup only.
inline features_t &mutable_fish_features() { return features_t::global_features; }

#endif
