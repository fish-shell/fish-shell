#ifndef FISH_EDITABLE_LINE_H
#define FISH_EDITABLE_LINE_H

struct HighlightSpecListFFI;

#if INCLUDE_RUST_HEADERS
#include "editable_line.rs.h"
#else
struct Edit;
struct UndoHistory;
struct EditableLine;
#endif

using edit_t = Edit;
using undo_history_t = UndoHistory;
using editable_line_t = EditableLine;

#endif
