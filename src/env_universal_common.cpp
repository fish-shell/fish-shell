// The utility library for universal variables. Used both by the client library and by the daemon.
#include "config.h"  // IWYU pragma: keep

#include <arpa/inet.h>  // IWYU pragma: keep
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
// We need the sys/file.h for the flock() declaration on Linux but not OS X.
#include <sys/file.h>  // IWYU pragma: keep
// We need the ioctl.h header so we can check if SIOCGIFHWADDR is defined by it so we know if we're
// on a Linux system.
#include <netinet/in.h>  // IWYU pragma: keep
#include <sys/ioctl.h>   // IWYU pragma: keep
#ifdef __CYGWIN__
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>  // IWYU pragma: keep
#endif
#if !defined(__APPLE__) && !defined(__CYGWIN__)
#include <pwd.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>   // IWYU pragma: keep
#include <sys/types.h>  // IWYU pragma: keep

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "common.h"
#include "env.h"
#include "env_universal_common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fd_readable_set.rs.h"
#include "flog.h"
#include "path.h"
#include "utf8.h"
#include "util.h"  // IWYU pragma: keep
#include "wcstringutil.h"
#include "wutil.h"

#ifdef __APPLE__
#define FISH_NOTIFYD_AVAILABLE
#include <notify.h>
#endif

#ifdef __HAIKU__
#define _BSD_SOURCE
#include <bsd/ifaddrs.h>
#endif  // Haiku

/// Error message.
#define PARSE_ERR L"Unable to parse universal variable message: '%ls'"

/// Small note about not editing ~/.fishd manually. Inserted at the top of all .fishd files.
#define SAVE_MSG "# This file contains fish universal variable definitions.\n"

/// Version for fish 3.0
#define UVARS_VERSION_3_0 "3.0"

// Maximum file size we'll read.
static constexpr size_t k_max_read_size = 16 * 1024 * 1024;

// Fields used in fish 2.x uvars.
namespace fish2x_uvars {
namespace {
constexpr const char *SET = "SET";
constexpr const char *SET_EXPORT = "SET_EXPORT";
}  // namespace
}  // namespace fish2x_uvars

// Fields used in fish 3.0 uvars
namespace fish3_uvars {
namespace {
constexpr const char *SETUVAR = "SETUVAR";
constexpr const char *EXPORT = "--export";
constexpr const char *PATH = "--path";
}  // namespace
}  // namespace fish3_uvars

/// The different types of messages found in the fishd file.
enum class uvar_message_type_t { set, set_export };

static maybe_t<wcstring> default_vars_path_directory() {
    wcstring path;
    if (!path_get_config(path)) return none();
    return path;
}

/// \return the default variable path, or an empty string on failure.
static wcstring default_vars_path() {
    if (auto path = default_vars_path_directory()) {
        path->append(L"/fish_variables");
        return path.acquire();
    }
    return wcstring{};
}

/// Test if the message msg contains the command cmd.
/// On success, updates the cursor to just past the command.
static bool match(const wchar_t **inout_cursor, const char *cmd) {
    const wchar_t *cursor = *inout_cursor;
    size_t len = std::strlen(cmd);
    if (!std::equal(cmd, cmd + len, cursor)) {
        return false;
    }
    if (cursor[len] && cursor[len] != L' ' && cursor[len] != L'\t') return false;
    *inout_cursor = cursor + len;
    return true;
}

/// The universal variable format has some funny escaping requirements; here we try to be safe.
static bool is_universal_safe_to_encode_directly(wchar_t c) {
    if (c < 32 || c > 128) return false;

    return iswalnum(c) || std::wcschr(L"/_", c);
}

/// Escape specified string.
static wcstring full_escape(const wcstring &in) {
    wcstring out;
    for (wchar_t c : in) {
        if (is_universal_safe_to_encode_directly(c)) {
            out.push_back(c);
        } else if (c <= static_cast<wchar_t>(ASCII_MAX)) {
            // See #1225 for discussion of use of ASCII_MAX here.
            append_format(out, L"\\x%.2x", c);
        } else if (c < 65536) {
            append_format(out, L"\\u%.4x", c);
        } else {
            append_format(out, L"\\U%.8x", c);
        }
    }
    return out;
}

/// Converts input to UTF-8 and appends it to receiver, using storage as temp storage.
static bool append_utf8(const wcstring &input, std::string *receiver, std::string *storage) {
    bool result = false;
    if (wchar_to_utf8_string(input, storage)) {
        receiver->append(*storage);
        result = true;
    }
    return result;
}

/// Creates a file entry like "SET fish_color_cwd:FF0". Appends the result to *result (as UTF8).
/// Returns true on success. storage may be used for temporary storage, to avoid allocations.
static bool append_file_entry(env_var_t::env_var_flags_t flags, const wcstring &key_in,
                              const wcstring &val_in, std::string *result, std::string *storage) {
    namespace f3 = fish3_uvars;
    assert(storage != nullptr);
    assert(result != nullptr);

    // Record the length on entry, in case we need to back up.
    bool success = true;
    const size_t result_length_on_entry = result->size();

    // Append SETVAR header.
    result->append(f3::SETUVAR);
    result->push_back(' ');

    // Append flags.
    if (flags & env_var_t::flag_export) {
        result->append(f3::EXPORT);
        result->push_back(' ');
    }
    if (flags & env_var_t::flag_pathvar) {
        result->append(f3::PATH);
        result->push_back(' ');
    }

    // Append variable name like "fish_color_cwd".
    if (!valid_var_name(key_in)) {
        FLOGF(error, L"Illegal variable name: '%ls'", key_in.c_str());
        success = false;
    }
    if (success && !append_utf8(key_in, result, storage)) {
        FLOGF(error, L"Could not convert %ls to narrow character string", key_in.c_str());
        success = false;
    }

    // Append ":".
    if (success) {
        result->push_back(':');
    }

    // Append value.
    if (success && !append_utf8(full_escape(val_in), result, storage)) {
        FLOGF(error, L"Could not convert %ls to narrow character string", val_in.c_str());
        success = false;
    }

    // Append newline.
    if (success) {
        result->push_back('\n');
    }

    // Don't modify result on failure. It's sufficient to simply resize it since all we ever did was
    // append to it.
    if (!success) {
        result->resize(result_length_on_entry);
    }

    return success;
}

