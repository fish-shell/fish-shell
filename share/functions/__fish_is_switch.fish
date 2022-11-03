# SPDX-FileCopyrightText: © 2018 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# Whether or not the current token is a switch
function __fish_is_switch
    string match -qr -- '^-' ""(commandline -ct)
end
