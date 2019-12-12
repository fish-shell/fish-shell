#include "config.h"  // IWYU pragma: keep

#include "redirection.h"

#include <fcntl.h>

#include "wutil.h"

#define NOCLOB_ERROR _(L"The file '%ls' already exists")

#define FILE_ERROR _(L"An error occurred while redirecting file '%ls'")

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
                autoclose_fd_t file_fd{wopen(io_file->filename, io_file->flags, OPEN_MASK)};
                if (!file_fd.valid()) {
                    if ((io_file->flags & O_EXCL) && (errno == EEXIST)) {
                        debug(1, NOCLOB_ERROR, io_file->filename.c_str());
                    } else {
                        debug(1, FILE_ERROR, io_file->filename.c_str());
                        if (should_debug(1)) wperror(L"open");
                    }
                    return none();
                }

                // If by chance we got the file we want, we're done. Otherwise move the fd to an
                // unused place and dup2 it.
                // Note move_fd_to_unused() will close the incoming file_fd.
                if (file_fd.fd() != io_file->fd) {
                    file_fd = move_fd_to_unused(std::move(file_fd), io_chain.fd_set(),
                                                false /* cloexec */);
                    if (!file_fd.valid()) {
                        debug(1, FILE_ERROR, io_file->filename.c_str());
                        if (should_debug(1)) wperror(L"dup");
                        return none();
                    }
                }
                assert(file_fd.valid() && "Should have a valid file_fd");

                // Mark our dup2 and our close actions.
                result.add_dup2(file_fd.fd(), io_file->fd);
                result.add_close(file_fd.fd());

                // Store our file.
                result.opened_fds_.push_back(std::move(file_fd));
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

            case io_mode_t::pipe: {
                const io_pipe_t *io = static_cast<const io_pipe_t *>(io_ref.get());
                result.add_dup2(io->pipe_fd(), io->fd);
                result.add_close(io->pipe_fd());
                break;
            }

            case io_mode_t::bufferfill: {
                const io_bufferfill_t *io = static_cast<const io_bufferfill_t *>(io_ref.get());
                result.add_dup2(io->write_fd(), io->fd);
                result.add_close(io->write_fd());
                break;
            }
        }
    }
    return {std::move(result)};
}

int dup2_list_t::fd_for_target_fd(int target) const {
    // Paranoia.
    if (target < 0) {
        return target;
    }
    // Note we can simply walk our action list backwards, looking for src -> target dups.
    int cursor = target;
    for (auto iter = actions_.rbegin(); iter != actions_.rend(); ++iter) {
        if (iter->target == cursor) {
            // cursor is replaced by iter->src
            cursor = iter->src;
        } else if (iter->src == cursor && iter->target < 0) {
            // cursor is closed.
            cursor = -1;
            break;
        }
    }
    return cursor;
}
