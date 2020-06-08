#include "null_terminated_array.h"

template <typename CharT>
static CharT **make_null_terminated_array_helper(
    const std::vector<std::basic_string<CharT> > &argv) {
    size_t count = argv.size();

    // We allocate everything in one giant block. First compute how much space we need.
    // N + 1 pointers.
    size_t pointers_allocation_len = (count + 1) * sizeof(CharT *);

    // In the very unlikely event that CharT has stricter alignment requirements than does a
    // pointer, round us up to the size of a CharT.
    pointers_allocation_len += sizeof(CharT) - 1;
    pointers_allocation_len -= pointers_allocation_len % sizeof(CharT);

    // N null terminated strings.
    size_t strings_allocation_len = 0;
    for (size_t i = 0; i < count; i++) {
        // The size of the string, plus a null terminator.
        strings_allocation_len += (argv.at(i).size() + 1) * sizeof(CharT);
    }

    // Now allocate their sum.
    auto base =
        static_cast<unsigned char *>(malloc(pointers_allocation_len + strings_allocation_len));
    if (!base) return nullptr;

    // Divvy it up into the pointers and strings.
    auto pointers = reinterpret_cast<CharT **>(base);
    auto strings = reinterpret_cast<CharT *>(base + pointers_allocation_len);

    // Start copying.
    for (size_t i = 0; i < count; i++) {
        const std::basic_string<CharT> &str = argv.at(i);
        *pointers++ = strings;  // store the current string pointer into self
        strings = std::copy(str.begin(), str.end(), strings);  // copy the string into strings
        *strings++ = static_cast<CharT>(0);  // each string needs a null terminator
    }
    *pointers++ = nullptr;  // array of pointers needs a null terminator

    // Make sure we know what we're doing.
    assert(reinterpret_cast<unsigned char *>(pointers) - base ==
           static_cast<std::ptrdiff_t>(pointers_allocation_len));
    assert(reinterpret_cast<unsigned char *>(strings) -
               reinterpret_cast<unsigned char *>(pointers) ==
           static_cast<std::ptrdiff_t>(strings_allocation_len));
    assert(reinterpret_cast<unsigned char *>(strings) - base ==
           static_cast<std::ptrdiff_t>(pointers_allocation_len + strings_allocation_len));

    return reinterpret_cast<CharT **>(base);
}

wchar_t **make_null_terminated_array(const wcstring_list_t &lst) {
    return make_null_terminated_array_helper(lst);
}

char **make_null_terminated_array(const std::vector<std::string> &lst) {
    return make_null_terminated_array_helper(lst);
}

void convert_wide_array_to_narrow(const null_terminated_array_t<wchar_t> &wide_arr,
                                  null_terminated_array_t<char> *output) {
    const wchar_t *const *arr = wide_arr.get();
    if (!arr) {
        output->clear();
        return;
    }

    std::vector<std::string> list;
    for (size_t i = 0; arr[i]; i++) {
        list.push_back(wcs2string(arr[i]));
    }
    output->set(list);
}
