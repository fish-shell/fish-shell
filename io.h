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
    /** buffer to save output in for IO_BUFFER. Note that in the original fish, the buffer was a pointer to a buffer_t stored in the param2 union down below, and when an io_data_t was duplicated the pointer was copied so that two io_data_ts referenced the same buffer. It's not clear to me how this was ever cleaned up correctly. But it's important that they share the same buffer for reasons I don't yet understand either. We can get correct sharing and cleanup with shared_ptr. */
    shared_ptr<std::vector<char> > out_buffer;

    /* No assignment allowed */
    void operator=(const io_data_t &rhs);

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
        /** fd to redirect specified fd to, for IO_FD */
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
    void set_filename(const wcstring &str)
    {
        free((void *)filename_cstr);
        filename_cstr = wcs2str(str.c_str());
    }

    /** Function to create the output buffer */
    void out_buffer_create()
    {
        out_buffer.reset(new std::vector<char>);
    }

    /** Function to append to the buffer */
    void out_buffer_append(const char *ptr, size_t count)
    {
        assert(out_buffer.get() != NULL);
        out_buffer->insert(out_buffer->end(), ptr, ptr + count);
    }

    /** Function to get a pointer to the buffer */
    char *out_buffer_ptr(void)
    {
        assert(out_buffer.get() != NULL);
        return out_buffer->empty() ? NULL : &out_buffer->at(0);
    }

    const char *out_buffer_ptr(void) const
    {
        assert(out_buffer.get() != NULL);
        return out_buffer->empty() ? NULL : &out_buffer->at(0);
    }

    /** Function to get the size of the buffer */
    size_t out_buffer_size(void) const
    {
        assert(out_buffer.get() != NULL);
        return out_buffer->size();
    }

    /** Set to true if this is an input io redirection */
    bool is_input;

    io_data_t() :
        out_buffer(),
        io_mode(0),
        fd(0),
        param1(),
        param2(),
        filename_cstr(NULL),
        is_input(0)
    {
    }

    ~io_data_t()
    {
        free((void *)filename_cstr);
    }
};

class io_chain_t : public std::vector<shared_ptr<io_data_t> >
{
public:
    io_chain_t();
    io_chain_t(shared_ptr<io_data_t> );

    void remove(shared_ptr<const io_data_t> element);
    io_chain_t duplicate() const;
    void duplicate_prepend(const io_chain_t &src);
    void destroy();

    shared_ptr<const io_data_t> get_io_for_fd(int fd) const;
    shared_ptr<io_data_t> get_io_for_fd(int fd);

};

/**
   Remove the specified io redirection from the chain
*/
void io_remove(io_chain_t &list, shared_ptr<const io_data_t> element);

/** Make a copy of the specified chain of redirections. Uses operator new. */
io_chain_t io_duplicate(const io_chain_t &chain);

/** Return a shallow copy of the specified chain of redirections that contains only the applicable redirections. That is, if there's multiple redirections for the same fd, only the second one is included. */
io_chain_t io_unique(const io_chain_t &chain);

/** Prepends a copy of the specified 'src' chain of redirections to 'dst.' Uses operator new. */
void io_duplicate_prepend(const io_chain_t &src, io_chain_t &dst);

/** Destroys an io_chain */
void io_chain_destroy(io_chain_t &chain);

/**
   Return the last io redirection in the chain for the specified file descriptor.
*/
shared_ptr<const io_data_t> io_chain_get(const io_chain_t &src, int fd);
shared_ptr<io_data_t> io_chain_get(io_chain_t &src, int fd);


/**
   Free all resources used by a IO_BUFFER type io redirection.
*/
void io_buffer_destroy(shared_ptr<io_data_t> io_buffer);

/**
   Create a IO_BUFFER type io redirection, complete with a pipe and a
   vector<char> for output. The default file descriptor used is 1 for
   output buffering and 0 for input buffering.

   \param is_input set this parameter to zero if the buffer should be
   used to buffer the output of a command, or non-zero to buffer the
   input to a command.
*/
io_data_t *io_buffer_create(bool is_input);

/**
   Close output pipe, and read from input pipe until eof.
*/
void io_buffer_read(io_data_t *d);

/** Print debug information about the specified IO redirection chain to stderr. */
void io_print(const io_chain_t &chain);

#endif
