# Completions for Gentoo's equery tool

# Author: Tassilo Horn <tassilo@member.fsf.org>

function __fish_equery_print_installed_pkgs --description 'Prints completions for installed packages on the system from /var/db/pkg'
 if test -d /var/db/pkg
   find /var/db/pkg/ -type d | cut -d'/' -f5-6 | sort -u | sed 's/-[0-9]\{1,\}\..*$//'
   return
 end
end

function __fish_equery_print_all_pkgs --description 'Prints completions for all available packages on the system from /usr/portage'
 if test -d /usr/portage
   find /usr/portage/ -maxdepth 2 -type d | cut -d'/' -f4-5 | sed 's/^\(distfiles\|profiles\|eclass\).*$//' | sort -u | sed 's/-[0-9]\{1,\}\..*$//'
   return
 end
end

function __fish_equery_print_all_categories --description 'Prints completions for all available categories on the system from /usr/portage'
 if test -d /usr/portage
   find /usr/portage/ -maxdepth 1 -type d | cut -d'/' -f4 | sed 's/^\(distfiles\|profiles\|eclass\).*$//' | sort -u | sed 's/-[0-9]\{1,\}\..*$//'
   return
 end
end

#########################################################################
# Global Opts
complete -c equery -s q -l quit -d "causes minimal output to be emitted"
complete -c equery -s C -l nocolor -d "turns off colours"
complete -c equery -s h -l help -d "displays a help summary"
complete -c equery -s V -l version -d "displays the equery version"
complete -c equery -s N -l no-pipe -d "turns off pipe detection"
# END Global Opts
#########################################################################

#########################################################################
# Subcommands
complete -c equery -n '__fish_use_subcommand' -xa 'belongs\t"'(_ "list all packages owning file(s)")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'check\t"'(_ "check MD5sums and timestamps of package")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'depends\t"'(_ "list all packages depending on specified package")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'depgraph\t"'(_ "display a dependency tree for package")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'files\t"'(_ "list files owned by package")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'hasuse\t"'(_ "list all packages with specified useflag")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'list\t"'(_ "list all packages matching pattern")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'size\t"'(_ "print size of files contained in package")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'uses\t"'(_ "display USE flags for package")'"'
complete -c equery -n '__fish_use_subcommand' -xa 'which\t"'(_ "print full path to ebuild for package")'"'
# END Subcommands
#########################################################################

#########################################################################
# Local opts
# belongs
complete -c equery -n '__fish_seen_subcommand_from belongs' -s c -l category -xa '(__fish_equery_print_all_categories)' -d "only search in given category"
complete -c equery -n '__fish_seen_subcommand_from belongs' -s f -l full-regex -d "supplied query is a regex"
complete -c equery -n '__fish_seen_subcommand_from belongs' -s e -l earlyout -d "stop when first match found"

# depends
complete -c equery -n '__fish_seen_subcommand_from depends' -s a -l all-packages -d "search in all available packages (slow)"
complete -c equery -n '__fish_seen_subcommand_from depends' -s d -l direct -d "search direct dependencies only (default)"
complete -c equery -n '__fish_seen_subcommand_from depends' -s D -l indirect -d "search indirect dependencies (very slow)"

# depgraph
complete -c equery -n '__fish_seen_subcommand_from depgraph' -s U -l no-useflags -d "do not show USE flags"
complete -c equery -n '__fish_seen_subcommand_from depgraph' -s l -l linear -d "do not use fancy formatting"

# files
complete -c equery -n '__fish_seen_subcommand_from files' -l timestamp -d "output the timestamp of each file"
complete -c equery -n '__fish_seen_subcommand_from files' -l md5sum -d "output the md5sum of each file"
complete -c equery -n '__fish_seen_subcommand_from files' -l type -d "output the type of each file"
complete -c equery -n '__fish_seen_subcommand_from files' -l filter -xa "dir obj sym dev fifo path conf cmd doc man info" -d "filter output based on files type or path (comma separated list)"

# hasuse
complete -c equery -n '__fish_seen_subcommand_from hasuse' -s i -l installed -d "search installed packages (default)"
complete -c equery -n '__fish_seen_subcommand_from hasuse' -s I -l exclude-installed -d "do not search installed packages"
complete -c equery -n '__fish_seen_subcommand_from hasuse' -s p -l portage-tree -d "also search in portage tree (/usr/portage)"
complete -c equery -n '__fish_seen_subcommand_from hasuse' -s o -l overlay-tree -d "also search in overlay tree (/usr/local/portage)"

# list
complete -c equery -n '__fish_seen_subcommand_from list' -s i -l installed -d "search installed packages (default)"
complete -c equery -n '__fish_seen_subcommand_from list' -s I -l exclude-installed -d "do not search installed packages"
complete -c equery -n '__fish_seen_subcommand_from list' -s p -l portage-tree -d "also search in portage tree (/usr/portage)"
complete -c equery -n '__fish_seen_subcommand_from list' -s o -l overlay-tree -d "also search in overlay tree (/usr/local/portage)"
complete -c equery -n '__fish_seen_subcommand_from list' -s f -l full-regex -d "query is a regular expression"
complete -c equery -n '__fish_seen_subcommand_from list' -s e -l exact-name -d "list only those packages that exactly match"
complete -c equery -n '__fish_seen_subcommand_from list' -s d -l duplicates -d "only list installed duplicate packages"

# size
complete -c equery -n '__fish_seen_subcommand_from size' -s b -l bytes -d "report size in bytes"

# uses
complete -c equery -n '__fish_seen_subcommand_from uses' -s a -l all -d "include non-installed packages"
# END Local opts
#########################################################################

#########################################################################
# Arguments
# Commands applied to a package which doesn't need to be installed
complete -c equery -n '__fish_seen_subcommand_from depends depgraph hasuse which' -xa '(__fish_equery_print_all_pkgs)'

# Commands applied to a package which has to be installed
complete -c equery -n '__fish_seen_subcommand_from check files uses size' -xa '(__fish_equery_print_installed_pkgs)'
# END Arguments
#########################################################################