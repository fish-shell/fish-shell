// The utility library for universal variables. Used both by the client library and by the daemon.
#include "config.h"  // IWYU pragma: keep

#include <arpa/inet.h>  // IWYU pragma: keep
#include <errno.h>
#include <fcntl.h>
// We need the sys/file.h for the flock() declaration on Linux but not OS X.
#include <sys/file.h>  // IWYU pragma: keep
// We need the ioctl.h header so we can check if SIOCGIFHWADDR is defined by it so we know if we're
// on a Linux system.
#include <limits.h>
#include <netinet/in.h>  // IWYU pragma: keep
#include <sys/ioctl.h>   // IWYU pragma: keep
#if !defined(__APPLE__) && !defined(__CYGWIN__)
#include <pwd.h>
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __CYGWIN__
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>  // IWYU pragma: keep
#endif
#include <sys/stat.h>
#include <sys/time.h>   // IWYU pragma: keep
#include <sys/types.h>  // IWYU pragma: keep
#include <unistd.h>
#include <wchar.h>

#include <atomic>
#include <map>
#include <string>
#include <type_traits>
#include <utility>

#include "common.h"
#include "env.h"
#include "env_universal_common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "path.h"
#include "utf8.h"
#include "util.h"  // IWYU pragma: keep
#include "wutil.h"
#include "wcstringutil.h"

#if __APPLE__
#define FISH_NOTIFYD_AVAILABLE 1
#include <notify.h>
#endif

#ifdef __HAIKU__
#define _BSD_SOURCE
#include <bsd/ifaddrs.h>
#endif  // Haiku

/// The set command.
#define SET_STR L"SET"

/// The set_export command.
#define SET_EXPORT_STR L"SET_EXPORT"

/// Non-wide version of the set command.
#define SET_MBS "SET"

/// Non-wide version of the set_export command.
#define SET_EXPORT_MBS "SET_EXPORT"

/// Error message.
#define PARSE_ERR L"Unable to parse universal variable message: '%ls'"

/// Small note about not editing ~/.fishd manually. Inserted at the top of all .fishd files.
#define SAVE_MSG "# This file contains fish universal variable definitions.\n"

/// Version for fish 3.0
#define UVARS_VERSION_3_0 "3.0"

/// The different types of messages found in the fishd file.
enum class uvar_message_type_t { set, set_export };

static wcstring get_machine_identifier();

/// return a list of paths where the uvars file has been historically stored.
static wcstring_list_t get_legacy_paths(const wcstring &wdir) {
    wcstring_list_t result;
    result.push_back(wdir + L"/fishd." + get_machine_identifier());
    wcstring hostname_id;
    if (get_hostname_identifier(hostname_id)) {
        result.push_back(wdir + L'/' + hostname_id);
    }
    return result;
}

static maybe_t<wcstring> default_vars_path_directory() {
    wcstring path;
    if (!path_get_config(path)) return none();
    return path;
}

static maybe_t<wcstring> default_vars_path() {
    if (auto path = default_vars_path_directory()) {
        path->append(L"/fish_universal_variables");
        return path;
    }
    return none();
}

/// Test if the message msg contains the command cmd.
static bool match(const wchar_t *msg, const wchar_t *cmd) {
    size_t len = wcslen(cmd);
    if (wcsncasecmp(msg, cmd, len) != 0) return false;

    if (msg[len] && msg[len] != L' ' && msg[len] != L'\t') return false;

    return true;
}

/// The universal variable format has some funny escaping requirements; here we try to be safe.
static bool is_universal_safe_to_encode_directly(wchar_t c) {
    if (c < 32 || c > 128) return false;

    return iswalnum(c) || wcschr(L"/_", c);
}

