# SPDX-FileCopyrightText: © 2017 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#completion for pkg_add

complete -c pkg_add -o D -d 'failsafe to overwrite' -xa 'allversions arch checksum dontmerge donttie downgrade installed libdepends nonroot paranoid repair scripts SIGNER snap unsigned updatedepends'
complete -c pkg_add -o V -d 'Turn on stats'
complete -c pkg_add -o a -d 'Automated package installation'
complete -c pkg_add -o h -d 'Print help'
complete -c pkg_add -o u -d 'Update packages'
complete -c pkg_add -o z -d 'Fuzzy match'
