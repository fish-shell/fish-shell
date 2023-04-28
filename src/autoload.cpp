#if 0

// The classes responsible for autoloading functions and completions.
#include "config.h"  // IWYU pragma: keep

#include "autoload.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>
#include <vector>

#include "common.h"
#include "env.h"
#include "io.h"
#include "lru.h"
#include "parser.h"
#include "wutil.h"  // IWYU pragma: keep

/// The time before we'll recheck an autoloaded file.
static const int kAutoloadStalenessInterval = 15;

/// Represents a file that we might want to autoload.
namespace {
struct autoloadable_file_t {
    /// The path to the file.
    wcstring path;

    /// The metadata for the file.
    file_id_t file_id;
};
}  // namespace

/// Class representing a cache of files that may be autoloaded.
/// This is responsible for performing cached accesses to a set of paths.
class autoload_file_cache_t {
    /// A timestamp is a monotonic point in time.
    using timestamp_t = std::chrono::time_point<std::chrono::steady_clock>;

    /// The directories from which to load.
    const std::vector<wcstring> dirs_{};

    /// Our LRU cache of checks that were misses.
    /// The key is the command, the  value is the time of the check.
    using misses_lru_cache_t = lru_cache_t<timestamp_t>;
    misses_lru_cache_t misses_cache_;

    /// The set of files that we have returned to the caller, along with the time of the check.
    /// The key is the command (not the path).
    struct known_file_t {
        autoloadable_file_t file;
        timestamp_t last_checked;
    };
    std::unordered_map<wcstring, known_file_t> known_files_;

    /// \return the current timestamp.
    static timestamp_t current_timestamp() { return std::chrono::steady_clock::now(); }

    /// \return whether a timestamp is fresh enough to use.
    static bool is_fresh(timestamp_t then, timestamp_t now);

    /// Attempt to find an autoloadable file by searching our path list for a given command.
    /// \return the file, or none() if none.
    maybe_t<autoloadable_file_t> locate_file(const wcstring &cmd) const;

   public:
    /// Initialize with a set of directories.
    explicit autoload_file_cache_t(std::vector<wcstring> dirs) : dirs_(std::move(dirs)) {}

    /// Initialize with empty directories.
    autoload_file_cache_t() = default;

    /// \return the directories.
    const std::vector<wcstring> &dirs() const { return dirs_; }

    /// Check if a command \p cmd can be loaded.
    /// If \p allow_stale is true, allow stale entries; otherwise discard them.
    /// This returns an autoloadable file, or none() if there is no such file.
    maybe_t<autoloadable_file_t> check(const wcstring &cmd, bool allow_stale = false);

    /// \return true if a command is cached (either as a hit or miss).
    bool is_cached(const wcstring &cmd) const;
};

maybe_t<autoloadable_file_t> autoload_file_cache_t::locate_file(const wcstring &cmd) const {
    // If the command is empty or starts with NULL (i.e. is empty as a path)
    // we'd try to source the *directory*, which exists.
    // So instead ignore these here.
    if (cmd.empty()) return none();
    if (cmd[0] == L'\0') return none();
    // Re-use the storage for path.
    wcstring path;
    for (const wcstring &dir : dirs()) {
        // Construct the path as dir/cmd.fish
        path = dir;
        path += L"/";
        path += cmd;
        path += L".fish";

        file_id_t file_id = file_id_for_path(path);
        if (file_id != kInvalidFileID) {
            // Found it.
            autoloadable_file_t result;
            result.path = std::move(path);
            result.file_id = file_id;
            return result;
        }
    }
    return none();
}

bool autoload_file_cache_t::is_fresh(timestamp_t then, timestamp_t now) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - then);
    return seconds.count() < kAutoloadStalenessInterval;
}

