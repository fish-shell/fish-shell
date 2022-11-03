# SPDX-FileCopyrightText: © 2016 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# Helper function for completions that need to enumerate Linux modules
function __fish_print_modules
    find /lib/modules/(uname -r)/{kernel,misc} -type f 2>/dev/null | sed -e 's$/.*/\([^/.]*\).*$\1$'
end
