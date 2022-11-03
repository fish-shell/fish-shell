# SPDX-FileCopyrightText: © 2017 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_complete_zfs_pools -d "Completes with available ZFS pools"
    zpool list -o name,comment -H | string replace -a \t'-' ''
end