maybe_t<autoloadable_file_t> autoload_file_cache_t::check(const wcstring &cmd, bool allow_stale) {
    // Check hits.
    auto iter = known_files_.find(cmd);
    if (iter != known_files_.end()) {
        if (allow_stale || is_fresh(iter->second.last_checked, current_timestamp())) {
            // Re-use this cached hit.
            return iter->second.file;
        }
        // The file is stale, remove it.
        known_files_.erase(iter);
    }

    // Check misses.
    if (timestamp_t *miss = misses_cache_.get(cmd)) {
        if (allow_stale || is_fresh(*miss, current_timestamp())) {
            // Re-use this cached miss.
            return none();
        }
        // The miss is stale, remove it.
        misses_cache_.evict_node(cmd);
    }

    // We couldn't satisfy this request from the cache. Hit the disk.
    maybe_t<autoloadable_file_t> file = locate_file(cmd);
    if (file.has_value()) {
        auto ins = known_files_.emplace(cmd, known_file_t{*file, current_timestamp()});
        assert(ins.second && "Known files cache should not have contained this cmd");
        (void)ins;
    } else {
        bool ins = misses_cache_.insert(cmd, current_timestamp());
        assert(ins && "Misses cache should not have contained this cmd");
        (void)ins;
    }
    return file;
}

bool autoload_file_cache_t::is_cached(const wcstring &cmd) const {
    return known_files_.count(cmd) > 0 || misses_cache_.contains(cmd);
}

autoload_t::autoload_t(wcstring env_var_name)
    : env_var_name_(std::move(env_var_name)), cache_(make_unique<autoload_file_cache_t>()) {}

autoload_t::autoload_t(autoload_t &&) noexcept = default;
autoload_t::~autoload_t() = default;

void autoload_t::invalidate_cache() {
    auto cache = make_unique<autoload_file_cache_t>(cache_->dirs());
    cache_ = std::move(cache);
}

bool autoload_t::can_autoload(const wcstring &cmd) {
    return cache_->check(cmd, true /* allow stale */).has_value();
}

bool autoload_t::has_attempted_autoload(const wcstring &cmd) { return cache_->is_cached(cmd); }

std::vector<wcstring> autoload_t::get_autoloaded_commands() const {
    std::vector<wcstring> result;
    result.reserve(autoloaded_files_.size());
    for (const auto &kv : autoloaded_files_) {
        result.push_back(kv.first);
    }
    // Sort the output to make it easier to test.
    std::sort(result.begin(), result.end());
    return result;
}

maybe_t<wcstring> autoload_t::resolve_command(const wcstring &cmd, const environment_t &env) {
    if (maybe_t<env_var_t> mvar = env.get(env_var_name_)) {
        return resolve_command(cmd, mvar->as_list());
    } else {
        return resolve_command(cmd, std::vector<wcstring>{});
    }
}

maybe_t<wcstring> autoload_t::resolve_command(const wcstring &cmd,
                                              const std::vector<wcstring> &paths) {
    // Are we currently in the process of autoloading this?
    if (current_autoloading_.count(cmd) > 0) return none();

    // Check to see if our paths have changed. If so, replace our cache.
    // Note we don't have to modify autoloadable_files_. We'll naturally detect if those have
    // changed when we query the cache.
    if (paths != cache_->dirs()) {
        cache_ = make_unique<autoload_file_cache_t>(paths);
    }

    // Do we have an entry to load?
    auto mfile = cache_->check(cmd);
    if (!mfile) return none();

    // Is this file the same as what we previously autoloaded?
    auto iter = autoloaded_files_.find(cmd);
    if (iter != autoloaded_files_.end() && iter->second == mfile->file_id) {
        // The file has been autoloaded and is unchanged.
        return none();
    }

    // We're going to (tell our caller to) autoload this command.
    current_autoloading_.insert(cmd);
    autoloaded_files_[cmd] = mfile->file_id;
    return std::move(mfile->path);
}

void autoload_t::perform_autoload(const wcstring &path, parser_t &parser) {
    // We do the useful part of what exec_subshell does ourselves
    // - we source the file.
    // We don't create a buffer or check ifs or create a read_limit

    wcstring script_source = L"source " + escape_string(path);
    auto prev_statuses = parser.get_last_statuses();
    const cleanup_t put_back([&] { parser.set_last_statuses(prev_statuses); });
    parser.eval(script_source, io_chain_t{});
}
#endif
