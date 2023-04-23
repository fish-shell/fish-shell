#include "null_terminated_array.h"

std::vector<std::string> wide_string_list_to_narrow(const std::vector<wcstring> &strs) {
    std::vector<std::string> res;
    res.reserve(strs.size());
    for (const wcstring &s : strs) {
        res.push_back(wcs2zstring(s));
    }
    return res;
}

const char **owning_null_terminated_array_t::get() { return impl_->get(); }

owning_null_terminated_array_t::owning_null_terminated_array_t(std::vector<std::string> &&strings)
    : impl_(new_owning_null_terminated_array(strings)) {}

owning_null_terminated_array_t::owning_null_terminated_array_t(
    rust::Box<OwningNullTerminatedArrayRef> impl)
    : impl_(std::move(impl)) {}