/// Encoding of a null string.
static const wchar_t *const ENV_NULL = L"\x1d";

/// Character used to separate arrays in universal variables file.
/// This is 30, the ASCII record separator.
static const wchar_t UVAR_ARRAY_SEP = 0x1e;

/// Decode a serialized universal variable value into a list.
static std::vector<wcstring> decode_serialized(const wcstring &val) {
    if (val == ENV_NULL) return {};
    return split_string(val, UVAR_ARRAY_SEP);
}

/// Decode a a list into a serialized universal variable value.
static wcstring encode_serialized(const std::vector<wcstring> &vals) {
    if (vals.empty()) return ENV_NULL;
    return join_strings(vals, UVAR_ARRAY_SEP);
}

maybe_t<env_var_t> env_universal_t::get(const wcstring &name) const {
    auto where = vars.find(name);
    if (where != vars.end()) return where->second;
    return none();
}

std::unique_ptr<env_var_t> env_universal_t::get_ffi(const wcstring &name) const {
    if (auto var = this->get(name)) {
        return make_unique<env_var_t>(var.acquire());
    } else {
        return nullptr;
    }
}

maybe_t<env_var_t::env_var_flags_t> env_universal_t::get_flags(const wcstring &name) const {
    auto where = vars.find(name);
    if (where != vars.end()) {
        return where->second.get_flags();
    }
    return none();
}

void env_universal_t::set(const wcstring &key, const env_var_t &var) {
    bool new_entry = vars.count(key) == 0;
    env_var_t &entry = vars[key];
    if (new_entry || entry != var) {
        entry = var;
        this->modified.insert(key);
        if (entry.exports()) export_generation += 1;
    }
}

bool env_universal_t::remove(const wcstring &key) {
    auto iter = this->vars.find(key);
    if (iter != this->vars.end()) {
        if (iter->second.exports()) export_generation += 1;
        this->vars.erase(iter);
        this->modified.insert(key);
        return true;
    }
    return false;
}

std::vector<wcstring> env_universal_t::get_names(bool show_exported, bool show_unexported) const {
    std::vector<wcstring> result;
    for (const auto &kv : vars) {
        const wcstring &key = kv.first;
        const env_var_t &var = kv.second;
        if ((var.exports() && show_exported) || (!var.exports() && show_unexported)) {
            result.push_back(key);
        }
    }
    return result;
}

// Given a variable table, generate callbacks representing the difference between our vars and the
// new vars. Update our exports generation.
void env_universal_t::generate_callbacks_and_update_exports(const var_table_t &new_vars,
                                                            callback_data_list_t &callbacks) {
    // Construct callbacks for erased values.
    for (const auto &kv : this->vars) {
        const wcstring &key = kv.first;
        // Skip modified values.
        if (this->modified.count(key)) {
            continue;
        }

        // If the value is not present in new_vars, it has been erased.
        if (new_vars.count(key) == 0) {
            callbacks.push_back(callback_data_t(key, none()));
            if (kv.second.exports()) export_generation += 1;
        }
    }

    // Construct callbacks for newly inserted or changed values.
    for (const auto &kv : new_vars) {
        const wcstring &key = kv.first;

        // Skip modified values.
        if (this->modified.find(key) != this->modified.end()) {
            continue;
        }

        // See if the value has changed.
        const env_var_t &new_entry = kv.second;
        var_table_t::const_iterator existing = this->vars.find(key);

        bool old_exports = (existing != this->vars.end() && existing->second.exports());
        bool export_changed = (old_exports != new_entry.exports());
        bool value_changed = existing != this->vars.end() && existing->second != new_entry;
        if (export_changed || value_changed) {
            export_generation += 1;
        }
        if (existing == this->vars.end() || export_changed || value_changed) {
            // Value is set for the first time, or has changed.
            callbacks.push_back(callback_data_t(key, new_entry));
        }
    }
}

void env_universal_t::acquire_variables(var_table_t &&vars_to_acquire) {
    // Copy modified values from existing vars to vars_to_acquire.
    for (const auto &key : this->modified) {
        auto src_iter = this->vars.find(key);
        if (src_iter == this->vars.end()) {
            /* The value has been deleted. */
            vars_to_acquire.erase(key);
        } else {
            // The value has been modified. Copy it over. Note we can destructively modify the
            // source entry in vars since we are about to get rid of this->vars entirely.
            env_var_t &src = src_iter->second;
            env_var_t &dst = vars_to_acquire[key];
            dst = src;
        }
    }

    // We have constructed all the callbacks and updated vars_to_acquire. Acquire it!
    this->vars = std::move(vars_to_acquire);
}

void env_universal_t::load_from_fd(int fd, callback_data_list_t &callbacks) {
    assert(fd >= 0);
    // Get the dev / inode.
    const file_id_t current_file = file_id_for_fd(fd);
    if (current_file == last_read_file) {
        FLOGF(uvar_file, L"universal log sync elided based on fstat()");
    } else {
        // Read a variables table from the file.
        var_table_t new_vars;
        uvar_format_t format = this->read_message_internal(fd, &new_vars);

        // Hacky: if the read format is in the future, avoid overwriting the file: never try to
        // save.
        if (format == uvar_format_t::future) {
            ok_to_save = false;
        }

        // Announce changes and update our exports generation.
        this->generate_callbacks_and_update_exports(new_vars, callbacks);

        // Acquire the new variables.
        this->acquire_variables(std::move(new_vars));
        last_read_file = current_file;
    }
}

bool env_universal_t::load_from_path(const wcstring &path, callback_data_list_t &callbacks) {
    return load_from_path(wcs2zstring(path), callbacks);
}

bool env_universal_t::load_from_path(const std::string &path, callback_data_list_t &callbacks) {
    // Check to see if the file is unchanged. We do this again in load_from_fd, but this avoids
    // opening the file unnecessarily.
    if (last_read_file != kInvalidFileID && file_id_for_path(path) == last_read_file) {
        FLOGF(uvar_file, L"universal log sync elided based on fast stat()");
        return true;
    }

    bool result = false;
    autoclose_fd_t fd{open_cloexec(path, O_RDONLY)};
    if (fd.valid()) {
        FLOGF(uvar_file, L"universal log reading from file");
        this->load_from_fd(fd.fd(), callbacks);
        result = true;
    }
    return result;
}

