# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#
# These are very common and useful
#
function ll --wraps ls --description "List contents of directory using long format"
    ls -lh $argv
end
