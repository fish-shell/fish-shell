# SPDX-FileCopyrightText: © 2017 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c signify -n __fish_seen_subcommand_from -s C -d 'Verify a signed checksum list'
complete -c signify -n __fish_seen_subcommand_from -s G -d 'Generate a new key pair'
complete -c signify -n __fish_seen_subcommand_from -s S -d 'Sign specified message'
complete -c signify -n __fish_seen_subcommand_from -s V -d 'Verify a signed message and sig'
