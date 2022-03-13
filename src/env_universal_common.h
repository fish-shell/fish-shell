#ifndef FISH_ENV_UNIVERSAL_COMMON_H
#define FISH_ENV_UNIVERSAL_COMMON_H
#include "config.h"  // IWYU pragma: keep

#include <pthread.h>
#include <stdio.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "common.h"
#include "env.h"
#include "fds.h"
#include "wutil.h"

/// Callback data, reflecting a change in universal variables.
struct callback_data_t {
    // The name of the variable.
    wcstring key;

    // The value of the variable, or none if it is erased.
    maybe_t<env_var_t> val;

    /// Construct from a key and maybe a value.
    callback_data_t(wcstring k, maybe_t<env_var_t> v) : key(std::move(k)), val(std::move(v)) {}

    /// \return whether this callback represents an erased variable.
    bool is_erase() const { return !val.has_value(); }
};

using callback_data_list_t = std::vector<callback_data_t>;

// List of fish universal variable formats.
// This is exposed for testing.
enum class uvar_format_t { fish_2_x, fish_3_0, future };

bool get_hostname_identifier(wcstring &result);

/// Class representing universal variables.
class env_universal_t {
   public:
    // Construct an empty universal variables.
    env_universal_t() = default;

    // Get the value of the variable with the specified name.
    maybe_t<env_var_t> get(const wcstring &name) const;

    // \return flags from the variable with the given name.
    maybe_t<env_var_t::env_var_flags_t> get_flags(const wcstring &name) const;

    // Sets a variable.
    void set(const wcstring &key, const env_var_t &var);

    // Removes a variable. Returns true if it was found, false if not.
    bool remove(const wcstring &key);

    // Gets variable names.
    wcstring_list_t get_names(bool show_exported, bool show_unexported) const;

    /// Get a view on the universal variable table.
    const var_table_t &get_table() const { return vars; }

    /// Initialize this uvars for the default path.
    /// This should be called at most once on any given instance.
    void initialize(callback_data_list_t &callbacks);

    /// Initialize a this uvars for a given path.
    /// This is exposed for testing only.
    void initialize_at_path(callback_data_list_t &callbacks, wcstring path);

    /// Reads and writes variables at the correct path. Returns true if modified variables were
    /// written.
    bool sync(callback_data_list_t &callbacks);

    /// Populate a variable table \p out_vars from a \p s string.
    /// This is exposed for testing only.
    /// \return the format of the file that we read.
    static uvar_format_t populate_variables(const std::string &s, var_table_t *out_vars);

    /// Guess a file format. Exposed for testing only.
    static uvar_format_t format_for_contents(const std::string &s);

    /// Serialize a variable list. Exposed for testing only.
    static std::string serialize_with_vars(const var_table_t &vars);

    /// Exposed for testing only.
    bool is_ok_to_save() const { return ok_to_save; }

    /// Access the export generation.
    uint64_t get_export_generation() const { return export_generation; }

   private:
    // Path that we save to. This is set in initialize(). If empty, initialize has not been called.
    wcstring vars_path_;
    std::string narrow_vars_path_;

    // The table of variables.
    var_table_t vars;

    // Keys that have been modified, and need to be written. A value here that is not present in
    // vars indicates a deleted value.
    std::unordered_set<wcstring> modified;

    // A generation count which is incremented every time an exported variable is modified.
    uint64_t export_generation{1};

    // Whether it's OK to save. This may be set to false if we discover that a future version of
    // fish wrote the uvars contents.
    bool ok_to_save{true};

    // If true, attempt to flock the uvars file.
    // This latches to false if the file is found to be remote, where flock may hang.
    bool do_flock{true};

    // File id from which we last read.
    file_id_t last_read_file = kInvalidFileID;

    /// \return whether we are initialized.
    bool initialized() const { return !vars_path_.empty(); }

    bool load_from_path(const wcstring &path, callback_data_list_t &callbacks);
    bool load_from_path(const std::string &path, callback_data_list_t &callbacks);

    void load_from_fd(int fd, callback_data_list_t &callbacks);

    // Functions concerned with saving.
    bool open_and_acquire_lock(const wcstring &path, autoclose_fd_t *out_fd);
    autoclose_fd_t open_temporary_file(const wcstring &directory, wcstring *out_path);
    bool write_to_fd(int fd, const wcstring &path);
    bool move_new_vars_file_into_place(const wcstring &src, const wcstring &dst);

    // Given a variable table, generate callbacks representing the difference between our vars and
    // the new vars. Also update our exports generation count as necessary.
    void generate_callbacks_and_update_exports(const var_table_t &new_vars,
                                               callback_data_list_t &callbacks);

    // Given a variable table, copy unmodified values into self.
    void acquire_variables(var_table_t &&vars_to_acquire);

    static bool populate_1_variable(const wchar_t *input, env_var_t::env_var_flags_t flags,
                                    var_table_t *vars, wcstring *storage);

    static void parse_message_2x_internal(const wcstring &msg, var_table_t *vars,
                                          wcstring *storage);
    static void parse_message_30_internal(const wcstring &msg, var_table_t *vars,
                                          wcstring *storage);
    static uvar_format_t read_message_internal(int fd, var_table_t *vars);

    bool save(const wcstring &directory, const wcstring &vars_path);
};

/// The "universal notifier" is an object responsible for broadcasting and receiving universal
/// variable change notifications. These notifications do not contain the change, but merely
/// indicate that the uvar file has changed. It is up to the uvar subsystem to re-read the file.
///
/// We support a few notification strategies. Not all strategies are supported on all platforms.
///
/// Notifiers may request polling, and/or provide a file descriptor to be watched for readability in
/// select().
///
/// To request polling, the notifier overrides usec_delay_between_polls() to return a positive
/// value. That value will be used as the timeout in select(). When select returns, the loop invokes
/// poll(). poll() should return true to indicate that the file may have changed.
///
/// To provide a file descriptor, the notifier overrides notification_fd() to return a non-negative
/// fd. This will be added to the "read" file descriptor list in select(). If the fd is readable,
/// notification_fd_became_readable() will be called; that function should be overridden to return
/// true if the file may have changed.
class universal_notifier_t {
   public:
    enum notifier_strategy_t {
        // Poll on shared memory.
        strategy_shmem_polling,

        // Mac-specific notify(3) implementation.
        strategy_notifyd,

        // Strategy that uses a named pipe. Somewhat complex, but portable and doesn't require
        // polling most of the time.
        strategy_named_pipe,
    };

    universal_notifier_t(const universal_notifier_t &) = delete;
    universal_notifier_t &operator=(const universal_notifier_t &) = delete;

   protected:
    universal_notifier_t();

   public:
    static notifier_strategy_t resolve_default_strategy();
    virtual ~universal_notifier_t();

    // Factory constructor.
    static std::unique_ptr<universal_notifier_t> new_notifier_for_strategy(
        notifier_strategy_t strat, const wchar_t *test_path = nullptr);

    // Default instance. Other instances are possible for testing.
    static universal_notifier_t &default_notifier();

    // Does a fast poll(). Returns true if changed.
    virtual bool poll();

    // Triggers a notification.
    virtual void post_notification();

    // Recommended delay between polls. A value of 0 means no polling required (so no timeout).
    virtual unsigned long usec_delay_between_polls() const;

    // Returns the fd from which to watch for events, or -1 if none.
    virtual int notification_fd() const;

    // The notification_fd is readable; drain it. Returns true if a notification is considered to
    // have been posted.
    virtual bool notification_fd_became_readable(int fd);
};

wcstring get_runtime_path();

#endif
