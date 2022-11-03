# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
#
# SPDX-License-Identifier: GPL-2.0-only

#completion for pkgmk

complete -f -c pkgmk -o i -l install -d 'Install with pkgadd after build'
complete -f -c pkgmk -o u -l upgrade -d 'Upgrade with pkgadd after build'
complete -f -c pkgmk -o r -l recursive -d 'Search and build packages recursively'
complete -f -c pkgmk -o d -l download -d 'Download the sources'
complete -f -c pkgmk -o do -l download-only -d 'Only download the sources'
complete -f -c pkgmk -o utd -l up-to-date -d 'Check if the package is uptodate'
complete -f -c pkgmk -o uf -l update-footprint -d 'Update footprint'
complete -f -c pkgmk -o if -l ignore-footprint -d 'Ignore footprint'
complete -f -c pkgmk -o um -l update-md5sum -d 'Update md5sum'
complete -f -c pkgmk -o im -l ignore-md5sum -d 'Ignore md5sum'
complete -f -c pkgmk -o ns -l no-strip -d 'Do not strip executables'
complete -f -c pkgmk -o f -l force -d 'Force rebuild'
complete -f -c pkgmk -o c -l clean -d 'Remove package and sources'
complete -f -c pkgmk -o kw -l keep-work -d 'Keep working dir'
complete -f -c pkgmk -o cf -l config-file -r -d 'Use another config'
complete -f -c pkgmk -o v -l version -d 'Print version'
complete -f -c pkgmk -o h -l help -d 'Print help'
