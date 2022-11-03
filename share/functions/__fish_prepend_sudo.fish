# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_prepend_sudo -d " DEPRECATED: use fish_commandline_prepend instead. Prepend 'sudo ' to the beginning of the current commandline"
    fish_commandline_prepend sudo
end
