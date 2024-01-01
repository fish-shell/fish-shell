// Prototypes for various functions, mostly string utilities, that are used by most parts of fish.
#ifndef FISH_COMMON_H
#define FISH_COMMON_H
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <limits.h>
// Needed for va_list et al.
#include <stdarg.h>  // IWYU pragma: keep
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>  // IWYU pragma: keep
#endif
#include <sys/types.h>
#include <termios.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifndef HAVE_STD__MAKE_UNIQUE
/// make_unique implementation
namespace std {
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&...args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace std
#endif
using std::make_unique;

#endif  // FISH_COMMON_H
