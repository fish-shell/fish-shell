// Support for exposing the terminal size.

#include "config.h"  // IWYU pragma: keep
#ifndef FISH_TERMSIZE_H
#define FISH_TERMSIZE_H

#include <stdint.h>

#include "common.h"
#include "global_safety.h"

class environment_t;
class parser_t;
struct termsize_tester_t;

/// A simple value type wrapping up a terminal size.
struct termsize_t {
    /// Default width and height.
    static constexpr int DEFAULT_WIDTH = 80;
    static constexpr int DEFAULT_HEIGHT = 24;

    /// width of the terminal, in columns.
    int width{DEFAULT_WIDTH};

    /// height of the terminal, in rows.
    int height{DEFAULT_HEIGHT};

    /// Construct from width and height.
    termsize_t(int w, int h) : width(w), height(h) {}

    /// Return a default-sized termsize.
    static termsize_t defaults() { return termsize_t{DEFAULT_WIDTH, DEFAULT_HEIGHT}; }

    bool operator==(termsize_t rhs) const {
        return this->width == rhs.width && this->height == rhs.height;
    }

    bool operator!=(termsize_t rhs) const { return !(*this == rhs); }
};

/// Termsize monitoring is more complicated than one may think.
/// The main source of complexity is the interaction between the environment variables COLUMNS/ROWS,
/// the WINCH signal, and the TIOCGWINSZ ioctl.
/// Our policy is "last seen wins": if COLUMNS or LINES is modified, we respect that until we get a
/// SIGWINCH.
struct termsize_container_t {
    /// \return the termsize without applying any updates.
    /// Return the default termsize if none.
    termsize_t last() const;

    /// If our termsize is stale, update it, using \p parser firing any events that may be
    /// registered for COLUMNS and LINES.
    /// \return the updated termsize.
    termsize_t updating(parser_t &parser);

    /// Initialize our termsize, using the given environment stack.
    /// This will prefer to use COLUMNS and LINES, but will fall back to the tty size reader.
    /// This does not change any variables in the environment.
    termsize_t initialize(const environment_t &vars);

    /// Note that a WINCH signal is received.
    /// Naturally this may be called from within a signal handler.
    static void handle_winch();

    /// Invalidate the tty in the sense that we need to re-fetch its termsize.
    static void invalidate_tty();

    /// Note that COLUMNS and/or LINES global variables changed.
    void handle_columns_lines_var_change(const environment_t &vars);

    /// \return the singleton shared container.
    static termsize_container_t &shared();

   private:
    /// A function used for accessing the termsize from the tty. This is only exposed for testing.
    using tty_size_reader_func_t = maybe_t<termsize_t> (*)();

    struct data_t {
        // The last termsize returned by TIOCGWINSZ, or none if none.
        maybe_t<termsize_t> last_from_tty{};

        // The last termsize seen from the environment (COLUMNS/LINES), or none if none.
        maybe_t<termsize_t> last_from_env{};

        // The last-seen tty-invalidation generation count.
        // Set to a huge value so it's initially stale.
        uint32_t last_tty_gen_count{UINT32_MAX};

        /// \return the current termsize from this data.
        termsize_t current() const;

        /// Mark that our termsize is (for the time being) from the environment, not the tty.
        void mark_override_from_env(termsize_t ts);
    };

    // Construct from a reader function.
    explicit termsize_container_t(tty_size_reader_func_t func) : tty_size_reader_(func) {}

    // Update COLUMNS and LINES in the parser's stack.
    void set_columns_lines_vars(termsize_t val, parser_t &parser);

    // Our lock-protected data.
    mutable owning_lock<data_t> data_{};

    // An indication that we are currently in the process of setting COLUMNS and LINES, and so do
    // not react to any changes.
    relaxed_atomic_bool_t setting_env_vars_{false};
    const tty_size_reader_func_t tty_size_reader_;

    friend termsize_tester_t;
};

/// Convenience helper to return the last known termsize.
inline termsize_t termsize_last() { return termsize_container_t::shared().last(); }

#endif  // FISH_TERMSIZE_H
