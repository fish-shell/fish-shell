#include "highlight.h"

highlight_spec_t::highlight_spec_t() : val(new_highlight_spec()) {}
highlight_spec_t::highlight_spec_t(const highlight_spec_t &other) : val(new_highlight_spec()) {
    *this = other;
}
highlight_spec_t &highlight_spec_t::operator=(const highlight_spec_t &other) {
    this->val = other.val->clone();
    return *this;
}

highlight_spec_t::highlight_spec_t(HighlightSpec other) : val(other.clone()) {}
highlight_spec_t::highlight_spec_t(highlight_role_t fg, highlight_role_t bg)
    : val(new_highlight_spec()) {
    val->foreground = fg;
    val->background = bg;
}

highlight_spec_t::operator HighlightSpec() const { return *val; }

highlight_spec_t highlight_spec_t::make_background(highlight_role_t bg_role) {
    return highlight_spec_t{highlight_role_t::normal, bg_role};
}

bool highlight_spec_t::operator==(const highlight_spec_t &other) const {
    return *this->val == *other.val;
}
bool highlight_spec_t::operator!=(const highlight_spec_t &other) const { return !(*this == other); }

void highlight_shell(const wcstring &buff, std::vector<highlight_spec_t> &colors,
                     const operation_context_t &ctx, bool io_ok, std::shared_ptr<size_t> cursor) {
    auto ffi_colors = highlight_shell_ffi(buff, ctx, io_ok, cursor);
    colors.resize(ffi_colors->size());
    for (size_t i = 0; i < ffi_colors->size(); i++) colors[i] = ffi_colors->at(i);
}
