# SPDX-FileCopyrightText: © 2021 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c monop -s r -d 'Specifies the assembly to use for looking up the type'
complete -c monop -s a -d 'Renders all of the types in the specified assembly'
complete -c monop -s s -s k -l search -d 'Searches through all known assemblies for types containing \'class\''
complete -c monop -l refs -d 'Print a list of the referenced assemblies for an assembly.'
complete -c monop -s f -l filter-obsolete -d 'Do not show obsolete types and members'
complete -c monop -s d -l declared-only -d 'Only show members declared in the type'
complete -c monop -s p -l private -d 'Show private members'
complete -c monop -l runtime-version -d 'Print runtime version'
complete -c monop -o xa -d 'Set the lookup path to the Xamarin.Android directory'
complete -c monop -o xi -d 'Set the lookup path to the Xamarin.iOS directory'
