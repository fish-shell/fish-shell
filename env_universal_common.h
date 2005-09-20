#ifndef ENV_UNIVERSAL_COMMON_HH
#define ENV_UNIVERSAL_COMMON_HH


/**
   The set command
*/
#define SET_STR L"SET"

/**
   The erase command
*/
#define ERASE_STR L"ERASE"

#define BARRIER_STR L"BARRIER"
#define BARRIER_REPLY_STR L"BARRIER_REPLY"


/**
   The filename to use for univeral variables. The username is appended
*/
#define SOCK_FILENAME "fishd.socket."

/**
   The different types of commands that can be sent between client/server
*/
enum
{
	SET,
	ERASE,
	BARRIER,
	BARRIER_REPLY,
}
	;

/**
   The table of universal variables
*/
extern hash_table_t env_universal_var;

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
	queue_t unsent;
	/**
	   Set to one when this connection should be killed
	*/
	int killme;
	/**
	   The state used for character conversions
	*/
	mbstate_t wstate;
	/**
	   The input string. Input from the socket goes here. When a
	   newline is encountered, the buffer is parsed and cleared.
	*/
	string_buffer_t input;
	
	/**
	   Link to the next connection
	*/
	struct connection *next;
}
	connection_t;

/**
   A struct representing a message to be sent between client and server
*/
typedef struct 
{
	int count;
	char body[0];
}
	message_t;

/**
   Read all available messages on this connection
*/
void read_message( connection_t * );

/**
   Send as many messages as possible without blocking to the connection
*/
void try_send_all( connection_t *c );

/**
   Create a messge with the specified properties
*/
message_t *create_message( int type, const wchar_t *key, const wchar_t *val );

/**
   Init the library
*/
void env_universal_common_init(void (*cb)(int type, const wchar_t *key, const wchar_t *val ) );

/**
   Destroy library data
*/
void env_universal_common_destroy();

#endif