/// Serialize the contents to a string.
std::string env_universal_t::serialize_with_vars(const var_table_t &vars) {
    std::string storage;
    std::string contents;
    contents.append(SAVE_MSG);
    contents.append("# VERSION: " UVARS_VERSION_3_0 "\n");

    // Preserve legacy behavior by sorting the values first
    using env_pair_t =
        std::pair<std::reference_wrapper<const wcstring>, std::reference_wrapper<const env_var_t>>;
    std::vector<env_pair_t> cloned(vars.begin(), vars.end());
    std::sort(cloned.begin(), cloned.end(), [](const env_pair_t &p1, const env_pair_t &p2) {
        return p1.first.get() < p2.first.get();
    });

    for (const auto &kv : cloned) {
        // Append the entry. Note that append_file_entry may fail, but that only affects one
        // variable; soldier on.
        const wcstring &key = kv.first;
        const env_var_t &var = kv.second;
        append_file_entry(var.get_flags(), key, encode_serialized(var.as_list()), &contents,
                          &storage);
    }
    return contents;
}

/// Writes our state to the fd. path is provided only for error reporting.
bool env_universal_t::write_to_fd(int fd, const wcstring &path) {
    assert(fd >= 0);
    bool success = true;
    std::string contents = serialize_with_vars(vars);
    if (write_loop(fd, contents.data(), contents.size()) < 0) {
        const char *error = std::strerror(errno);
        FLOGF(error, _(L"Unable to write to universal variables file '%ls': %s"), path.c_str(),
              error);
        success = false;
    }

    // Since we just wrote out this file, it matches our internal state; pretend we read from it.
    this->last_read_file = file_id_for_fd(fd);

    // We don't close the file.
    return success;
}

bool env_universal_t::move_new_vars_file_into_place(const wcstring &src, const wcstring &dst) {
    int ret = wrename(src, dst);
    if (ret != 0) {
        const char *error = std::strerror(errno);
        FLOGF(error, _(L"Unable to rename file from '%ls' to '%ls': %s"), src.c_str(), dst.c_str(),
              error);
    }
    return ret == 0;
}

void env_universal_t::initialize_at_path(callback_data_list_t &callbacks, wcstring path) {
    if (path.empty()) return;
    assert(!initialized() && "Already initialized");
    vars_path_ = std::move(path);
    narrow_vars_path_ = wcs2zstring(vars_path_);

    if (load_from_path(narrow_vars_path_, callbacks)) {
        // Successfully loaded from our normal path.
        return;
    }
}

void env_universal_t::initialize(callback_data_list_t &callbacks) {
    // Set do_flock to false immediately if the default variable path is on a remote filesystem.
    // See #7968.
    if (path_get_config_remoteness() == dir_remoteness_t::remote) do_flock = false;
    this->initialize_at_path(callbacks, default_vars_path());
}

autoclose_fd_t env_universal_t::open_temporary_file(const wcstring &directory, wcstring *out_path) {
    // Create and open a temporary file for writing within the given directory. Try to create a
    // temporary file, up to 10 times. We don't use mkstemps because we want to open it CLO_EXEC.
    // This should almost always succeed on the first try.
    assert(!string_suffixes_string(L"/", directory));  //!OCLINT(multiple unary operator)

    int saved_errno = 0;
    const wcstring tmp_name_template = directory + L"/fishd.tmp.XXXXXX";
    autoclose_fd_t result;
    std::string narrow_str;
    for (size_t attempt = 0; attempt < 10 && !result.valid(); attempt++) {
        narrow_str = wcs2zstring(tmp_name_template);
        result.reset(fish_mkstemp_cloexec(&narrow_str[0]));
        saved_errno = errno;
    }
    *out_path = str2wcstring(narrow_str);

    if (!result.valid()) {
        const char *error = std::strerror(saved_errno);
        FLOGF(error, _(L"Unable to open temporary file '%ls': %s"), out_path->c_str(), error);
    }
    return result;
}

/// Try locking the file.
/// \return true on success, false on error.
static bool flock_uvar_file(int fd) {
    double start_time = timef();
    while (flock(fd, LOCK_EX) == -1) {
        if (errno != EINTR) return false;  // do nothing per issue #2149
    }
    double duration = timef() - start_time;
    if (duration > 0.25) {
        FLOGF(warning, _(L"Locking the universal var file took too long (%.3f seconds)."),
              duration);
        return false;
    }
    return true;
}

bool env_universal_t::open_and_acquire_lock(const wcstring &path, autoclose_fd_t *out_fd) {
    // Attempt to open the file for reading at the given path, atomically acquiring a lock. On BSD,
    // we can use O_EXLOCK. On Linux, we open the file, take a lock, and then compare fstat() to
    // stat(); if they match, it means that the file was not replaced before we acquired the lock.
    //
    // We pass O_RDONLY with O_CREAT; this creates a potentially empty file. We do this so that we
    // have something to lock on.
    bool locked_by_open = false;
    int flags = O_RDWR | O_CREAT;

#ifdef O_EXLOCK
    if (do_flock) {
        flags |= O_EXLOCK;
        locked_by_open = true;
    }
#endif

    autoclose_fd_t fd{};
    while (!fd.valid()) {
        fd = autoclose_fd_t{wopen_cloexec(path, flags, 0644)};

        if (!fd.valid()) {
            int err = errno;
            if (err == EINTR) continue;  // signaled; try again

#ifdef O_EXLOCK
            if ((flags & O_EXLOCK) && (err == ENOTSUP || err == EOPNOTSUPP)) {
                // Filesystem probably does not support locking. Give up on locking.
                // Note that on Linux the two errno symbols have the same value but on BSD they're
                // different.
                flags &= ~O_EXLOCK;
                do_flock = false;
                locked_by_open = false;
                continue;
            }
#endif
            FLOGF(error, _(L"Unable to open universal variable file '%s': %s"), path.c_str(),
                  std::strerror(err));
            break;
        }

        assert(fd.valid() && "Should have a valid fd here");

        // Lock if we want to lock and open() didn't do it for us.
        // If flock fails, give up on locking forever.
        if (do_flock && !locked_by_open) {
            if (!flock_uvar_file(fd.fd())) do_flock = false;
        }

        // Hopefully we got the lock. However, it's possible the file changed out from under us
        // while we were waiting for the lock. Make sure that didn't happen.
        if (file_id_for_fd(fd.fd()) != file_id_for_path(path)) {
            // Oops, it changed! Try again.
            fd.close();
        }
    }

    *out_fd = std::move(fd);
    return out_fd->valid();
}

