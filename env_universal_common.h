#ifndef FISH_ENV_UNIVERSAL_COMMON_H
#define FISH_ENV_UNIVERSAL_COMMON_H

#include <wchar.h>
#include <queue>
#include <string>
#include "util.h"

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
typedef struct connection
{
    /**
       The file descriptor this socket lives on
    */
    int fd;
    /**
       Queue of onsent messages
    */
    message_queue_t *unsent;
    /**
       Set to one when this connection should be killed
    */
    int killme;
    /**
       The input string. Input from the socket goes here. When a
       newline is encountered, the buffer is parsed and cleared.
    */
    std::vector<char> input;

    /**
       The read buffer.
    */
    char buffer[ENV_UNIVERSAL_BUFFER_SIZE];

    /**
       Number of bytes that have already been consumed.
    */
    size_t buffer_consumed;

    /**
       Number of bytes that have been read into the buffer.
    */
    size_t buffer_used;


    /**
       Link to the next connection
    */
    struct connection *next;
}
connection_t;

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
   Destroy library data
*/
void env_universal_common_destroy();

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
wchar_t *env_universal_common_get(const wcstring &name);

/**
   Get the export flag of the variable with the specified
   name. Returns false if the variable doesn't exist.

   This function operate agains the local copy of all universal
   variables, it does not communicate with any other process.
*/
bool env_universal_common_get_export(const wcstring &name);

/**
   Add messages about all existing variables to the specified connection
*/
void enqueue_all(connection_t *c);

/**
   Fill in the specified connection_t struct. Use the specified file
   descriptor for communication.
*/
void connection_init(connection_t *c, int fd);

/**
   Close and destroy the specified connection struct. This frees
   allstructures allocated by the connection, such as ques of unsent
   messages.
*/
void connection_destroy(connection_t *c);

#endif
