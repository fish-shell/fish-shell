// Support for exposing the terminal size.

#include "termsize.h"

#include "maybe.h"
#include "parser.h"
#include "wcstringutil.h"
#include "wutil.h"

// A counter which is incremented every SIGWINCH, or when the tty is otherwise invalidated.
static volatile uint32_t s_tty_termsize_gen_count{0};

/// \return a termsize from ioctl, or none on error or if not supported.
static maybe_t<termsize_t> read_termsize_from_tty() {
    maybe_t<termsize_t> result{};
#ifdef HAVE_WINSIZE
    struct winsize winsize = {0, 0, 0, 0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) >= 0) {
        result = termsize_t{winsize.ws_col, winsize.ws_row};
    }
#endif
    return result;
}

// static
termsize_container_t &termsize_container_t::shared() {
    // Heap-allocated to avoid runtime dtor registration.
    static auto *res = new termsize_container_t(read_termsize_from_tty);
    return *res;
}

termsize_t termsize_container_t::data_t::current() const {
    // This encapsulates our ordering logic. If we have a termsize from a tty, use it; otherwise use
    // what we have seen from the environment.
    if (this->last_from_tty) return *this->last_from_tty;
    if (this->last_from_env) return *this->last_from_env;
    return termsize_t::defaults();
}

void termsize_container_t::data_t::mark_override_from_env(termsize_t ts) {
    // Here we pretend to have an up-to-date tty value so that we will prefer the environment value.
    this->last_from_env = ts;
    this->last_from_tty.reset();
    this->last_tty_gen_count = s_tty_termsize_gen_count;
}

termsize_t termsize_container_t::last() const { return this->data_.acquire()->current(); }

termsize_t termsize_container_t::updating(parser_t &parser) {
    termsize_t new_size = termsize_t::defaults();
    termsize_t prev_size = termsize_t::defaults();

    // Take the lock in a local region.
    // Capture the size before and the new size.
    {
        auto data = data_.acquire();
        prev_size = data->current();

        // Critical read of signal-owned variable.
        // This must happen before the TIOCGWINSZ ioctl.
        const uint32_t tty_gen_count = s_tty_termsize_gen_count;
        if (data->last_tty_gen_count != tty_gen_count) {
            // Our idea of the size of the terminal may be stale.
            // Apply any updates.
            data->last_tty_gen_count = tty_gen_count;
            data->last_from_tty = this->tty_size_reader_();
        }
        new_size = data->current();
    }

    // Announce any updates.
    if (new_size != prev_size) set_columns_lines_vars(new_size, parser);
    return new_size;
}

void termsize_container_t::set_columns_lines_vars(termsize_t val, parser_t &parser) {
    const bool saved = setting_env_vars_;
    setting_env_vars_ = true;
    parser.set_var_and_fire(L"COLUMNS", ENV_GLOBAL, to_string(val.width));
    parser.set_var_and_fire(L"LINES", ENV_GLOBAL, to_string(val.height));
    setting_env_vars_ = saved;
}

/// Convert an environment variable to an int, or return a default value.
/// The int must be >0 and <USHRT_MAX (from struct winsize).
static int var_to_int_or(const maybe_t<env_var_t> &var, int def) {
    if (var.has_value() && !var->empty()) {
        errno = 0;
        int proposed = fish_wcstoi(var->as_string().c_str());
        if (errno == 0 && proposed > 0 && proposed <= USHRT_MAX) {
            return proposed;
        }
    }
    return def;
}

termsize_t termsize_container_t::initialize(const environment_t &vars) {
    termsize_t new_termsize{
        var_to_int_or(vars.get(L"COLUMNS", ENV_GLOBAL), -1),
        var_to_int_or(vars.get(L"LINES", ENV_GLOBAL), -1),
    };
    auto data = data_.acquire();
    if (new_termsize.width > 0 && new_termsize.height > 0) {
        data->mark_override_from_env(new_termsize);
    } else {
        data->last_tty_gen_count = s_tty_termsize_gen_count;
        data->last_from_tty = this->tty_size_reader_();
    }
    return data->current();
}

void termsize_container_t::handle_columns_lines_var_change(const environment_t &vars) {
    // Do nothing if we are the ones setting it.
    if (setting_env_vars_) return;

    // Construct a new termsize from COLUMNS and LINES, then set it in our data.
    termsize_t new_termsize{
        var_to_int_or(vars.get(L"COLUMNS", ENV_GLOBAL), termsize_t::DEFAULT_WIDTH),
        var_to_int_or(vars.get(L"LINES", ENV_GLOBAL), termsize_t::DEFAULT_HEIGHT),
    };

    // Store our termsize as an environment override.
    data_.acquire()->mark_override_from_env(new_termsize);
}

// static
void termsize_container_t::handle_winch() { s_tty_termsize_gen_count += 1; }

// static
void termsize_container_t::invalidate_tty() { s_tty_termsize_gen_count += 1; }
