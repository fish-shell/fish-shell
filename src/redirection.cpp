#include "config.h"  // IWYU pragma: keep

#include "redirection.h"
#include "wutil.h"

#include <fcntl.h>

/// File descriptor redirection error message.
#define FD_ERROR "An error occurred while redirecting file descriptor %s"

/// Pipe error message.
#define LOCAL_PIPE_ERROR "An error occurred while setting up pipe"

#define NOCLOB_ERROR _(L"The file '%s' already exists")

#define FILE_ERROR _(L"An error occurred while redirecting file '%s'")

/// Base open mode to pass to calls to open.
#define OPEN_MASK 0666

dup2_list_t::~dup2_list_t() = default;

maybe_t<dup2_list_t> dup2_list_t::resolve_chain(const io_chain_t &io_chain) {
    ASSERT_IS_NOT_FORKED_CHILD();
    dup2_list_t result;
    for (const auto &io_ref : io_chain) {
        switch (io_ref->io_mode) {
            case io_mode_t::file: {
                // Here we definitely do not want to set CLO_EXEC because our child needs access.
                // Open the file.
                const io_file_t *io_file = static_cast<const io_file_t *>(io_ref.get());
                int file_fd = open(io_file->filename_cstr, io_file->flags, OPEN_MASK);
                if (file_fd < 0) {
                    if ((io_file->flags & O_EXCL) && (errno == EEXIST)) {
                        debug(1, NOCLOB_ERROR, io_file->filename_cstr);
                    } else {
                        debug(1, FILE_ERROR, io_file->filename_cstr);
                        if (should_debug(1)) wperror(L"open");
                    }
                    return none();
                }

                // If by chance we got the file we want, we're done. Otherwise move the fd to an unused place and dup2 it.
                // Note move_fd_to_unused() will close the incoming file_fd.
                if (file_fd != io_file->fd) {
                    file_fd = move_fd_to_unused(file_fd, io_chain, false /* cloexec */);
                    if (file_fd < 0) {
                        debug(1, FILE_ERROR, io_file->filename_cstr);
                        if (should_debug(1)) wperror(L"dup");
                        return none();
                    }
                }

                // Record that we opened this file, so we will auto-close it.
                assert(file_fd >= 0 && "Should have a valid file_fd");
                result.opened_fds_.emplace_back(file_fd);

                // Mark our dup2 and our close actions.
                result.add_dup2(file_fd, io_file->fd);
                result.add_close(file_fd);
                break;
            }

            case io_mode_t::close: {
                const io_close_t *io = static_cast<const io_close_t *>(io_ref.get());
                result.add_close(io->fd);
                break;
            }

            case io_mode_t::fd: {
                const io_fd_t *io = static_cast<const io_fd_t *>(io_ref.get());
                result.add_dup2(io->old_fd, io->fd);
                break;
            }

            case io_mode_t::buffer:
            case io_mode_t::pipe: {
                const io_pipe_t *io = static_cast<const io_pipe_t *>(io_ref.get());
                // If write_pipe_idx is 0, it means we're connecting to the read end (first pipe
                // fd). If it's 1, we're connecting to the write end (second pipe fd).
                unsigned int write_pipe_idx = (io->is_input ? 0 : 1);
                result.add_dup2(io->pipe_fd[write_pipe_idx], io->fd);
                if (io->pipe_fd[0] >= 0) result.add_close(io->pipe_fd[0]);
                if (io->pipe_fd[1] >= 0) result.add_close(io->pipe_fd[1]);
                break;
            }
        }
    }
    return result;
}
