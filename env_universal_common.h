#ifndef FISH_ENV_UNIVERSAL_COMMON_H
#define FISH_ENV_UNIVERSAL_COMMON_H

#include <wchar.h>
#include <queue>
#include <string>
#include <set>
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
   The erase command
*/
#define ERASE_STR L"ERASE"

/**
   The barrier command
*/
#define BARRIER_STR L"BARRIER"

/**
   The barrier_reply command
*/
#define BARRIER_REPLY_STR L"BARRIER_REPLY"


/**
   The filename to use for univeral variables. The username is appended
*/
#define SOCK_FILENAME "fishd.socket."

/**
   The different types of commands that can be sent between client/server
*/
typedef enum
{
    SET,
    SET_EXPORT,
    ERASE,
    BARRIER,
    BARRIER_REPLY,
} fish_message_type_t;

/**
   The size of the buffer used for reading from the socket
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
       Message body. The message must be allocated using enough memory to actually contain the message.
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
    void write_to_fd(int fd);
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

class universal_notifier_t
{
public:
    enum notifier_strategy_t
    {
        strategy_default,
        strategy_shmem_polling,
        strategy_notifyd
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
    static universal_notifier_t *new_notifier_for_strategy(notifier_strategy_t strat);
    
    /* Default instance. Other instances are possible for testing. */
    static universal_notifier_t &default_notifier();
    
    /* Returns the fd from which to watch for events, or -1 if none */
    virtual int notification_fd();
    
    /* Does a fast poll(). Returns true if changed. */
    virtual bool poll();
    
    /* Indicates whether this notifier requires polling. */
    virtual bool needs_polling() const;
    
    /* Triggers a notification */
    virtual void post_notification();
    
    /* Recommended delay between polls. A value of 0 means no polling required (so no timeout) */
    virtual unsigned long usec_delay_between_polls() const;
};

std::string get_machine_identifier();
bool get_hostname_identifier(std::string *result);

#endif
