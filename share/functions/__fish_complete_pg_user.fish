# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_complete_pg_user
    psql -Atqwc 'select usename from pg_user' template1 2>/dev/null
end
