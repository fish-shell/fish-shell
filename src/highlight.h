// Prototypes for functions for syntax highlighting.
#ifndef FISH_HIGHLIGHT_H
#define FISH_HIGHLIGHT_H

#include <stddef.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "color.h"
#include "cxx.h"
#include "env.h"
#include "flog.h"
#include "history.h"
#include "maybe.h"
#include "operation_context.h"
#include "parser.h"

struct highlight_spec_t;

#if INCLUDE_RUST_HEADERS
#include "highlight.rs.h"
#else
struct HighlightSpec;
enum class HighlightRole : uint8_t;
#endif

using highlight_role_t = HighlightRole;
// using highlight_spec_t = HighlightSpec;

struct highlight_spec_t {
    rust::Box<HighlightSpec> val;

    highlight_spec_t();
    highlight_spec_t(const highlight_spec_t &other);
    highlight_spec_t &operator=(const highlight_spec_t &other);

    highlight_spec_t(HighlightSpec other);
    highlight_spec_t(highlight_role_t fg, highlight_role_t bg = {});

    bool operator==(const highlight_spec_t &other) const;
    bool operator!=(const highlight_spec_t &other) const;

    HighlightSpec *operator->() { return &*this->val; }
    const HighlightSpec *operator->() const { return &*this->val; }

    operator HighlightSpec() const;

    static highlight_spec_t make_background(highlight_role_t bg_role);
};

void highlight_shell(const wcstring &buff, std::vector<highlight_spec_t> &colors,
                     const operation_context_t &ctx, bool io_ok = false,
                     std::shared_ptr<size_t> cursor = {});

#endif
