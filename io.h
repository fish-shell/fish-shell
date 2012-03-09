#ifndef FISH_IO_H
#define FISH_IO_H

#include <vector>
#include <tr1/memory>
using std::tr1::shared_ptr;

/**
   Describes what type of IO operation an io_data_t represents
*/
enum io_mode
{
	IO_FILE, IO_PIPE, IO_FD, IO_BUFFER, IO_CLOSE
};

/** Represents an FD redirection */
class io_data_t
{
private:
    /** buffer to save output in for IO_BUFFER. Note that in the original fish, the buffer was a pointer to a buffer_t stored in the param2 union down below, and when an io_data_t was duplicated the pointer was copied so that two io_data_ts referenced the same buffer. It's not clear to me how this was ever cleaned up correctly. But it's important that they share the same buffer for reasons I don't yet understand either. But we can get correct sharing and cleanup with shared_ptr. */
    shared_ptr<std::vector<char> > out_buffer;

    /* No assignment allowed */
    void operator=(const io_data_t &rhs) { assert(0); }

public:
	/** Type of redirect */
	int io_mode;
	/** FD to redirect */
	int fd;

	/** 
		Type-specific parameter for redirection 
	*/
	union
	{
		/** Fds for IO_PIPE and for IO_BUFFER */
		int pipe_fd[2];
		/** fd to redirect specified fd to, for IO_FD*/
		int old_fd;
	} param1;
    

	/**  Second type-specific paramter for redirection */
	union
	{
		/** file creation flags to send to open for IO_FILE */
		int flags;
		/** Whether to close old_fd for IO_FD */
		int close_old;
	} param2;
    
    /** Filename IO_FILE. malloc'd. This needs to be used after fork, so don't use wcstring here. */
    const char *filename_cstr;
    
    /** Convenience to set filename_cstr via wcstring */
    void set_filename(const wcstring &str) {
        free((void *)filename_cstr);
        filename_cstr = wcs2str(str.c_str());
    }

    /** Function to create the output buffer */
    void out_buffer_create() {
        out_buffer.reset(new std::vector<char>);
    }
        
    /** Function to append to the buffer */
    void out_buffer_append(const char *ptr, size_t count) {
        assert(out_buffer.get() != NULL); 
        out_buffer->insert(out_buffer->end(), ptr, ptr + count);
    }
    
    /** Function to get a pointer to the buffer */
    char *out_buffer_ptr(void) {
        assert(out_buffer.get() != NULL);
        return &out_buffer->at(0);
    }
    
    /** Function to get the size of the buffer */
    size_t out_buffer_size(void) const {
        assert(out_buffer.get() != NULL);
        return out_buffer->size();
    }

	/** Set to true if this is an input io redirection */
	int is_input;
	
	/** Pointer to the next IO redirection */
	io_data_t *next;
    
    io_data_t() : filename_cstr(NULL), next(NULL)
    {
    }
    
    io_data_t(const io_data_t &rhs) :
        out_buffer(rhs.out_buffer),
        io_mode(rhs.io_mode),
        fd(rhs.fd),
        param1(rhs.param1),
        param2(rhs.param2),
        filename_cstr(rhs.filename_cstr ? strdup(rhs.filename_cstr) : NULL),
        is_input(rhs.is_input),
        next(rhs.next)
    {
        
    }
    
    ~io_data_t() {
        free((void *)filename_cstr);
    }
};

 
/**
   Join two chains of io redirections
*/
io_data_t *io_add( io_data_t *first_chain, io_data_t *decond_chain );

/**
   Remove the specified io redirection from the chain
*/
io_data_t *io_remove( io_data_t *list, io_data_t *element );

/**
   Make a copy of the specified chain of redirections. Uses operator new.
*/
io_data_t *io_duplicate( io_data_t *l );

/**
   Return the last io redirection in the chain for the specified file descriptor.
*/
io_data_t *io_get( io_data_t *io, int fd );



/**
   Free all resources used by a IO_BUFFER type io redirection.
*/
void io_buffer_destroy( io_data_t *io_buffer );

/**
   Create a IO_BUFFER type io redirection, complete with a pipe and a
   vector<char> for output. The default file descriptor used is 1 for
   output buffering and 0 for input buffering.

   \param is_input set this parameter to zero if the buffer should be
   used to buffer the output of a command, or non-zero to buffer the
   input to a command.
*/
io_data_t *io_buffer_create( int is_input );

/**
   Close output pipe, and read from input pipe until eof.
*/
void io_buffer_read( io_data_t *d );

/**
   Print debug information about the specified IO redirection chain to stderr.
*/
void io_print( io_data_t *io );

#endif
