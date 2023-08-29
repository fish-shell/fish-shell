#include "fish_indent_common.h"

// Symbols that get autocxx bindings but are not used in a given binary, will cause "undefined
// reference" when trying to link that binary. Work around this by marking them as used in
// all binaries.
void mark_as_used() {
    //
    pretty_printer_t({}, {});
}