// Returns true if modified variables were written, false if not. (There may still be variable
// changes due to other processes on a false return).
bool env_universal_t::sync(callback_data_list_t &callbacks) {
    if (!initialized()) return false;

    FLOGF(uvar_file, L"universal log sync");
    // Our saving strategy:
    //
    // 1. Open the file, producing an fd.
    // 2. Lock the file (may be combined with step 1 on systems with O_EXLOCK)
    // 3. After taking the lock, check if the file at the given path is different from what we
    // opened. If so, start over.
    // 4. Read from the file. This can be elided if its dev/inode is unchanged since the last read
    // 5. Open an adjacent temporary file
    // 6. Write our changes to an adjacent file
    // 7. Move the adjacent file into place via rename. This is assumed to be atomic.
    // 8. Release the lock and close the file
    //
    // Consider what happens if Process 1 and 2 both do this simultaneously. Can there be data loss?
    // Process 1 opens the file and then attempts to take the lock. Now, either process 1 will see
    // the original file, or process 2's new file. If it sees the new file, we're OK: it's going to
    // read from the new file, and so there's no data loss. If it sees the old file, then process 2
    // must have locked it (if process 1 locks it, switch their roles). The lock will block until
    // process 2 reaches step 7; at that point process 1 will reach step 2, notice that the file has
    // changed, and then start over.
    //
    // It's possible that the underlying filesystem does not support locks (lockless NFS). In this
    // case, we risk data loss if two shells try to write their universal variables simultaneously.
    // In practice this is unlikely, since uvars are usually written interactively.
    //
    // Prior versions of fish used a hard link scheme to support file locking on lockless NFS. The
    // risk here is that if the process crashes or is killed while holding the lock, future
    // instances of fish will not be able to obtain it. This seems to be a greater risk than that of
    // data loss on lockless NFS. Users who put their home directory on lockless NFS are playing
    // with fire anyways.
    // If we have no changes, just load.
    if (modified.empty()) {
        this->load_from_path(narrow_vars_path_, callbacks);
        FLOGF(uvar_file, L"universal log no modifications");
        return false;
    }

    const wcstring directory = wdirname(vars_path_);
    autoclose_fd_t vars_fd{};

    FLOGF(uvar_file, L"universal log performing full sync");

    // Open the file.
    if (!this->open_and_acquire_lock(vars_path_, &vars_fd)) {
        FLOGF(uvar_file, L"universal log open_and_acquire_lock() failed");
        return false;
    }

    // Read from it.
    assert(vars_fd.valid());
    this->load_from_fd(vars_fd.fd(), callbacks);

    if (ok_to_save) {
        return this->save(directory, vars_path_);
    } else {
        return true;
    }
}

// Write our file contents.
// \return true on success, false on failure.
bool env_universal_t::save(const wcstring &directory, const wcstring &vars_path) {
    assert(ok_to_save && "It's not OK to save");

    wcstring private_file_path;

    // Open adjacent temporary file.
    autoclose_fd_t private_fd = this->open_temporary_file(directory, &private_file_path);
    bool success = private_fd.valid();

    if (!success) FLOGF(uvar_file, L"universal log open_temporary_file() failed");

    // Write to it.
    if (success) {
        assert(private_fd.valid());
        success = this->write_to_fd(private_fd.fd(), private_file_path);
        if (!success) FLOGF(uvar_file, L"universal log write_to_fd() failed");
    }

    if (success) {
        wcstring real_path;
        if (auto maybe_real_path = wrealpath(vars_path)) {
            real_path = *maybe_real_path;
        } else {
            real_path = vars_path;
        }

        // Ensure we maintain ownership and permissions (#2176).
        struct stat sbuf;
        if (wstat(real_path, &sbuf) >= 0) {
            if (fchown(private_fd.fd(), sbuf.st_uid, sbuf.st_gid) == -1)
                FLOGF(uvar_file, L"universal log fchown() failed");
            if (fchmod(private_fd.fd(), sbuf.st_mode) == -1)
                FLOGF(uvar_file, L"universal log fchmod() failed");
        }

        // Linux by default stores the mtime with low precision, low enough that updates that occur
        // in quick succession may result in the same mtime (even the nanoseconds field). So
        // manually set the mtime of the new file to a high-precision clock. Note that this is only
        // necessary because Linux aggressively reuses inodes, causing the ABA problem; on other
        // platforms we tend to notice the file has changed due to a different inode (or file size!)
        //
        // The current time within the Linux kernel is cached, and generally only updated on a timer
        // interrupt. So if the timer interrupt is running at 10 milliseconds, the cached time will
        // only be updated once every 10 milliseconds.
        //
        // It's probably worth finding a simpler solution to this. The tests ran into this, but it's
        // unlikely to affect users.
#if defined(UVAR_FILE_SET_MTIME_HACK)
        struct timespec times[2] = {};
        times[0].tv_nsec = UTIME_OMIT;  // don't change ctime
        if (0 == clock_gettime(CLOCK_REALTIME, &times[1])) {
            futimens(private_fd.fd(), times);
        }
#endif

        // Apply new file.
        success = this->move_new_vars_file_into_place(private_file_path, real_path);
        if (!success) FLOGF(uvar_file, L"universal log move_new_vars_file_into_place() failed");
    }

    if (success) {
        // Since we moved the new file into place, clear the path so we don't try to unlink it.
        private_file_path.clear();
    }

    // Clean up.
    if (!private_file_path.empty()) {
        wunlink(private_file_path);
    }
    if (success) {
        // All of our modified variables have now been written out.
        modified.clear();
    }
    return success;
}

uvar_format_t env_universal_t::read_message_internal(int fd, var_table_t *vars) {
    // Read everything from the fd. Put a sane limit on it.
    std::string contents;
    while (contents.size() < k_max_read_size) {
        char buffer[4096];
        ssize_t amt = read_loop(fd, buffer, sizeof buffer);
        if (amt <= 0) {
            break;
        }
        contents.append(buffer, amt);
    }

    // Handle overlong files.
    if (contents.size() >= k_max_read_size) {
        contents.resize(k_max_read_size);
        // Back up to a newline.
        size_t newline = contents.rfind('\n');
        contents.resize(newline == wcstring::npos ? 0 : newline);
    }

    return populate_variables(contents, vars);
}

