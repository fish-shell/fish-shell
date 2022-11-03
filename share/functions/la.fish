# SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#
# These are very common and useful
#
function la --wraps ls --description "List contents of directory, including hidden files in directory using long format"
    ls -lAh $argv
end
