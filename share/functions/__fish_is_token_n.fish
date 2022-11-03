# SPDX-FileCopyrightText: © 2012 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_is_token_n --description 'Test if current token is on Nth place' --argument-names n
    __fish_is_nth_token $n
end
