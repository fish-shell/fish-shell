# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

echo broken
if status is-login
    syntax-error
end
