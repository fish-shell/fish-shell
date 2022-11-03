# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

set -l valid_subcmds up down strip save

function __fish_wg_complete_interfaces
    wg show interfaces
end

complete -c wg-quick -f
complete -c wg-quick -n "__fish_seen_subcommand_from $valid_subcmds" -a '(__fish_wg_complete_interfaces)'
complete -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a up -d 'Add and set up an interface'
complete -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a down -d 'Tear down and remove an interface'
complete -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a strip -d 'Output config for use with wg'
complete -c wg-quick -n "not __fish_seen_subcommand_from $valid_subcmds" -a save -d 'Saves the configuration of an existing interface'
