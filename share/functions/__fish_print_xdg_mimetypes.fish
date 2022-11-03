# SPDX-FileCopyrightText: © 2012 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_xdg_mimetypes --description 'Print XDG mime types'
    set -l files (__fish_print_xdg_applications_directories)/mimeinfo.cache
    # If we have no file, don't run `cat` without arguments!
    set -q files[1] || return 1
    cat $files 2>/dev/null | string match -v '[MIME Cache]' | string replace = \t
end
