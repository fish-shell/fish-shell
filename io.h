#ifndef FISH_IO_H
#define FISH_IO_H

/**
   Describes what type of IO operation an io_data_t represents
*/
enum io_mode
{
	IO_FILE, IO_PIPE, IO_FD, IO_BUFFER, IO_CLOSE
}
;

/** Represents an FD redirection */
typedef struct io_data
{
	/** Type of redirect */
	int io_mode;
	/** FD to redirect */
	int fd;
	/** parameter for redirection */
	union
	{
		/** Fds for IO_PIPE and for IO_BUFFER */
		int pipe_fd[2];
		/** Filename IO_FILE */
		wchar_t *filename;
		/** fd to redirect specified fd to, for IO_FD*/
		int old_fd;
	} param1
	;
	union
	{
		/** file creation flags to send to open for IO_FILE */
		int flags;
		/** buffer to save output in for IO_BUFFER */
		buffer_t *out_buffer;		
		/** Whether to close old_fd for IO_FD */
		int close_old;
		
	} param2
	;
	
	/** Pointer to the next IO redirection */
	struct io_data *next;
}
io_data_t;

 
/**
   Join two chains of io redirections
*/
io_data_t *io_add( io_data_t *first_chain, io_data_t *decond_chain );

/**
   Remove the specified io redirection from the chain
*/
io_data_t *io_remove( io_data_t *list, io_data_t *element );

/**
   Make a copy of the specified chain of redirections
*/
io_data_t *io_duplicate( io_data_t *l );

/**
   Return the last io redirection in ht e chain for the specified file descriptor.
*/
io_data_t *io_get( io_data_t *io, int fd );



/**
   Free all resources used by a IO_BUFFER type io redirection.
*/
void io_buffer_destroy( io_data_t *io_buffer );

/**
   Create a IO_BUFFER type io redirection, complete with a pipe and a
   buffer_t for output.
*/
io_data_t *io_buffer_create();

/**
   Close output pipe, and read from input pipe until eof.
*/
void io_buffer_read( io_data_t *d );

#endif
