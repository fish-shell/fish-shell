#include "builtin.h"
#include "event.h"
#include "fds.h"
#include "highlight.h"
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
    highlight_spec_t{};
    rgb_color_t{};
    setenv_lock({}, {}, {});
    set_inheriteds_ffi();
    unsetenv_lock({});
    rgb_color_t::white();
    rgb_color_t{};
}