/// Escape specified string.
static wcstring full_escape(const wcstring &in) {
    wcstring out;
    for (wchar_t c : in) {
        if (is_universal_safe_to_encode_directly(c)) {
            out.push_back(c);
        } else if (c <= (wchar_t)ASCII_MAX) {
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
static bool append_file_entry(uvar_message_type_t type, const wcstring &key_in,
                              const wcstring &val_in, std::string *result, std::string *storage) {
    assert(storage != NULL);
    assert(result != NULL);

    // Record the length on entry, in case we need to back up.
    bool success = true;
    const size_t result_length_on_entry = result->size();

    // Append header like "SET "
    result->append(type == uvar_message_type_t::set ? SET_MBS : SET_EXPORT_MBS);
    result->push_back(' ');

    // Append variable name like "fish_color_cwd".
    if (!valid_var_name(key_in)) {
        debug(0, L"Illegal variable name: '%ls'", key_in.c_str());
        success = false;
    }
    if (success && !append_utf8(key_in, result, storage)) {
        debug(0, L"Could not convert %ls to narrow character string", key_in.c_str());
        success = false;
    }

    // Append ":".
    if (success) {
        result->push_back(':');
    }

    // Append value.
    if (success && !append_utf8(full_escape(val_in), result, storage)) {
        debug(0, L"Could not convert %ls to narrow character string", val_in.c_str());
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
static const wchar_t * const ENV_NULL = L"\x1d";

/// Character used to separate arrays in universal variables file.
/// This is 30, the ASCII record separator.
static const wchar_t UVAR_ARRAY_SEP = 0x1e;

/// Decode a serialized universal variable value into a list.
static wcstring_list_t decode_serialized(const wcstring &val) {
    if (val == ENV_NULL) return {};
    return split_string(val, UVAR_ARRAY_SEP);
}

/// Decode a a list into a serialized universal variable value.
static wcstring encode_serialized(const wcstring_list_t &vals) {
    if (vals.empty()) return ENV_NULL;
    return join_strings(vals, UVAR_ARRAY_SEP);
}

env_universal_t::env_universal_t(wcstring path) : explicit_vars_path(std::move(path)) {}

maybe_t<env_var_t> env_universal_t::get(const wcstring &name) const {
    var_table_t::const_iterator where = vars.find(name);
    if (where != vars.end()) return where->second;
    return none();
}

bool env_universal_t::get_export(const wcstring &name) const {
    bool result = false;
    var_table_t::const_iterator where = vars.find(name);
    if (where != vars.end()) {
        result = where->second.exports();
    }
    return result;
}

void env_universal_t::set_internal(const wcstring &key, wcstring_list_t vals, bool exportv,
                                   bool overwrite) {
    ASSERT_IS_LOCKED(lock);
    if (!overwrite && this->modified.find(key) != this->modified.end()) {
        // This value has been modified and we're not overwriting it. Skip it.
        return;
    }

    env_var_t &entry = vars[key];
    if (entry.exports() != exportv || entry.as_list() != vals) {
        entry.set_vals(std::move(vals));
        entry.set_exports(exportv);

        // If we are overwriting, then this is now modified.
        if (overwrite) {
            this->modified.insert(key);
        }
    }
}

void env_universal_t::set(const wcstring &key, wcstring_list_t vals, bool exportv) {
    scoped_lock locker(lock);
    this->set_internal(key, std::move(vals), exportv, true /* overwrite */);
}

bool env_universal_t::remove_internal(const wcstring &key) {
    ASSERT_IS_LOCKED(lock);
    size_t erased = this->vars.erase(key);
    if (erased > 0) {
        this->modified.insert(key);
    }
    return erased > 0;
}

bool env_universal_t::remove(const wcstring &key) {
    scoped_lock locker(lock);
    return this->remove_internal(key);
}

wcstring_list_t env_universal_t::get_names(bool show_exported, bool show_unexported) const {
    wcstring_list_t result;
    scoped_lock locker(lock);
    var_table_t::const_iterator iter;
    for (iter = vars.begin(); iter != vars.end(); ++iter) {
        const wcstring &key = iter->first;
        const env_var_t &var = iter->second;
        if ((var.exports() && show_exported) || (!var.exports() && show_unexported)) {
            result.push_back(key);
        }
    }
    return result;
}

// Given a variable table, generate callbacks representing the difference between our vars and the
// new vars.
void env_universal_t::generate_callbacks(const var_table_t &new_vars,
                                         callback_data_list_t &callbacks) const {
    // Construct callbacks for erased values.
    for (var_table_t::const_iterator iter = this->vars.begin(); iter != this->vars.end(); ++iter) {
        const wcstring &key = iter->first;

        // Skip modified values.
        if (this->modified.find(key) != this->modified.end()) {
            continue;
        }

        // If the value is not present in new_vars, it has been erased.
        if (new_vars.find(key) == new_vars.end()) {
            callbacks.push_back(callback_data_t(key, none()));
        }
    }

    // Construct callbacks for newly inserted or changed values.
    for (var_table_t::const_iterator iter = new_vars.begin(); iter != new_vars.end(); ++iter) {
        const wcstring &key = iter->first;

        // Skip modified values.
        if (this->modified.find(key) != this->modified.end()) {
            continue;
        }

        // See if the value has changed.
        const env_var_t &new_entry = iter->second;
        var_table_t::const_iterator existing = this->vars.find(key);
        if (existing == this->vars.end() || existing->second.exports() != new_entry.exports() ||
            existing->second != new_entry) {
            // Value has changed.
            callbacks.push_back(callback_data_t(key, new_entry.as_string()));
        }
    }
}

void env_universal_t::acquire_variables(var_table_t &vars_to_acquire) {
    // Copy modified values from existing vars to vars_to_acquire.
    for (const auto &key : this->modified) {
        var_table_t::iterator src_iter = this->vars.find(key);
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
    ASSERT_IS_LOCKED(lock);
    assert(fd >= 0);
    // Get the dev / inode.
    const file_id_t current_file = file_id_for_fd(fd);
    if (current_file == last_read_file) {
        debug(5, L"universal log sync elided based on fstat()");
    } else {
        // Read a variables table from the file.
        var_table_t new_vars = this->read_message_internal(fd);

        // Announce changes.
        this->generate_callbacks(new_vars, callbacks);

        // Acquire the new variables.
        this->acquire_variables(new_vars);
        last_read_file = current_file;
    }
}

bool env_universal_t::load_from_path(const wcstring &path, callback_data_list_t &callbacks) {
    ASSERT_IS_LOCKED(lock);

    // Check to see if the file is unchanged. We do this again in load_from_fd, but this avoids
    // opening the file unnecessarily.
    if (last_read_file != kInvalidFileID && file_id_for_path(path) == last_read_file) {
        debug(5, L"universal log sync elided based on fast stat()");
        return true;
    }

    bool result = false;
    int fd = wopen_cloexec(path, O_RDONLY);
    if (fd >= 0) {
        debug(5, L"universal log reading from file");
        this->load_from_fd(fd, callbacks);
        close(fd);
        result = true;
    }
    return result;
}

/// Writes our state to the fd. path is provided only for error reporting.
bool env_universal_t::write_to_fd(int fd, const wcstring &path) {
    ASSERT_IS_LOCKED(lock);
    assert(fd >= 0);
    bool success = true;

    // Stuff we output to fd.
    std::string contents;

    // Temporary storage.
    std::string storage;

    // Write the save message. If this fails, we don't bother complaining.
    write_loop(fd, SAVE_MSG, strlen(SAVE_MSG));

    var_table_t::const_iterator iter = vars.begin();
    while (iter != vars.end()) {
        // Append the entry. Note that append_file_entry may fail, but that only affects one
        // variable; soldier on.
        const wcstring &key = iter->first;
        const env_var_t &var = iter->second;
        append_file_entry(
            var.exports() ? uvar_message_type_t::set_export : uvar_message_type_t::set, key,
            encode_serialized(var.as_list()), &contents, &storage);

        // Go to next.
        ++iter;

        // Flush if this is the last iteration or we exceed a page.
        if (iter == vars.end() || contents.size() >= 4096) {
            if (write_loop(fd, contents.data(), contents.size()) < 0) {
                const char *error = strerror(errno);
                debug(0, _(L"Unable to write to universal variables file '%ls': %s"), path.c_str(),
                      error);
                success = false;
                break;
            }
            contents.clear();
        }
    }

    // Since we just wrote out this file, it matches our internal state; pretend we read from it.
    this->last_read_file = file_id_for_fd(fd);

    // We don't close the file.
    return success;
}

bool env_universal_t::move_new_vars_file_into_place(const wcstring &src, const wcstring &dst) {
    int ret = wrename(src, dst);
    if (ret != 0) {
        const char *error = strerror(errno);
        debug(0, _(L"Unable to rename file from '%ls' to '%ls': %s"), src.c_str(), dst.c_str(),
              error);
    }
    return ret == 0;
}

bool env_universal_t::initialize(callback_data_list_t &callbacks) {
    scoped_lock locker(lock);
    if (!explicit_vars_path.empty()) {
        return load_from_path(explicit_vars_path, callbacks);
    }

    // Get the variables path; if there is none (e.g. HOME is bogus) it's hopeless.
    auto vars_path = default_vars_path();
    if (!vars_path) return false;

    bool success = load_from_path(*vars_path, callbacks);
    if (!success && errno == ENOENT) {
        // We failed to load, because the file was not found. Attempt to load from our legacy paths.
        if (auto dir = default_vars_path_directory()) {
            for (const wcstring &path : get_legacy_paths(*dir)) {
                if (load_from_path(path, callbacks)) {
                    // Mark every variable as modified.
                    // This tells the uvars to write out the values loaded from the legacy path;
                    // otherwise it will conclude that the values have been deleted since they
                    // aren't present.
                    for (const auto &kv : vars) {
                        modified.insert(kv.first);
                    }
                    success = true;
                    break;
                }
            }
        }
    }
    return success;
}

bool env_universal_t::open_temporary_file(const wcstring &directory, wcstring *out_path,
                                          int *out_fd) {
    // Create and open a temporary file for writing within the given directory. Try to create a
    // temporary file, up to 10 times. We don't use mkstemps because we want to open it CLO_EXEC.
    // This should almost always succeed on the first try.
    assert(!string_suffixes_string(L"/", directory));  //!OCLINT(multiple unary operator)

    bool success = false;
    int saved_errno;
    const wcstring tmp_name_template = directory + L"/fishd.tmp.XXXXXX";

    for (size_t attempt = 0; attempt < 10 && !success; attempt++) {
        char *narrow_str = wcs2str(tmp_name_template);
        int result_fd = fish_mkstemp_cloexec(narrow_str);
        saved_errno = errno;
        success = result_fd != -1;
        *out_fd = result_fd;
        *out_path = str2wcstring(narrow_str);
        free(narrow_str);
    }

    if (!success) {
        const char *error = strerror(saved_errno);
        debug(0, _(L"Unable to open temporary file '%ls': %s"), out_path->c_str(), error);
    }
    return success;
}

/// Check how long the operation took and print a message if it took too long.
/// Returns false if it took too long else true.
static bool check_duration(double start_time) {
    double duration = timef() - start_time;
    if (duration > 0.25) {
        debug(1, _(L"Locking the universal var file took too long (%.3f seconds)."), duration);
        return false;
    }
    return true;
}

/// Try locking the file. Return true if we succeeded else false. This is safe in terms of the
/// fallback function implemented in terms of fcntl: only ever run on the main thread, and protected
/// by the universal variable lock.
static bool lock_uvar_file(int fd) {
    double start_time = timef();
    while (flock(fd, LOCK_EX) == -1) {
        if (errno != EINTR) return false;  // do nothing per issue #2149
    }
    return check_duration(start_time);
}

bool env_universal_t::open_and_acquire_lock(const wcstring &path, int *out_fd) {
    // Attempt to open the file for reading at the given path, atomically acquiring a lock. On BSD,
    // we can use O_EXLOCK. On Linux, we open the file, take a lock, and then compare fstat() to
    // stat(); if they match, it means that the file was not replaced before we acquired the lock.
    //
    // We pass O_RDONLY with O_CREAT; this creates a potentially empty file. We do this so that we
    // have something to lock on.
    static std::atomic<bool> do_locking(true);
    bool needs_lock = true;
    int flags = O_RDWR | O_CREAT;

#ifdef O_EXLOCK
    if (do_locking) {
        flags |= O_EXLOCK;
        needs_lock = false;
    }
#endif

    int fd = -1;
    while (fd == -1) {
        double start_time = timef();
        fd = wopen_cloexec(path, flags, 0644);
        if (fd == -1) {
            if (errno == EINTR) continue;  // signaled; try again
#ifdef O_EXLOCK
            if (do_locking && (errno == ENOTSUP || errno == EOPNOTSUPP)) {
                // Filesystem probably does not support locking. Clear the flag and try again. Note
                // that we try taking the lock via flock anyways. Note that on Linux the two errno
                // symbols have the same value but on BSD they're different.
                flags &= ~O_EXLOCK;
                needs_lock = true;
                continue;
            }
#endif
            const char *error = strerror(errno);
            debug(0, _(L"Unable to open universal variable file '%ls': %s"), path.c_str(), error);
            break;
        }

        assert(fd >= 0);  // if we get here, we must have a valid fd
        if (!needs_lock && do_locking) {
            do_locking = check_duration(start_time);
        }

        // Try taking the lock, if necessary. If we failed, we may be on lockless NFS, etc.; in that
        // case we pretend we succeeded. See the comment in save_to_path for the rationale.
        if (needs_lock && do_locking) {
            do_locking = lock_uvar_file(fd);
        }

        // Hopefully we got the lock. However, it's possible the file changed out from under us
        // while we were waiting for the lock. Make sure that didn't happen.
        if (file_id_for_fd(fd) != file_id_for_path(path)) {
            // Oops, it changed! Try again.
            close(fd);
            fd = -1;
        }
    }

    *out_fd = fd;
    return fd >= 0;
}

// Returns true if modified variables were written, false if not. (There may still be variable
// changes due to other processes on a false return).
bool env_universal_t::sync(callback_data_list_t &callbacks) {
    debug(5, L"universal log sync");
    scoped_lock locker(lock);
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
    wcstring vars_path = explicit_vars_path;
    if (vars_path.empty()) {
        if (auto default_path = default_vars_path()) {
            vars_path = default_path.acquire();
        }
    }
    if (vars_path.empty()) {
        debug(2, L"No universal variable path available");
        return false;
    }

    // If we have no changes, just load.
    if (modified.empty()) {
        this->load_from_path(vars_path, callbacks);
        debug(5, L"universal log no modifications");
        return false;
    }

    const wcstring directory = wdirname(vars_path);
    bool success = true;
    int vars_fd = -1;
    int private_fd = -1;
    wcstring private_file_path;

    debug(5, L"universal log performing full sync");

    // Open the file.
    if (success) {
        success = this->open_and_acquire_lock(vars_path, &vars_fd);
        if (!success) debug(5, L"universal log open_and_acquire_lock() failed");
    }

    // Read from it.
    if (success) {
        assert(vars_fd >= 0);
        this->load_from_fd(vars_fd, callbacks);
    }

    // Open adjacent temporary file.
    if (success) {
        success = this->open_temporary_file(directory, &private_file_path, &private_fd);
        if (!success) debug(5, L"universal log open_temporary_file() failed");
    }

    // Write to it.
    if (success) {
        assert(private_fd >= 0);
        success = this->write_to_fd(private_fd, private_file_path);
        if (!success) debug(5, L"universal log write_to_fd() failed");
    }

    if (success) {
        // Ensure we maintain ownership and permissions (#2176).
        struct stat sbuf;
        if (wstat(vars_path, &sbuf) >= 0) {
            if (fchown(private_fd, sbuf.st_uid, sbuf.st_gid) == -1)
                debug(5, L"universal log fchown() failed");
            if (fchmod(private_fd, sbuf.st_mode) == -1) debug(5, L"universal log fchmod() failed");
        }

// Linux by default stores the mtime with low precision, low enough that updates that occur in quick
// succession may result in the same mtime (even the nanoseconds field). So manually set the mtime
// of the new file to a high-precision clock. Note that this is only necessary because Linux
// aggressively reuses inodes, causing the ABA problem; on other platforms we tend to notice the
// file has changed due to a different inode (or file size!)
//
// It's probably worth finding a simpler solution to this. The tests ran into this, but it's
// unlikely to affect users.
#if HAVE_CLOCK_GETTIME && HAVE_FUTIMENS
        struct timespec times[2] = {};
        times[0].tv_nsec = UTIME_OMIT;  // don't change ctime
        if (0 == clock_gettime(CLOCK_REALTIME, &times[1])) {
            futimens(private_fd, times);
        }
#endif

        // Apply new file.
        success = this->move_new_vars_file_into_place(private_file_path, vars_path);
        if (!success) debug(5, L"universal log move_new_vars_file_into_place() failed");
    }

    if (success) {
        // Since we moved the new file into place, clear the path so we don't try to unlink it.
        private_file_path.clear();
    }

    // Clean up.
    if (vars_fd >= 0) {
        close(vars_fd);
    }
    if (private_fd >= 0) {
        close(private_fd);
    }
    if (!private_file_path.empty()) {
        wunlink(private_file_path);
    }

    if (success) {
        // All of our modified variables have now been written out.
        modified.clear();
    }

    return success;
}

var_table_t env_universal_t::read_message_internal(int fd) {
    var_table_t result;

    // Temp value used to avoid repeated allocations.
    wcstring storage;

    // The line we construct (and then parse).
    std::string line;
    wcstring wide_line;
    for (;;) {
        // Read into a buffer. Note this is NOT null-terminated!
        char buffer[1024];
        ssize_t amt = read_loop(fd, buffer, sizeof buffer);
        if (amt <= 0) {
            break;
        }
        const size_t bufflen = (size_t)amt;

        // Walk over it by lines. The contents of an unterminated line will be left in 'line' for
        // the next iteration.
        ssize_t line_start = 0;
        while (line_start < amt) {
            // Run until we hit a newline.
            size_t cursor = line_start;
            while (cursor < bufflen && buffer[cursor] != '\n') {
                cursor++;
            }

            // Copy over what we read.
            line.append(buffer + line_start, cursor - line_start);

            // Process it if it's a newline (which is true if we are before the end of the buffer).
            if (cursor < bufflen && !line.empty()) {
                if (utf8_to_wchar(line.data(), line.size(), &wide_line, 0)) {
                    env_universal_t::parse_message_2x_internal(wide_line, &result, &storage);
                }
                line.clear();
            }

            // Skip over the newline (or skip past the end).
            line_start = cursor + 1;
        }
    }

    // We make no effort to handle an unterminated last line.
    return result;
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
        if (!strcmp(versionbuf, UVARS_VERSION_3_0)) {
            return uvar_format_t::fish_3_0;
        } else {
            // Unknown future version.
            return uvar_format_t::future;
        }
    }
    // No version found, assume 2.x
    return uvar_format_t::fish_2_x;
}

void env_universal_t::populate_variables(const std::string &s, var_table_t *out_vars) {
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
}

/// Parse message msg per fish 3.0 format.
void env_universal_t::parse_message_30_internal(const wcstring &msgstr, var_table_t *vars,
                                                wcstring *storage) {
    // TODO.
}

/// Parse message msg per fish 2.x format.
void env_universal_t::parse_message_2x_internal(const wcstring &msgstr, var_table_t *vars,
                                             wcstring *storage) {
    const wchar_t *msg = msgstr.c_str();

    // debug(3, L"parse_message( %ls );", msg);
    if (msg[0] == L'#') return;

    bool is_set_export = match(msg, SET_EXPORT_STR);
    bool is_set = !is_set_export && match(msg, SET_STR);
    if (is_set || is_set_export) {
        const wchar_t *name, *tmp;
        const bool exportv = is_set_export;

        name = msg + (exportv ? wcslen(SET_EXPORT_STR) : wcslen(SET_STR));
        while (name[0] == L'\t' || name[0] == L' ') name++;

        tmp = wcschr(name, L':');
        if (tmp) {
            // Use 'storage' to hold our key to avoid allocations.
            storage->assign(name, tmp - name);
            const wcstring &key = *storage;

            wcstring val;
            if (unescape_string(tmp + 1, &val, 0)) {
                env_var_t &entry = (*vars)[key];
                entry.set_exports(exportv);
                entry.set_vals(decode_serialized(val));
            }
        } else {
            debug(1, PARSE_ERR, msg);
        }
    } else {
        debug(1, PARSE_ERR, msg);
    }
}

/// Maximum length of hostname. Longer hostnames are truncated.
#define HOSTNAME_LEN 32

/// Length of a MAC address.
#define MAC_ADDRESS_MAX_LEN 6

// Thanks to Jan Brittenson, http://lists.apple.com/archives/xcode-users/2009/May/msg00062.html
#ifdef SIOCGIFHWADDR

// Linux
#include <net/if.h>
#include <sys/socket.h>
static bool get_mac_address(unsigned char macaddr[MAC_ADDRESS_MAX_LEN],
                            const char *interface = "eth0") {
    bool result = false;
    const int dummy = socket(AF_INET, SOCK_STREAM, 0);
    if (dummy >= 0) {
        struct ifreq r;
        strncpy((char *)r.ifr_name, interface, sizeof r.ifr_name - 1);
        r.ifr_name[sizeof r.ifr_name - 1] = 0;
        if (ioctl(dummy, SIOCGIFHWADDR, &r) >= 0) {
            memcpy(macaddr, r.ifr_hwaddr.sa_data, MAC_ADDRESS_MAX_LEN);
            result = true;
        }
        close(dummy);
    }
    return result;
}

#elif defined(HAVE_GETIFADDRS)

// OS X and BSD
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <sys/socket.h>
static bool get_mac_address(unsigned char macaddr[MAC_ADDRESS_MAX_LEN],
                            const char *interface = "en0") {
    // BSD, Mac OS X
    struct ifaddrs *ifap;
    bool ok = false;

    if (getifaddrs(&ifap) != 0) {
        return ok;
    }

    for (const ifaddrs *p = ifap; p; p = p->ifa_next) {
        bool is_af_link = p->ifa_addr && p->ifa_addr->sa_family == AF_LINK;
        if (is_af_link && p->ifa_name && p->ifa_name[0] &&
            !strcmp((const char *)p->ifa_name, interface)) {
            const sockaddr_dl &sdl = *reinterpret_cast<sockaddr_dl *>(p->ifa_addr);

            size_t alen = sdl.sdl_alen;
            if (alen > MAC_ADDRESS_MAX_LEN) alen = MAC_ADDRESS_MAX_LEN;
            memcpy(macaddr, sdl.sdl_data + sdl.sdl_nlen, alen);
            ok = true;
            break;
        }
    }
    freeifaddrs(ifap);
    return ok;
}

#else

// Unsupported
static bool get_mac_address(unsigned char macaddr[MAC_ADDRESS_MAX_LEN]) { return false; }

#endif

/// Function to get an identifier based on the hostname.
bool get_hostname_identifier(wcstring &result) {
    //The behavior of gethostname if the buffer size is insufficient differs by implementation and libc version
    //Work around this by using a "guaranteed" sufficient buffer size then truncating the result.
    bool success = false;
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        result.assign(str2wcstring(hostname));
        result.assign(truncate(result, HOSTNAME_LEN));
        success = true;
    }
    return success;
}

/// Get a sort of unique machine identifier. Prefer the MAC address; if that fails, fall back to the
/// hostname; if that fails, pick something.
static wcstring get_machine_identifier() {
    wcstring result;
    unsigned char mac_addr[MAC_ADDRESS_MAX_LEN] = {};
    if (get_mac_address(mac_addr)) {
        result.reserve(2 * MAC_ADDRESS_MAX_LEN);
        for (size_t i = 0; i < MAC_ADDRESS_MAX_LEN; i++) {
            append_format(result, L"%02x", mac_addr[i]);
        }
    } else if (!get_hostname_identifier(result)) {
        result.assign(L"nohost");  // fallback to a dummy value
    }
    return result;
}

class universal_notifier_shmem_poller_t : public universal_notifier_t {
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
    long long last_change_time;
    uint32_t last_seed;
    volatile universal_notifier_shmem_t *region;

    void open_shmem() {
        assert(region == NULL);

        // Use a path based on our uid to avoid collisions.
        char path[NAME_MAX];
        snprintf(path, sizeof path, "/%ls_shmem_%d", program_name ? program_name : L"fish",
                 getuid());

        bool errored = false;
        int fd = shm_open(path, O_RDWR | O_CREAT, 0600);
        if (fd < 0) {
            const char *error = strerror(errno);
            debug(0, _(L"Unable to open shared memory with path '%s': %s"), path, error);
            errored = true;
        }

        // Get the size.
        off_t size = 0;
        if (!errored) {
            struct stat buf = {};
            if (fstat(fd, &buf) < 0) {
                const char *error = strerror(errno);
                debug(0, _(L"Unable to fstat shared memory object with path '%s': %s"), path,
                      error);
                errored = true;
            }
            size = buf.st_size;
        }

        // Set the size, if it's too small.
        bool set_size = !errored && size < (off_t)sizeof(universal_notifier_shmem_t);
        if (set_size && ftruncate(fd, sizeof(universal_notifier_shmem_t)) < 0) {
            const char *error = strerror(errno);
            debug(0, _(L"Unable to truncate shared memory object with path '%s': %s"), path, error);
            errored = true;
        }

        // Memory map the region.
        if (!errored) {
            void *addr = mmap(NULL, sizeof(universal_notifier_shmem_t), PROT_READ | PROT_WRITE,
                              MAP_SHARED, fd, 0);
            if (addr == MAP_FAILED) {
                const char *error = strerror(errno);
                debug(0, _(L"Unable to memory map shared memory object with path '%s': %s"), path,
                      error);
                this->region = NULL;
            } else {
                this->region = static_cast<universal_notifier_shmem_t *>(addr);
            }
        }

        // Close the fd, even if the mapping succeeded.
        if (fd >= 0) {
            close(fd);
        }

        // Read the current seed.
        this->poll();
    }

   public:
    // Our notification involves changing the value in our shared memory. In practice, all clients
    // will be in separate processes, so it suffices to set the value to a pid. For testing
    // purposes, however, it's useful to keep them in the same process, so we increment the value.
    // This isn't "safe" in the sense that multiple simultaneous increments may result in one being
    // lost, but it should always result in the value being changed, which is sufficient.
    void post_notification() {
        if (region != NULL) {
            /* Read off the seed */
            uint32_t seed = ntohl(region->universal_variable_seed);  //!OCLINT(constant cond op)

            // Increment it. Don't let it wrap to zero.
            do {
                seed++;
            } while (seed == 0);
            last_seed = seed;

            // Write out our data.
            region->magic = htonl(SHMEM_MAGIC_NUMBER);       //!OCLINT(constant cond op)
            region->version = htonl(SHMEM_VERSION_CURRENT);  //!OCLINT(constant cond op)
            region->universal_variable_seed = htonl(seed);   //!OCLINT(constant cond op)
        }
    }

    universal_notifier_shmem_poller_t() : last_change_time(0), last_seed(0), region(NULL) {
        open_shmem();
    }

    ~universal_notifier_shmem_poller_t() {
        if (region != NULL) {
            // Behold: C++ in all its glory!
            void *address = const_cast<void *>(static_cast<volatile void *>(region));
            if (munmap(address, sizeof(universal_notifier_shmem_t)) < 0) {
                wperror(L"munmap");
            }
        }
    }

    bool poll() {
        bool result = false;
        if (region != NULL) {
            uint32_t seed = ntohl(region->universal_variable_seed);  //!OCLINT(constant cond op)
            if (seed != last_seed) {
                result = true;
                last_seed = seed;
                last_change_time = get_time();
            }
        }
        return result;
    }

    unsigned long usec_delay_between_polls() const {
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
    universal_notifier_shmem_poller_t() {
        DIE("universal_notifier_shmem_poller_t cannot be used on this system");
    }
#endif
};

/// A notifyd-based notifier. Very straightforward.
class universal_notifier_notifyd_t : public universal_notifier_t {
#if FISH_NOTIFYD_AVAILABLE
    int notify_fd;
    int token;
    std::string name;

    void setup_notifyd() {
        // Per notify(3), the user.uid.%d style is only accessible to processes with that uid.
        char local_name[256];
        snprintf(local_name, sizeof local_name, "user.uid.%d.%ls.uvars", getuid(),
                 program_name ? program_name : L"fish");
        name.assign(local_name);

        uint32_t status =
            notify_register_file_descriptor(name.c_str(), &this->notify_fd, 0, &this->token);
        if (status != NOTIFY_STATUS_OK) {
            debug(1, "notify_register_file_descriptor() failed with status %u.", status);
            debug(1, "Universal variable notifications may not be received.");
        }
        if (this->notify_fd >= 0) {
            // Mark us for non-blocking reads, and CLO_EXEC.
            int flags = fcntl(this->notify_fd, F_GETFL, 0);
            if (flags >= 0 && !(flags & O_NONBLOCK)) {
                fcntl(this->notify_fd, F_SETFL, flags | O_NONBLOCK);
            }

            set_cloexec(this->notify_fd);
            // Serious hack: notify_fd is likely the read end of a pipe. The other end is owned by
            // libnotify, which does not mark it as CLO_EXEC (it should!). The next fd is probably
            // notify_fd + 1. Do it ourselves. If the implementation changes and some other FD gets
            // marked as CLO_EXEC, that's probably a good thing.
            set_cloexec(this->notify_fd + 1);
        }
    }

   public:
    universal_notifier_notifyd_t() : notify_fd(-1), token(-1 /* NOTIFY_TOKEN_INVALID */) {
        setup_notifyd();
    }

    ~universal_notifier_notifyd_t() {
        if (token != -1 /* NOTIFY_TOKEN_INVALID */) {
            notify_cancel(token);
        }
    }

    int notification_fd() const { return notify_fd; }

    bool notification_fd_became_readable(int fd) {
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
        return read_something;
    }

    void post_notification() {
        uint32_t status = notify_post(name.c_str());
        if (status != NOTIFY_STATUS_OK) {
            debug(1, "notify_post() failed with status %u. Uvar notifications may not be sent.",
                  status);
        }
    }
#else  // this class isn't valid on this system
   public:
    universal_notifier_notifyd_t() {
        DIE("universal_notifier_notifyd_t cannot be used on this system");
    }
#endif
};

#if !defined(__APPLE__) && !defined(__CYGWIN__)
#define NAMED_PIPE_FLASH_DURATION_USEC (1e5)
#define SUSTAINED_READABILITY_CLEANUP_DURATION_USEC (5 * 1e6)
#endif

// Named-pipe based notifier. All clients open the same named pipe for reading and writing. The
// pipe's readability status is a trigger to enter polling mode.
//
// To post a notification, write some data to the pipe, wait a little while, and then read it back.
//
// To receive a notification, watch for the pipe to become readable. When it does, enter a polling
// mode until the pipe is no longer readable. To guard against the possibility of a shell exiting
// when there is data remaining in the pipe, if the pipe is kept readable too long, clients will
// attempt to read data out of it (to render it no longer readable).
class universal_notifier_named_pipe_t : public universal_notifier_t {
#if !defined(__APPLE__) && !defined(__CYGWIN__)
    int pipe_fd;
    long long readback_time_usec;
    size_t readback_amount;

    bool polling_due_to_readable_fd;
    long long drain_if_still_readable_time_usec;

    void make_pipe(const wchar_t *test_path);

    void drain_excessive_data() {
        // The pipe seems to have data on it, that won't go away. Read a big chunk out of it. We
        // don't read until it's exhausted, because if someone were to pipe say /dev/null, that
        // would cause us to hang!
        size_t read_amt = 64 * 1024;
        void *buff = malloc(read_amt);
        ignore_result(read(this->pipe_fd, buff, read_amt));
        free(buff);
    }

   public:
    universal_notifier_named_pipe_t(const wchar_t *test_path)
        : pipe_fd(-1),
          readback_time_usec(0),
          readback_amount(0),
          polling_due_to_readable_fd(false),
          drain_if_still_readable_time_usec(0) {
        make_pipe(test_path);
    }

    ~universal_notifier_named_pipe_t() override {
        if (pipe_fd >= 0) {
            close(pipe_fd);
        }
    }

    int notification_fd() const override {
        if (polling_due_to_readable_fd) {
            // We are in polling mode because we think our fd is readable. This means that, if we
            // return it to be select()'d on, we'll be called back immediately. So don't return it.
            return -1;
        }
        // We are not in polling mode. Return the fd so it can be watched.
        return pipe_fd;
    }

    bool notification_fd_became_readable(int fd) override {
        // Our fd is readable. We deliberately do not read anything out of it: if we did, other
        // sessions may miss the notification. Instead, we go into "polling mode:" we do not
        // select() on our fd for a while, and sync periodically until the fd is no longer readable.
        // However, if we are the one who posted the notification, we don't sync (until we clean
        // up!)
        UNUSED(fd);
        bool should_sync = false;
        if (readback_time_usec == 0) {
            polling_due_to_readable_fd = true;
            drain_if_still_readable_time_usec =
                get_time() + SUSTAINED_READABILITY_CLEANUP_DURATION_USEC;
            should_sync = true;
        }
        return should_sync;
    }

    void post_notification() override {
        if (pipe_fd >= 0) {
            // We need to write some data (any data) to the pipe, then wait for a while, then read
            // it back. Nobody is expected to read it except us.
            int pid_nbo = htonl(getpid());  //!OCLINT(constant cond op)
            ssize_t amt_written = write(this->pipe_fd, &pid_nbo, sizeof pid_nbo);
            if (amt_written < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                // Very unsual: the pipe is full!
                drain_excessive_data();
            }

            // Now schedule a read for some time in the future.
            this->readback_time_usec = get_time() + NAMED_PIPE_FLASH_DURATION_USEC;
            this->readback_amount += sizeof pid_nbo;
        }
    }

    unsigned long usec_delay_between_polls() const override {
        unsigned long readback_delay = ULONG_MAX;
        if (this->readback_time_usec > 0) {
            // How long until the readback?
            long long now = get_time();
            if (now >= this->readback_time_usec) {
                // Oops, it already passed! Return something tiny.
                readback_delay = 1000;
            } else {
                readback_delay = (unsigned long)(this->readback_time_usec - now);
            }
        }

        unsigned long polling_delay = ULONG_MAX;
        if (polling_due_to_readable_fd) {
            // We're in polling mode. Don't return a value less than our polling interval.
            polling_delay = NAMED_PIPE_FLASH_DURATION_USEC;
        }

        // Now return the smaller of the two values. If we get ULONG_MAX, it means there's no more
        // need to poll; in that case return 0.
        unsigned long result = mini(readback_delay, polling_delay);
        if (result == ULONG_MAX) {
            result = 0;
        }
        return result;
    }

    bool poll() override {
        // Check if we are past the readback time.
        if (this->readback_time_usec > 0 && get_time() >= this->readback_time_usec) {
            // Read back what we wrote. We do nothing with the value.
            while (this->readback_amount > 0) {
                char buff[64];
                size_t amt_to_read = mini(this->readback_amount, sizeof buff);
                ignore_result(read(this->pipe_fd, buff, amt_to_read));
                this->readback_amount -= amt_to_read;
            }
            assert(this->readback_amount == 0);
            this->readback_time_usec = 0;
        }

        // Check to see if we are doing readability polling.
        if (!polling_due_to_readable_fd || pipe_fd < 0) {
            return false;
        }

        // We are polling, so we are definitely going to sync.
        // See if this is still readable.
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(this->pipe_fd, &fds);
        struct timeval timeout = {};
        select(this->pipe_fd + 1, &fds, NULL, NULL, &timeout);
        if (!FD_ISSET(this->pipe_fd, &fds)) {
            // No longer readable, no longer polling.
            polling_due_to_readable_fd = false;
            drain_if_still_readable_time_usec = 0;
        } else {
            // Still readable. If it's been readable for a long time, there is probably
            // lingering data on the pipe.
            if (get_time() >= drain_if_still_readable_time_usec) {
                drain_excessive_data();
            }
        }
        return true;
    }
#else  // this class isn't valid on this system
   public:
    universal_notifier_named_pipe_t(const wchar_t *test_path) {
        static_cast<void>(test_path);
        DIE("universal_notifier_named_pipe_t cannot be used on this system");
    }
#endif
};

universal_notifier_t::notifier_strategy_t universal_notifier_t::resolve_default_strategy() {
#if FISH_NOTIFYD_AVAILABLE
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
    return NULL;
}

// Default implementations.
universal_notifier_t::universal_notifier_t() = default;

universal_notifier_t::~universal_notifier_t() = default;

int universal_notifier_t::notification_fd() const { return -1; }

void universal_notifier_t::post_notification() {}

bool universal_notifier_t::poll() { return false; }

unsigned long universal_notifier_t::usec_delay_between_polls() const { return 0; }

bool universal_notifier_t::notification_fd_became_readable(int fd) {
    UNUSED(fd);
    return false;
}

#if !defined(__APPLE__) && !defined(__CYGWIN__)
/// Returns a "variables" file in the appropriate runtime directory. This is called infrequently and
/// so does not need to be cached.
static wcstring default_named_pipe_path() {
    wcstring result = env_get_runtime_path();
    if (!result.empty()) {
        result.append(L"/fish_universal_variables");
    }
    return result;
}

void universal_notifier_named_pipe_t::make_pipe(const wchar_t *test_path) {
    wcstring vars_path = test_path ? wcstring(test_path) : default_named_pipe_path();
    vars_path.append(L".notifier");
    const std::string narrow_path = wcs2string(vars_path);

    int mkfifo_status = mkfifo(narrow_path.c_str(), 0600);
    if (mkfifo_status == -1 && errno != EEXIST) {
        const char *error = strerror(errno);
        const wchar_t *errmsg = _(L"Unable to make a pipe for universal variables using '%ls': %s");
        debug(0, errmsg, vars_path.c_str(), error);
        pipe_fd = -1;
        return;
    }

    int fd = wopen_cloexec(vars_path, O_RDWR | O_NONBLOCK, 0600);
    if (fd < 0) {
        const char *error = strerror(errno);
        const wchar_t *errmsg = _(L"Unable to open a pipe for universal variables using '%ls': %s");
        debug(0, errmsg, vars_path.c_str(), error);
        pipe_fd = -1;
        return;
    }

    pipe_fd = fd;
}
#endif
