# SPDX-FileCopyrightText: © 2015 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_pacman_repos --description "Print the repositories configured for arch's pacman package manager"
    string match -er "\[.*\]" </etc/pacman.conf | string match -r -v "^#|options" | string replace -ra "\[|\]" ""
end
