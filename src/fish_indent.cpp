// The fish_indent program.
/*
Copyright (C) 2014 ridiculous_fish

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ast.h"
#include "env.h"
#include "env/env_ffi.rs.h"
#include "fds.h"
#include "ffi_baggage.h"
#include "ffi_init.rs.h"
#include "fish_indent.rs.h"
#include "fish_version.h"
#include "flog.h"
#include "future_feature_flags.h"
#include "highlight.h"
#include "operation_context.h"
#include "print_help.rs.h"
#include "tokenizer.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

int main() {
    program_name = L"fish_indent";
    return fish_indent_main();
}
