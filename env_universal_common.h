#ifndef FISH_ENV_UNIVERSAL_COMMON_H
#define FISH_ENV_UNIVERSAL_COMMON_H

#include <wchar.h>
#include <queue>
#include <string>
#include <set>
#include "wutil.h"
#include "util.h"
#include "env.h"

/**
   The set command
*/
#define SET_STR L"SET"

/**
   The set_export command
*/
#define SET_EXPORT_STR L"SET_EXPORT"

/**
   The different types of messages found in the fishd file
*/
typedef enum
{
    SET,
    SET_EXPORT
} fish_message_type_t;

/**
   The size of the buffer used for reading from the file
*/
#define ENV_UNIVERSAL_BUFFER_SIZE 1024

/**
   A struct representing a message to be sent between client and server
*/
typedef struct
{
    /**
       Number of queues that contain this message. Once this reaches zero, the message should be deleted
    */
    int count;

    /**
       Message body.
    */
    std::string body;

} message_t;

typedef std::queue<message_t *> message_queue_t;

/**
   This struct represents a connection between a universal variable server/client
*/
class connection_t
{
private:
    /* No assignment */
    connection_t &operator=(const connection_t &);

public:

    /**
       The file descriptor this socket lives on
    */
    int fd;

    /**
       Queue of unsent messages
    */
    std::queue<message_t *> unsent;

    /**
       Set to one when this connection should be killed
    */
    bool killme;
    /**
       The input string. Input from the socket goes here. When a
       newline is encountered, the buffer is parsed and cleared.
    */
    std::vector<char> input;

    /**
       The read buffer.
    */
    std::vector<char> read_buffer;

    /**
       Number of bytes that have already been consumed.
    */
    size_t buffer_consumed;

    /* Constructor */
    connection_t(int input_fd);
};

/**
   Read all available messages on this connection
*/
void read_message(connection_t *);

/**
   Send as many messages as possible without blocking to the connection
*/
void try_send_all(connection_t *c);

/**
   Create a messge with the specified properties
*/
message_t *create_message(fish_message_type_t type, const wchar_t *key, const wchar_t *val);

/**
   Init the library
*/
void env_universal_common_init(void (*cb)(fish_message_type_t type, const wchar_t *key, const wchar_t *val));

/**
   Add all variable names to the specified list

   This function operate agains the local copy of all universal
   variables, it does not communicate with any other process.
*/
void env_universal_common_get_names(wcstring_list_t &lst,
                                    bool show_exported,
                                    bool show_unexported);

/**
   Perform the specified variable assignment.

   This function operate agains the local copy of all universal
   variables, it does not communicate with any other process.

   Do not call this function. Create a message to do it. This function
   is only to be used when fishd is dead.
*/
void env_universal_common_set(const wchar_t *key, const wchar_t *val, bool exportv);

/**
   Remove the specified variable.

   This function operate agains the local copy of all universal
   variables, it does not communicate with any other process.

   Do not call this function. Create a message to do it. This function
   is only to be used when fishd is dead.
*/
void env_universal_common_remove(const wcstring &key);

/**
   Get the value of the variable with the specified name

   This function operate agains the local copy of all universal
   variables, it does not communicate with any other process.
*/
env_var_t env_universal_common_get(const wcstring &name);

/**
   Get the export flag of the variable with the specified
   name. Returns false if the variable doesn't exist.

   This function operate agains the local copy of all universal
   variables, it does not communicate with any other process.
*/
bool env_universal_common_get_export(const wcstring &name);

/** Synchronizes all changse: writes everything out, reads stuff in */
void env_universal_common_sync();

/**
   Add messages about all existing variables to the specified connection
*/
void enqueue_all(connection_t *c);

/**
   Close and destroy the specified connection struct. This frees
   allstructures allocated by the connection, such as ques of unsent
   messages.
*/
void connection_destroy(connection_t *c);

typedef std::vector<struct callback_data_t> callback_data_list_t;

/** Class representing universal variables */
class env_universal_t
{
    /* Current values */
    var_table_t vars;
    
    /* Keys that have been modified, and need to be written. A value here that is not present in vars indicates a deleted value. */
    std::set<wcstring> modified;
    
    /* Path that we save to. If empty, use the default */
    const wcstring explicit_vars_path;
    
    mutable pthread_mutex_t lock;
    bool tried_renaming;
    bool load_from_path(const wcstring &path, callback_data_list_t *callbacks);
    void load_from_fd(int fd, callback_data_list_t *callbacks);
    void erase_unmodified_values();
    
    void parse_message_internal(wchar_t *msg, connection_t *src, callback_data_list_t *callbacks);
    
    void set_internal(const wcstring &key, const wcstring &val, bool exportv, bool overwrite);
    void remove_internal(const wcstring &name, bool overwrite);
    
