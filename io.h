#ifndef FISH_IO_H
#define FISH_IO_H

#include <vector>
#include <tr1/memory>
using std::tr1::shared_ptr;

/**
   Describes what type of IO operation an io_data_t represents
*/
enum io_mode_t
{
    IO_INVALID, IO_FILE, IO_PIPE, IO_FD, IO_BUFFER, IO_CLOSE
};

/** Represents an FD redirection */
class io_data_t
{
private:
    /* No assignment or copying allowed */
    io_data_t(const io_data_t &rhs);
    void operator=(const io_data_t &rhs);

public:
    /** Type of redirect */
    io_mode_t io_mode;
    /** FD to redirect */
    int fd;

    virtual void print() const;

    /** Set to true if this is an input io redirection */
    bool is_input;

    io_data_t(io_mode_t m = IO_INVALID, int f=0) :
        io_mode(m),
        fd(f),
        is_input(0)
    {
    }

    virtual ~io_data_t()
    {
    }
};

class io_close_t : public io_data_t
{
public:
    io_close_t(int f) :
        io_data_t(IO_CLOSE, f)
    {
    }

    virtual void print() const;
};

class io_fd_t : public io_data_t
{
public:
    /** fd to redirect specified fd to */
    int old_fd;
    /** Whether to close old_fd */
    int close_old;

    virtual void print() const;

    io_fd_t(int f, int old, bool close = false) :
        io_data_t(IO_FD, f),
        old_fd(old),
        close_old(close)
    {
    }
};

class io_file_t : public io_data_t
{
public:
    /** Filename, malloc'd. This needs to be used after fork, so don't use wcstring here. */
    const char *filename_cstr;
    /** file creation flags to send to open */
    int flags;

    /** Convenience to set filename_cstr via wcstring */
    void set_filename(const wcstring &str)
    {
        free((void *)filename_cstr);
        filename_cstr = wcs2str(str.c_str());
    }

    virtual void print() const;

    io_file_t(int f, const char *fname = NULL, int fl = 0) :
        io_data_t(IO_FILE, f),
        filename_cstr(fname ? strdup(fname) : NULL),
        flags(fl)
    {
    }

    virtual ~io_file_t()
    {
        free((void *)filename_cstr);
    }
};

class io_pipe_t : public io_data_t
{
public:
    int pipe_fd[2];

    virtual void print() const;

    io_pipe_t(int f):
        io_data_t(IO_PIPE, f),
        pipe_fd()
    {
    }
};

class io_buffer_t : public io_pipe_t
{
private:
    /** buffer to save output in */
    shared_ptr<std::vector<char> > out_buffer;

    io_buffer_t(int f):
        io_pipe_t(f),
        out_buffer()
    {
        io_mode = IO_BUFFER;
    }

public:
    virtual void print() const;

    virtual ~io_buffer_t();

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

    /**
       Close output pipe, and read from input pipe until eof.
    */
    void read();

    /**
       Create a IO_BUFFER type io redirection, complete with a pipe and a
       vector<char> for output. The default file descriptor used is 1 for
       output buffering and 0 for input buffering.

       \param is_input set this parameter to zero if the buffer should be
       used to buffer the output of a command, or non-zero to buffer the
       input to a command.
    */
    static io_buffer_t *create(bool is_input);
};

class io_chain_t : public std::vector<shared_ptr<io_data_t> >
{
public:
    io_chain_t();
    io_chain_t(const shared_ptr<io_data_t> &);

    void remove(const shared_ptr<const io_data_t> &element);
    io_chain_t duplicate() const;
    void duplicate_prepend(const io_chain_t &src);
    void destroy();

    shared_ptr<const io_data_t> get_io_for_fd(int fd) const;
    shared_ptr<io_data_t> get_io_for_fd(int fd);

};

/**
   Remove the specified io redirection from the chain
*/
void io_remove(io_chain_t &list, const shared_ptr<const io_data_t> &element);

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


/** Print debug information about the specified IO redirection chain to stderr. */
void io_print(const io_chain_t &chain);

#endif
