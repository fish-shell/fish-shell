/** \file io.c

Utilities for io redirection.

*/
#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <set>
#include <algorithm>

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <unistd.h>
#include <fcntl.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERMIO_H
#include <termio.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "exec.h"
#include "common.h"
#include "io.h"


void io_data_t::print() const
{
    switch (io_mode)
    {
        case IO_PIPE:
            fprintf(stderr, "pipe {%d, %d}\n", param1.pipe_fd[0], param1.pipe_fd[1]);
            break;
    }
}

void io_close_t::print() const
{
    fprintf(stderr, "close %d\n", fd);
}

void io_fd_t::print() const
{
    fprintf(stderr, "FD map %d -> %d\n", old_fd, fd);
}

void io_file_t::print() const
{
    fprintf(stderr, "file (%s)\n", filename_cstr);
}

void io_buffer_t::print() const
{
    fprintf(stderr, "buffer %p (size %lu)\n", out_buffer_ptr(), out_buffer_size());
}

void io_buffer_read(io_buffer_t *d)
{
    exec_close(d->param1.pipe_fd[1]);

    if (d->io_mode == IO_BUFFER)
    {
        /*    if( fcntl( d->param1.pipe_fd[0], F_SETFL, 0 ) )
            {
              wperror( L"fcntl" );
              return;
              }  */
        debug(4, L"io_buffer_read: blocking read on fd %d", d->param1.pipe_fd[0]);
        while (1)
        {
            char b[4096];
            long l;
            l=read_blocked(d->param1.pipe_fd[0], b, 4096);
            if (l==0)
            {
                break;
            }
            else if (l<0)
            {
                /*
                  exec_read_io_buffer is only called on jobs that have
                  exited, and will therefore never block. But a broken
                  pipe seems to cause some flags to reset, causing the
                  EOF flag to not be set. Therefore, EAGAIN is ignored
                  and we exit anyway.
                */
                if (errno != EAGAIN)
                {
                    debug(1,
                          _(L"An error occured while reading output from code block on file descriptor %d"),
                          d->param1.pipe_fd[0]);
                    wperror(L"io_buffer_read");
                }

                break;
            }
            else
            {
                d->out_buffer_append(b, l);
            }
        }
    }
}


io_buffer_t *io_buffer_create(bool is_input)
{
    bool success = true;
    io_buffer_t *buffer_redirect = new io_buffer_t(is_input ? 0 : 1);
    buffer_redirect->out_buffer_create();
    buffer_redirect->is_input = is_input ? true : false;

    if (exec_pipe(buffer_redirect->param1.pipe_fd) == -1)
    {
        debug(1, PIPE_ERROR);
        wperror(L"pipe");
        success = false;
    }
    else if (fcntl(buffer_redirect->param1.pipe_fd[0],
                   F_SETFL,
                   O_NONBLOCK))
    {
        debug(1, PIPE_ERROR);
        wperror(L"fcntl");
        success = false;
    }

    if (! success)
    {
        delete buffer_redirect;
        buffer_redirect = NULL;
    }

    return buffer_redirect;
}

void io_buffer_destroy(const shared_ptr<io_buffer_t> &io_buffer)
{

    /**
       If this is an input buffer, then io_read_buffer will not have
       been called, and we need to close the output fd as well.
    */
    if (io_buffer->is_input)
    {
        exec_close(io_buffer->param1.pipe_fd[1]);
    }

    exec_close(io_buffer->param1.pipe_fd[0]);

    /*
      Dont free fd for writing. This should already be free'd before
      calling exec_read_io_buffer on the buffer
    */
}

void io_chain_t::remove(const shared_ptr<const io_data_t> &element)
{
    // See if you can guess why std::find doesn't work here
    for (io_chain_t::iterator iter = this->begin(); iter != this->end(); ++iter)
    {
        if (*iter == element)
        {
            this->erase(iter);
            break;
        }
    }
}

void io_chain_t::duplicate_prepend(const io_chain_t &src)
{
    /* Prepend a duplicate of src before this. */
    this->insert(this->begin(), src.begin(), src.end());
}

void io_chain_t::destroy()
{
    this->clear();
}

void io_remove(io_chain_t &list, const shared_ptr<const io_data_t> &element)
{
    list.remove(element);
}

void io_print(const io_chain_t &chain)
{
    if (chain.empty())
    {
        fprintf(stderr, "Empty chain %p\n", &chain);
        return;
    }

    fprintf(stderr, "Chain %p (%ld items):\n", &chain, (long)chain.size());
    for (size_t i=0; i < chain.size(); i++)
    {
        const shared_ptr<const io_data_t> &io = chain.at(i);
        fprintf(stderr, "\t%lu: fd:%d, input:%s, ", (unsigned long)i, io->fd, io->is_input ? "yes" : "no");
        io->print();
    }
}

void io_duplicate_prepend(const io_chain_t &src, io_chain_t &dst)
{
    return dst.duplicate_prepend(src);
}

void io_chain_destroy(io_chain_t &chain)
{
    chain.destroy();
}

/* Return the last IO for the given fd */
shared_ptr<const io_data_t> io_chain_t::get_io_for_fd(int fd) const
{
    size_t idx = this->size();
    while (idx--)
    {
        const shared_ptr<const io_data_t> &data = this->at(idx);
        if (data->fd == fd)
        {
            return data;
        }
    }
    return shared_ptr<const io_data_t>();
}

shared_ptr<io_data_t> io_chain_t::get_io_for_fd(int fd)
{
    size_t idx = this->size();
    while (idx--)
    {
        const shared_ptr<io_data_t> &data = this->at(idx);
        if (data->fd == fd)
        {
            return data;
        }
    }
    return shared_ptr<io_data_t>();
}

/* The old function returned the last match, so we mimic that. */
shared_ptr<const io_data_t> io_chain_get(const io_chain_t &src, int fd)
{
    return src.get_io_for_fd(fd);
}

shared_ptr<io_data_t> io_chain_get(io_chain_t &src, int fd)
{
    return src.get_io_for_fd(fd);
}

io_chain_t::io_chain_t(const shared_ptr<io_data_t> &data) :
    std::vector<shared_ptr<io_data_t> >(1, data)
{

}

io_chain_t::io_chain_t() : std::vector<shared_ptr<io_data_t> >()
{
}
