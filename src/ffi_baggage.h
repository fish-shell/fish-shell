#include "builtin.h"
#include "builtins/bind.h"
#include "builtins/commandline.h"
#include "event.h"
#include "fds.h"
#include "highlight.h"
#include "input.h"
#include "parse_util.h"
#include "reader.h"
#include "screen.h"

// Symbols that get autocxx bindings but are not used in a given binary, will cause "undefined
// reference" when trying to link that binary. Work around this by marking them as used in
// all binaries.
void mark_as_used(const parser_t& parser, env_stack_t& env_stack) {
    wcstring s;

    event_fire_generic(parser, {});
    event_fire_generic(parser, {}, {});
    expand_tilde(s, env_stack);
    get_history_variable_text_ffi({});
    highlight_spec_t{};
    init_input();
    reader_change_cursor_selection_mode(cursor_selection_mode_t::exclusive);
    reader_change_history({});
    reader_read_ffi({}, {}, {});
    reader_schedule_prompt_repaint();
    reader_set_autosuggestion_enabled_ffi({});
    reader_status_count();
    restore_term_mode();
    rgb_color_t{};
    setenv_lock({}, {}, {});
    set_inheriteds_ffi();
    term_copy_modes();
    unsetenv_lock({});

    builtin_bind({}, {}, {});
    builtin_commandline({}, {}, {});
}