/// \return the format corresponding to file contents \p s.
uvar_format_t env_universal_t::format_for_contents(const std::string &s) {
    // Walk over leading comments, looking for one like '# version'
    line_iterator_t<std::string> iter{s};
    while (iter.next()) {
        const std::string &line = iter.line();
        if (line.empty()) continue;
        if (line.front() != L'#') {
            // Exhausted leading comments.
            break;
        }
        // Note scanf %s is max characters to write; add 1 for null terminator.
        char versionbuf[64 + 1];
        if (sscanf(line.c_str(), "# VERSION: %64s", versionbuf) != 1) continue;

        // Try reading the version.
        if (!std::strcmp(versionbuf, UVARS_VERSION_3_0)) {
            return uvar_format_t::fish_3_0;
        } else {
            // Unknown future version.
            return uvar_format_t::future;
        }
    }
    // No version found, assume 2.x
    return uvar_format_t::fish_2_x;
}

uvar_format_t env_universal_t::populate_variables(const std::string &s, var_table_t *out_vars) {
    // Decide on the format.
    const uvar_format_t format = format_for_contents(s);

    line_iterator_t<std::string> iter{s};
    wcstring wide_line;
    wcstring storage;
    while (iter.next()) {
        const std::string &line = iter.line();
        // Skip empties and constants.
        if (line.empty() || line.front() == L'#') continue;

        // Convert to UTF8.
        wide_line.clear();
        if (!utf8_to_wchar(line.data(), line.size(), &wide_line, 0)) continue;

        switch (format) {
            case uvar_format_t::fish_2_x:
                env_universal_t::parse_message_2x_internal(wide_line, out_vars, &storage);
                break;
            case uvar_format_t::fish_3_0:
            // For future formats, just try with the most recent one.
            case uvar_format_t::future:
                env_universal_t::parse_message_30_internal(wide_line, out_vars, &storage);
                break;
        }
    }
    return format;
}

static const wchar_t *skip_spaces(const wchar_t *str) {
    while (*str == L' ' || *str == L'\t') str++;
    return str;
}

bool env_universal_t::populate_1_variable(const wchar_t *input, env_var_t::env_var_flags_t flags,
                                          var_table_t *vars, wcstring *storage) {
    const wchar_t *str = skip_spaces(input);
    const wchar_t *colon = std::wcschr(str, L':');
    if (!colon) return false;

    // Parse out the value into storage, and decode it into a variable.
    storage->clear();
    auto unescaped = unescape_string(colon + 1, 0);
    if (!unescaped) {
        return false;
    }
    *storage = *unescaped;
    env_var_t var{decode_serialized(*storage), flags};

    // Parse out the key and write into the map.
    storage->assign(str, colon - str);
    const wcstring &key = *storage;
    (*vars)[key] = std::move(var);
    return true;
}

/// Parse message msg per fish 3.0 format.
void env_universal_t::parse_message_30_internal(const wcstring &msgstr, var_table_t *vars,
                                                wcstring *storage) {
    namespace f3 = fish3_uvars;
    const wchar_t *const msg = msgstr.c_str();
    if (msg[0] == L'#') return;

    const wchar_t *cursor = msg;
    if (!match(&cursor, f3::SETUVAR)) {
        FLOGF(warning, PARSE_ERR, msg);
        return;
    }
    // Parse out flags.
    env_var_t::env_var_flags_t flags = 0;
    for (;;) {
        cursor = skip_spaces(cursor);
        if (*cursor != L'-') break;
        if (match(&cursor, f3::EXPORT)) {
            flags |= env_var_t::flag_export;
        } else if (match(&cursor, f3::PATH)) {
            flags |= env_var_t::flag_pathvar;
        } else {
            // Skip this unknown flag, for future proofing.
            while (*cursor && *cursor != L' ' && *cursor != L'\t') cursor++;
        }
    }

    // Populate the variable with these flags.
    if (!populate_1_variable(cursor, flags, vars, storage)) {
        FLOGF(warning, PARSE_ERR, msg);
    }
}

/// Parse message msg per fish 2.x format.
void env_universal_t::parse_message_2x_internal(const wcstring &msgstr, var_table_t *vars,
                                                wcstring *storage) {
    namespace f2x = fish2x_uvars;
    const wchar_t *const msg = msgstr.c_str();
    const wchar_t *cursor = msg;

    if (cursor[0] == L'#') return;

    env_var_t::env_var_flags_t flags = 0;
    if (match(&cursor, f2x::SET_EXPORT)) {
        flags |= env_var_t::flag_export;
    } else if (match(&cursor, f2x::SET)) {
        flags |= 0;
    } else {
        FLOGF(warning, PARSE_ERR, msg);
        return;
    }

    if (!populate_1_variable(cursor, flags, vars, storage)) {
        FLOGF(warning, PARSE_ERR, msg);
    }
}

/// Maximum length of hostname. Longer hostnames are truncated.
#define HOSTNAME_LEN 255

/// Function to get an identifier based on the hostname.
bool get_hostname_identifier(wcstring &result) {
    // The behavior of gethostname if the buffer size is insufficient differs by implementation and
    // libc version Work around this by using a "guaranteed" sufficient buffer size then truncating
    // the result.
    bool success = false;
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        result.assign(str2wcstring(hostname));
        result.assign(truncate(result, HOSTNAME_LEN));
        // Don't return an empty hostname, we may attempt to open a directory instead.
        success = !result.empty();
    }
    return success;
}

namespace {
class universal_notifier_shmem_poller_t final : public universal_notifier_t {
#ifdef __CYGWIN__
    // This is what our shared memory looks like. Everything here is stored in network byte order
    // (big-endian).
    struct universal_notifier_shmem_t {
        uint32_t magic;
        uint32_t version;
        uint32_t universal_variable_seed;
    };

#define SHMEM_MAGIC_NUMBER 0xF154
#define SHMEM_VERSION_CURRENT 1000

   private:
    long long last_change_time{0};
    uint32_t last_seed{0};
    volatile universal_notifier_shmem_t *region{nullptr};

