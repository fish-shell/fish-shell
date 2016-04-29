#ifndef FISH_ENV_UNIVERSAL_COMMON_H
#define FISH_ENV_UNIVERSAL_COMMON_H

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <memory>
#include <set>
#include <vector>

#include "common.h"
#include "env.h"
#include "wutil.h"

/// The different types of messages found in the fishd file.
typedef enum { SET, SET_EXPORT, ERASE } fish_message_type_t;

/// Callback data, reflecting a change in universal variables.
struct callback_data_t {
    fish_message_type_t type;
    wcstring key;
    wcstring val;

    callback_data_t(fish_message_type_t t, const wcstring &k, const wcstring &v)
        : type(t), key(k), val(v) {}
};

typedef std::vector<struct callback_data_t> callback_data_list_t;

/// Class representing universal variables.
class env_universal_t {
    var_table_t vars;  // current values

    // Keys that have been modified, and need to be written. A value here that is not present in
    // vars indicates a deleted value.
    std::set<wcstring> modified;

    // Path that we save to. If empty, use the default.
    const wcstring explicit_vars_path;

    mutable pthread_mutex_t lock;
    bool tried_renaming;
    bool load_from_path(const wcstring &path, callback_data_list_t *callbacks);
    void load_from_fd(int fd, callback_data_list_t *callbacks);

    void set_internal(const wcstring &key, const wcstring &val, bool exportv, bool overwrite);
    bool remove_internal(const wcstring &name);

    // Functions concerned with saving.
    bool open_and_acquire_lock(const wcstring &path, int *out_fd);
    bool open_temporary_file(const wcstring &directory, wcstring *out_path, int *out_fd);
    bool write_to_fd(int fd, const wcstring &path);
    bool move_new_vars_file_into_place(const wcstring &src, const wcstring &dst);

    // File id from which we last read.
    file_id_t last_read_file;

    // Given a variable table, generate callbacks representing the difference between our vars and
    // the new vars.
    void generate_callbacks(const var_table_t &new_vars, callback_data_list_t *callbacks) const;

    // Given a variable table, copy unmodified values into self. May destructively modified
    // vars_to_acquire.
    void acquire_variables(var_table_t *vars_to_acquire);

    static void parse_message_internal(const wcstring &msg, var_table_t *vars, wcstring *storage);
    static var_table_t read_message_internal(int fd);

   public:
    explicit env_universal_t(const wcstring &path);
    ~env_universal_t();

    // Get the value of the variable with the specified name.
    env_var_t get(const wcstring &name) const;

    // Returns whether the variable with the given name is exported, or false if it does not exist.
    bool get_export(const wcstring &name) const;

    // Sets a variable.
    void set(const wcstring &key, const wcstring &val, bool exportv);

    // Removes a variable. Returns true if it was found, false if not.
    bool remove(const wcstring &name);

    // Gets variable names.
    wcstring_list_t get_names(bool show_exported, bool show_unexported) const;

    /// Loads variables at the correct path.
    bool load();

    /// Reads and writes variables at the correct path. Returns true if modified variables were
    /// written.
    bool sync(callback_data_list_t *callbacks);
};

/// The "universal notifier" is an object responsible for broadcasting and receiving universal
/// variable change notifications. These notifications do not contain the change, but merely
/// indicate that the uvar file has changed. It is up to the uvar subsystem to re-read the file.
///
/// We support a few notificatins strategies. Not all strategies are supported on all platforms.
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
        // Default meta-strategy to use the 'best' notifier for the system.
        strategy_default,

        // Use a value in shared memory. Simple, but requires polling and therefore semi-frequent
        // wakeups.
        strategy_shmem_polling,

        // Strategy that uses a named pipe. Somewhat complex, but portable and doesn't require
        // polling most of the time.
        strategy_named_pipe,

        // Strategy that uses notify(3). Simple and efficient, but OS X only.
        strategy_notifyd,

        // Null notifier, does nothing.
        strategy_null
    };

   protected:
    universal_notifier_t();

   private:
    // No copying.
    universal_notifier_t &operator=(const universal_notifier_t &);
    universal_notifier_t(const universal_notifier_t &x);
    static notifier_strategy_t resolve_default_strategy();

   public:
    virtual ~universal_notifier_t();

    // Factory constructor. Free with delete.
    static universal_notifier_t *new_notifier_for_strategy(notifier_strategy_t strat,
                                                           const wchar_t *test_path = NULL);

    // Default instance. Other instances are possible for testing.
    static universal_notifier_t &default_notifier();

    // Does a fast poll(). Returns true if changed.
    virtual bool poll();

    // Triggers a notification.
    virtual void post_notification();

    // Recommended delay between polls. A value of 0 means no polling required (so no timeout).
    virtual unsigned long usec_delay_between_polls() const;

    // Returns the fd from which to watch for events, or -1 if none.
    virtual int notification_fd();

    // The notification_fd is readable; drain it. Returns true if a notification is considered to
    // have been posted.
    virtual bool notification_fd_became_readable(int fd);
};

// Environment variable for requesting a particular universal notifier. See
// fetch_default_strategy_from_environment for names.
#define UNIVERSAL_NOTIFIER_ENV_NAME "fish_universal_notifier"

#endif
