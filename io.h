#ifndef FISH_IO_H
#define FISH_IO_H

#include <vector>

// Note that we have to include something to get any _LIBCPP_VERSION defined so we can detect libc++
// So it's key that vector go above. If we didn't need vector for other reasons, we might include ciso646, which does nothing

#if defined(_LIBCPP_VERSION) || __cplusplus > 199711L
// C++11 or libc++ (which is a C++11-only library, but the memory header works OK in C++03)
#include <memory>
using std::shared_ptr;
#else
// C++03 or libstdc++
#include <tr1/memory>
using std::tr1::shared_ptr;
#endif

/**
   Describes what type of IO operation an io_data_t represents
*/
enum io_mode_t
{
    IO_FILE, IO_PIPE, IO_FD, IO_BUFFER, IO_CLOSE
};

/** Represents an FD redirection */
class io_data_t
{
private:
    /* No assignment or copying allowed */
    io_data_t(const io_data_t &rhs);
    void operator=(const io_data_t &rhs);

protected:
    io_data_t(io_mode_t m, int f) :
        io_mode(m),
        fd(f)
    {
    }

public:
    /** Type of redirect */
    const io_mode_t io_mode;
    /** FD to redirect */
    const int fd;

    virtual void print() const = 0;
    virtual ~io_data_t() = 0;
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
    const int old_fd;
    /** Whether to close old_fd */
    const bool close_old;

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
    const char * const filename_cstr;
    /** file creation flags to send to open */
    const int flags;

    virtual void print() const;

    io_file_t(int f, const wcstring &fname, int fl = 0) :
        io_data_t(IO_FILE, f),
        filename_cstr(wcs2str(fname)),
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
protected:
    io_pipe_t(io_mode_t m, int f, bool i):
        io_data_t(m, f),
        is_input(i)
    {
        pipe_fd[0] = pipe_fd[1] = -1;
    }

public:
    int pipe_fd[2];
    const bool is_input;

    virtual void print() const;

    io_pipe_t(int f, bool i):
        io_data_t(IO_PIPE, f),
        is_input(i)
    {
        pipe_fd[0] = pipe_fd[1] = -1;
    }
};

class io_buffer_t : public io_pipe_t
{
private:
    /** buffer to save output in */
    std::vector<char> out_buffer;

    io_buffer_t(int f, bool i):
        io_pipe_t(IO_BUFFER, f, i),
        out_buffer()
    {
    }

public:
    virtual void print() const;

    virtual ~io_buffer_t();

    /** Function to append to the buffer */
    void out_buffer_append(const char *ptr, size_t count)
    {
        out_buffer.insert(out_buffer.end(), ptr, ptr + count);
    }

    /** Function to get a pointer to the buffer */
    char *out_buffer_ptr(void)
    {
        return out_buffer.empty() ? NULL : &out_buffer.at(0);
    }

    const char *out_buffer_ptr(void) const
    {
        return out_buffer.empty() ? NULL : &out_buffer.at(0);
    }

    /** Function to get the size of the buffer */
    size_t out_buffer_size(void) const
    {
        return out_buffer.size();
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

       \param fd when -1, determined from is_input.
    */
    static io_buffer_t *create(bool is_input, int fd = -1);
};

class io_chain_t : public std::vector<shared_ptr<io_data_t> >
{
public:
    io_chain_t();
    io_chain_t(const shared_ptr<io_data_t> &);

    void remove(const shared_ptr<const io_data_t> &element);
    void push_back(const shared_ptr<io_data_t> &element);
    void push_front(const shared_ptr<io_data_t> &element);
    void append(const io_chain_t &chain);

    shared_ptr<const io_data_t> get_io_for_fd(int fd) const;
    shared_ptr<io_data_t> get_io_for_fd(int fd);
};

/**
   Remove the specified io redirection from the chain
*/
void io_remove(io_chain_t &list, const shared_ptr<const io_data_t> &element);

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