    void open_shmem() {
        assert(region == nullptr);

        // Use a path based on our uid to avoid collisions.
        char path[NAME_MAX];
        snprintf(path, sizeof path, "/%ls_shmem_%d", program_name ? program_name : L"fish",
                 getuid());

        autoclose_fd_t fd{shm_open(path, O_RDWR | O_CREAT, 0600)};
        if (!fd.valid()) {
            const char *error = std::strerror(errno);
            FLOGF(error, _(L"Unable to open shared memory with path '%s': %s"), path, error);
            return;
        }

        // Get the size.
        off_t size = 0;
        struct stat buf = {};
        if (fstat(fd.fd(), &buf) < 0) {
            const char *error = std::strerror(errno);
            FLOGF(error, _(L"Unable to fstat shared memory object with path '%s': %s"), path,
                  error);
            return;
        }
        size = buf.st_size;

        // Set the size, if it's too small.
        if (size < (off_t)sizeof(universal_notifier_shmem_t)) {
            if (ftruncate(fd.fd(), sizeof(universal_notifier_shmem_t)) < 0) {
                const char *error = std::strerror(errno);
                FLOGF(error, _(L"Unable to truncate shared memory object with path '%s': %s"), path,
                      error);
                return;
            }
        }

        // Memory map the region.
        void *addr = mmap(nullptr, sizeof(universal_notifier_shmem_t), PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd.fd(), 0);
        if (addr == MAP_FAILED) {
            const char *error = std::strerror(errno);
            FLOGF(error, _(L"Unable to memory map shared memory object with path '%s': %s"), path,
                  error);
            this->region = nullptr;
            return;
        }
        this->region = static_cast<universal_notifier_shmem_t *>(addr);

        // Read the current seed.
        this->poll();
    }

   public:
    // Our notification involves changing the value in our shared memory. In practice, all clients
    // will be in separate processes, so it suffices to set the value to a pid. For testing
    // purposes, however, it's useful to keep them in the same process, so we increment the value.
    // This isn't "safe" in the sense that multiple simultaneous increments may result in one being
    // lost, but it should always result in the value being changed, which is sufficient.
    void post_notification() override {
        if (region != nullptr) {
            /* Read off the seed */
            uint32_t seed = ntohl(region->universal_variable_seed);  //!OCLINT(constant cond op)

            // Increment it. Don't let it wrap to zero.
            do {
                seed++;
            } while (seed == 0);

            // Write out our data.
            region->magic = htonl(SHMEM_MAGIC_NUMBER);       //!OCLINT(constant cond op)
            region->version = htonl(SHMEM_VERSION_CURRENT);  //!OCLINT(constant cond op)
            region->universal_variable_seed = htonl(seed);   //!OCLINT(constant cond op)

            FLOGF(uvar_notifier, "posting notification: seed %u -> %u", last_seed, seed);
            last_seed = seed;
        }
    }

    universal_notifier_shmem_poller_t() { open_shmem(); }

    ~universal_notifier_shmem_poller_t() {
        if (region != nullptr) {
            void *address = const_cast<void *>(static_cast<volatile void *>(region));
            if (munmap(address, sizeof(universal_notifier_shmem_t)) < 0) {
                wperror(L"munmap");
            }
        }
    }

    bool poll() override {
        bool result = false;
        if (region != nullptr) {
            uint32_t seed = ntohl(region->universal_variable_seed);  //!OCLINT(constant cond op)
            if (seed != last_seed) {
                result = true;
                FLOGF(uvar_notifier, "polled true: shmem seed change %u -> %u", last_seed, seed);
                last_seed = seed;
                last_change_time = get_time();
            }
        }
        return result;
    }

    unsigned long usec_delay_between_polls() const override {
        // If it's been less than five seconds since the last change, we poll quickly Otherwise we
        // poll more slowly. Note that a poll is a very cheap shmem read. The bad part about making
        // this high is the process scheduling/wakeups it produces.
        long long usec_per_sec = 1000000;
        if (get_time() - last_change_time < 5LL * usec_per_sec) {
            return usec_per_sec / 10;  // 10 times a second
        }
        return usec_per_sec / 3;  // 3 times a second
    }
#else  // this class isn't valid on this system
   public:
    [[noreturn]] universal_notifier_shmem_poller_t() {
        DIE("universal_notifier_shmem_poller_t cannot be used on this system");
    }
#endif
};

/// A notifyd-based notifier. Very straightforward.
class universal_notifier_notifyd_t final : public universal_notifier_t {
#ifdef FISH_NOTIFYD_AVAILABLE
    // Note that we should not use autoclose_fd_t, as notify_cancel() takes responsibility for
    // closing it.
    int notify_fd{-1};
    int token{-1};  // NOTIFY_TOKEN_INVALID
    std::string name{};

    void setup_notifyd() {
        // Per notify(3), the user.uid.%d style is only accessible to processes with that uid.
        char local_name[256];
        snprintf(local_name, sizeof local_name, "user.uid.%d.%ls.uvars", getuid(),
                 program_name ? program_name : L"fish");
        name.assign(local_name);

        uint32_t status =
            notify_register_file_descriptor(name.c_str(), &this->notify_fd, 0, &this->token);

        if (status != NOTIFY_STATUS_OK) {
            FLOGF(warning, "notify_register_file_descriptor() failed with status %u.", status);
            FLOGF(warning, "Universal variable notifications may not be received.");
        }
        if (notify_fd >= 0) {
            // Mark us for non-blocking reads, and CLO_EXEC.
            int flags = fcntl(notify_fd, F_GETFL, 0);
            if (flags >= 0 && !(flags & O_NONBLOCK)) {
                fcntl(notify_fd, F_SETFL, flags | O_NONBLOCK);
            }

            (void)set_cloexec(notify_fd);
            // Serious hack: notify_fd is likely the read end of a pipe. The other end is owned by
            // libnotify, which does not mark it as CLO_EXEC (it should!). The next fd is probably
            // notify_fd + 1. Do it ourselves. If the implementation changes and some other FD gets
            // marked as CLO_EXEC, that's probably a good thing.
            (void)set_cloexec(notify_fd + 1);
        }
    }

   public:
    universal_notifier_notifyd_t() { setup_notifyd(); }