    /* Functions concerned with saving */
    bool open_and_acquire_lock(const wcstring &path, int *out_fd);
    bool open_temporary_file(const wcstring &directory, wcstring *out_path, int *out_fd);
    bool write_to_fd(int fd, const wcstring &path);
    bool move_new_vars_file_into_place(const wcstring &src, const wcstring &dst);
    
    /* File id from which we last read */
    file_id_t last_read_file;
    
    void read_message_internal(connection_t *src, callback_data_list_t *callbacks);
    void enqueue_all_internal(connection_t *c) const;
    
public:
    env_universal_t(const wcstring &path);
    ~env_universal_t();
    
    /* Get the value of the variable with the specified name */
    env_var_t get(const wcstring &name) const;
    
    /* Returns whether the variable with the given name is exported, or false if it does not exist */
    bool get_export(const wcstring &name) const;
    
    /* Sets a variable */
    void set(const wcstring &key, const wcstring &val, bool exportv);
    
    /* Removes a variable */
    void remove(const wcstring &name);
    
    /* Gets variable names */
    wcstring_list_t get_names(bool show_exported, bool show_unexported) const;
    
    /* Writes variables to the connection */
    void enqueue_all(connection_t *c) const;
    
    /** Loads variables at the correct path */
    bool load();

    /** Reads and writes variables at the correct path. Returns true if modified variables were written. */
    bool sync(callback_data_list_t *callbacks);
    
    /* Internal use */
    void read_message(connection_t *src, callback_data_list_t *callbacks);
};

/** The "universal notifier" is an object responsible for broadcasting and receiving universal variable change notifications. These notifications do not contain the change, but merely indicate that the uvar file has changed. It is up to the uvar subsystem to re-read the file.

   We support a few notificatins strategies. Not all strategies are supported on all platforms.
   
   Notifiers may request polling, and/or provide a file descriptor to be watched for readability in select().
   
   To request polling, the notifier overrides usec_delay_between_polls() to return a positive value. That value will be used as the timeout in select(). When select returns, the loop invokes poll(). poll() should return true to indicate that the file may have changed.
    
   To provide a file descriptor, the notifier overrides notification_fd() to return a non-negative fd. This will be added to the "read" file descriptor list in select(). If the fd is readable, notification_fd_became_readable() will be called; that function should be overridden to return true if the file may have changed.

*/
class universal_notifier_t
{
public:
    enum notifier_strategy_t
    {
        // Default meta-strategy to use the 'best' notifier for the system
        strategy_default,
        
        // Use a value in shared memory. Simple, but requires polling and therefore semi-frequent wakeups.
        strategy_shmem_polling,
        
        // Strategy that uses a named pipe. Somewhat complex, but portable and doesn't require polling most of the time.
        strategy_named_pipe,
                
        // Strategy that uses notify(3). Simple and efficient, but OS X only.
        strategy_notifyd,
        
        // Null notifier, does nothing
        strategy_null
    };

    protected:
    universal_notifier_t();
    
    private:
    /* No copying */
    universal_notifier_t &operator=(const universal_notifier_t &);
    universal_notifier_t(const universal_notifier_t &x);
    static notifier_strategy_t resolve_default_strategy();
    
    public:
    
    virtual ~universal_notifier_t();
    
    /* Factory constructor. Free with delete */
    static universal_notifier_t *new_notifier_for_strategy(notifier_strategy_t strat, const wchar_t *test_path = NULL);
    
    /* Default instance. Other instances are possible for testing. */
    static universal_notifier_t &default_notifier();
    
    /* Does a fast poll(). Returns true if changed. */
    virtual bool poll();
    
    /* Triggers a notification */
    virtual void post_notification();
    
    /* Recommended delay between polls. A value of 0 means no polling required (so no timeout) */
    virtual unsigned long usec_delay_between_polls() const;
    
    /* Returns the fd from which to watch for events, or -1 if none */
    virtual int notification_fd();
    
    /* The notification_fd is readable; drain it. Returns true if a notification is considered to have been posted. */
    virtual bool notification_fd_became_readable(int fd);
};

std::string get_machine_identifier();
bool get_hostname_identifier(std::string *result);

/* Temporary */
bool synchronizes_via_fishd();

bool universal_log_enabled();
#define UNIVERSAL_LOG(x) if (universal_log_enabled()) fprintf(stderr, "UNIVERSAL LOG: %s\n", x)

/* Environment variable for requesting a particular universal notifier. See fetch_default_strategy_from_environment for names. */
#define UNIVERSAL_NOTIFIER_ENV_NAME "fish_universal_notifier"

/* Environment variable for enabling universal variable logging (to stderr) */
#define UNIVERSAL_LOGGING_ENV_NAME "fish_universal_log"

/* Environment variable for enabling fishd */
#define UNIVERSAL_USE_FISHD "fish_use_fishd"

#endif
