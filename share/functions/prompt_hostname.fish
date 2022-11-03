# SPDX-FileCopyrightText: © 2016 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function prompt_hostname --description 'short hostname for the prompt'
    string replace -r "\..*" "" $hostname
end