    ~universal_notifier_notifyd_t() {
        if (token != -1 /* NOTIFY_TOKEN_INVALID */) {
            // Note this closes notify_fd.
            notify_cancel(token);
        }
    }

    int notification_fd() const override { return notify_fd; }

    bool notification_fd_became_readable(int fd) override {
        // notifyd notifications come in as 32 bit values. We don't care about the value. We set
        // ourselves as non-blocking, so just read until we can't read any more.
        assert(fd == notify_fd);
        bool read_something = false;
        unsigned char buff[64];
        ssize_t amt_read;
        do {
            amt_read = read(notify_fd, buff, sizeof buff);
            read_something = (read_something || amt_read > 0);
        } while (amt_read == sizeof buff);
        FLOGF(uvar_notifier, "notify fd %s readable", read_something ? "was" : "was not");
        return read_something;
    }

    void post_notification() override {
        FLOG(uvar_notifier, "posting notification");
        uint32_t status = notify_post(name.c_str());
        if (status != NOTIFY_STATUS_OK) {
            FLOGF(warning,
                  "notify_post() failed with status %u. Uvar notifications may not be sent.",
                  status);
        }
    }
#else  // this class isn't valid on this system
   public:
    [[noreturn]] universal_notifier_notifyd_t() {
        DIE("universal_notifier_notifyd_t cannot be used on this system");
    }
#endif
};

/// Returns a "variables" file in the appropriate runtime directory. This is called infrequently and
/// so does not need to be cached.
static wcstring default_named_pipe_path() {
    wcstring result = env_get_runtime_path();
    if (!result.empty()) {
        result.append(L"/fish_universal_variables");
    }
    return result;
}

/// Create a fifo (named pipe) at \p test_path if non-null, or a default runtime path if null.
/// Open the fifo for both reading and writing, in non-blocking mode.
/// \return the fifo, or an invalid fd on failure.
static autoclose_fd_t make_fifo(const wchar_t *test_path, const wchar_t *suffix) {
    wcstring vars_path = test_path ? wcstring(test_path) : default_named_pipe_path();
    vars_path.append(suffix);
    const std::string narrow_path = wcs2zstring(vars_path);

    int mkfifo_status = mkfifo(narrow_path.c_str(), 0600);
    if (mkfifo_status == -1 && errno != EEXIST) {
        const char *error = std::strerror(errno);
        const wchar_t *errmsg = _(L"Unable to make a pipe for universal variables using '%ls': %s");
        FLOGF(error, errmsg, vars_path.c_str(), error);
        return autoclose_fd_t{};
    }

    autoclose_fd_t res{wopen_cloexec(vars_path, O_RDWR | O_NONBLOCK, 0600)};
    if (!res.valid()) {
        const char *error = std::strerror(errno);
        const wchar_t *errmsg = _(L"Unable to open a pipe for universal variables using '%ls': %s");
        FLOGF(error, errmsg, vars_path.c_str(), error);
    }
    return res;
}

// Named-pipe based notifier. All clients open the same named pipe for reading and writing. The
// pipe's readability status is a trigger to enter polling mode.
//
// To post a notification, write some data to the pipe, wait a little while, and then read it back.
//
// To receive a notification, watch for the pipe to become readable. When it does, enter a polling
// mode until the pipe is no longer readable, where we poll based on the modification date of the
// pipe. To guard against the possibility of a shell exiting when there is data remaining in the
// pipe, if the pipe is kept readable too long, clients will attempt to read data out of it (to
// render it no longer readable).
class universal_notifier_named_pipe_t final : public universal_notifier_t {
#if !defined(__CYGWIN__)
    // We operate a state machine.
    enum state_t{
        // The pipe is not yet readable. There is nothing to do in poll.
        // If the pipe becomes readable we will enter the polling state.
        waiting_for_readable,

        // The pipe is readable. In poll, check if the pipe is still readable,
        // and whether its timestamp has changed.
        polling_during_readable,

        // We have written to the pipe (so we expect it to be readable).
        // We may read back from it in poll().
        waiting_to_drain,
    };

    // The state we are currently in.
    state_t state{waiting_for_readable};

    // When we entered that state, in microseconds since epoch.
    long long state_start_usec{-1};

    // The pipe itself; this is opened read/write.
    autoclose_fd_t pipe_fd;

    // The pipe's file ID containing the last modified timestamp.
    file_id_t pipe_timestamps{};

    // If we are in waiting_to_drain state, how much we have written and therefore are responsible
    // for draining.
    size_t drain_amount{0};

    // We "flash" the pipe to make it briefly readable, for this many usec.
    static constexpr long long k_flash_duration_usec = 1e4;

    // If the pipe remains readable for this many usec, we drain it.
    static constexpr long long k_readable_too_long_duration_usec = 1e6;

    /// \return the name of a state.
    static const char *state_name(state_t s) {
        switch (s) {
            case waiting_for_readable:
                return "waiting";
            case polling_during_readable:
                return "polling";
            case waiting_to_drain:
                return "draining";
        }
        DIE("Unreachable");
    }

    // Switch to a state (may or may not be new).
    void set_state(state_t new_state) {
        FLOGF(uvar_notifier, "changing from %s to %s", state_name(state), state_name(new_state));
        state = new_state;
        state_start_usec = get_time();
    }

    // Called when the pipe has been readable for too long.
    void drain_excess() const {
        // The pipe seems to have data on it, that won't go away. Read a big chunk out of it. We
        // don't read until it's exhausted, because if someone were to pipe say /dev/null, that
        // would cause us to hang!
        FLOG(uvar_notifier, "pipe was full, draining it");
        char buff[512];
        ignore_result(read(pipe_fd.fd(), buff, sizeof buff));
    }

    // Called when we want to read back data we have written, to mark the pipe as non-readable.
    void drain_written() {
        while (this->drain_amount > 0) {
            char buff[64];
            size_t amt = std::min(this->drain_amount, sizeof buff);
            ignore_result(read(this->pipe_fd.fd(), buff, amt));
            this->drain_amount -= amt;
        }
    }

    /// Check if the pipe's file ID (aka struct stat) is different from what we have stored.
    /// If it has changed, it indicates that someone has modified the pipe; update our stored id.
    /// \return true if changed, false if not.
    bool update_pipe_timestamps() {
        if (!pipe_fd.valid()) return false;
        file_id_t timestamps = file_id_for_fd(pipe_fd.fd());
        if (timestamps == this->pipe_timestamps) {
            return false;
        }
        this->pipe_timestamps = timestamps;
        return true;
    }

   public:
    explicit universal_notifier_named_pipe_t(const wchar_t *test_path)
        : pipe_fd(make_fifo(test_path, L".notifier")) {}

    ~universal_notifier_named_pipe_t() override = default;

    int notification_fd() const override {
        if (!pipe_fd.valid()) return -1;
        // If we are waiting for the pipe to be readable, return it for select.
        // Otherwise we expect it to be readable already; return invalid.
        switch (state) {
            case waiting_for_readable:
                return pipe_fd.fd();
            case polling_during_readable:
            case waiting_to_drain:
                return -1;
        }
        DIE("unreachable");
    }

    bool notification_fd_became_readable(int fd) override {
        assert(fd == pipe_fd.fd() && "Wrong fd");
        UNUSED(fd);
        switch (state) {
            case waiting_for_readable:
                // We are now readable.
                // Grab the timestamp and return true indicating that we received a notification.
                set_state(polling_during_readable);
                update_pipe_timestamps();
                return true;

            case polling_during_readable:
            case waiting_to_drain:
                // We did not return an fd to wait on, so should not be called.
                DIE("should not be called in this state");
        }
        DIE("unreachable");
    }

    void post_notification() override {
        if (!pipe_fd.valid()) return;
        // We need to write some data (any data) to the pipe, then wait for a while, then read
        // it back. Nobody is expected to read it except us.
        FLOGF(uvar_notifier, "writing to pipe (written %lu)", (unsigned long)drain_amount);
        char c[1] = {'\0'};
        ssize_t amt_written = write(pipe_fd.fd(), c, sizeof c);
        if (amt_written < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            // Very unusual: the pipe is full! Try to read some and repeat once.
            drain_excess();
            amt_written = write(pipe_fd.fd(), c, sizeof c);
            if (amt_written < 0) {
                FLOG(uvar_notifier, "pipe could not be drained, skipping notification");
                return;
            }
            FLOG(uvar_notifier, "pipe drained");
        }
        assert(amt_written >= 0 && "Amount should not be negative");
        this->drain_amount += amt_written;
        // We unconditionally set our state to waiting to drain.
        set_state(waiting_to_drain);
        update_pipe_timestamps();
    }

    unsigned long usec_delay_between_polls() const override {
        if (!pipe_fd.valid()) return 0;
        switch (state) {
            case waiting_for_readable:
                // No polling necessary until it becomes readable.
                return 0;

            case polling_during_readable:
            case waiting_to_drain:
                return k_flash_duration_usec;
        }
        DIE("Unreachable");
    }

    bool poll() override {
        if (!pipe_fd.valid()) return false;
        switch (state) {
            case waiting_for_readable:
                // Nothing to do until the fd is readable.
                return false;

            case polling_during_readable: {
                // If we're no longer readable, go back to wait mode.
                // Conversely, if we have been readable too long, perhaps some fish died while its
                // written data was still on the pipe; drain some.
                if (!poll_fd_readable(pipe_fd.fd())) {
                    set_state(waiting_for_readable);
                } else if (get_time() >= state_start_usec + k_readable_too_long_duration_usec) {
                    drain_excess();
                }
                // Sync if the pipe's timestamp is different, meaning someone modified the pipe
                // since we last saw it.
                if (update_pipe_timestamps()) {
                    FLOG(uvar_notifier, "pipe changed, will sync uvars");
                    return true;
                }
                return false;
            }

            case waiting_to_drain: {
                // We wrote data to the pipe. Maybe read it back.
                // If we are still readable, then there is still data on the pipe; maybe another
                // change occurred with ours.
                if (get_time() >= state_start_usec + k_flash_duration_usec) {
                    drain_written();
                    if (!poll_fd_readable(pipe_fd.fd())) {
                        set_state(waiting_for_readable);
                    } else {
                        set_state(polling_during_readable);
                    }
                }
                return update_pipe_timestamps();
            }
        }
        DIE("Unreachable");
    }

#else  // this class isn't valid on this system
   public:
    universal_notifier_named_pipe_t(const wchar_t *test_path) {
        static_cast<void>(test_path);
        DIE("universal_notifier_named_pipe_t cannot be used on this system");
    }
#endif
};
}  // namespace

universal_notifier_t::notifier_strategy_t universal_notifier_t::resolve_default_strategy() {
#ifdef FISH_NOTIFYD_AVAILABLE
    return strategy_notifyd;
#elif defined(__CYGWIN__)
    return strategy_shmem_polling;
#else
    return strategy_named_pipe;
#endif
}

universal_notifier_t &universal_notifier_t::default_notifier() {
    static std::unique_ptr<universal_notifier_t> result =
        new_notifier_for_strategy(universal_notifier_t::resolve_default_strategy());
    return *result;
}

std::unique_ptr<universal_notifier_t> universal_notifier_t::new_notifier_for_strategy(
    universal_notifier_t::notifier_strategy_t strat, const wchar_t *test_path) {
    switch (strat) {
        case strategy_notifyd: {
            return make_unique<universal_notifier_notifyd_t>();
        }
        case strategy_shmem_polling: {
            return make_unique<universal_notifier_shmem_poller_t>();
        }
        case strategy_named_pipe: {
            return make_unique<universal_notifier_named_pipe_t>(test_path);
        }
    }
    DIE("should never reach this statement");
    return nullptr;
}

// Default implementations.
universal_notifier_t::universal_notifier_t() = default;

universal_notifier_t::~universal_notifier_t() = default;

int universal_notifier_t::notification_fd() const { return -1; }

bool universal_notifier_t::poll() { return false; }

void universal_notifier_t::post_notification() {}

unsigned long universal_notifier_t::usec_delay_between_polls() const { return 0; }

bool universal_notifier_t::notification_fd_became_readable(int fd) {
    UNUSED(fd);
    return false;
}

var_table_ffi_t::var_table_ffi_t(const var_table_t &table) {
    for (const auto &kv : table) {
        this->names.push_back(kv.first);
        this->vars.push_back(kv.second);
    }
}
var_table_ffi_t::~var_table_ffi_t() = default;
