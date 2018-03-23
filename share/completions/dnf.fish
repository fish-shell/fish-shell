#
# Completions for the dnf Fedora 27 
# 
# by celso.lom - 2018
#
# Need the sqlite3 package

# Check commands to return package list
function __fish_dnf_package_installed
	not __fish_seen_subcommand_from group; and __fish_seen_subcommand_from update upgrade remove erase reinstall
end

function __fish_dnf_package_notinstalled
	not __fish_seen_subcommand_from group; and __fish_seen_subcommand_from install
end

function __fish_dnf_package_allavailable
	not __fish_seen_subcommand_from group history; and __fish_seen_subcommand_from info deplist
end


# Return installed packages
function __dnf_return_installed
	sqlite3 /var/cache/dnf/packages.db "select pkg from installed WHERE pkg LIKE \"$cur%\"" 2>/dev/null
end

# Return available but not installed packages
function __dnf_return_notinstalled
	sqlite3 /var/cache/dnf/packages.db "select available.pkg from available left join installed on available.pkg = installed.pkg where installed.pkg IS NULL and available.pkg LIKE \"$cur%\"" 2>/dev/null
end

# Return all packages
function __dnf_return_allavailable
	sqlite3 /var/cache/dnf/packages.db "select pkg from available WHERE pkg LIKE \"$cur%\"" 2>/dev/null
end


# Return Groups
#TODO: Completion of groups to slow, test other options
# Available
#for i in (dnf group list --available | grep -v ':' | sed -e "s/^ *//g" | )
#	set -g dnfgroup_available "$dnfgroup_available\n$i"
#end
#
## Installed
#for i in (dnf group list --installed | grep -v ':' | sed -e "s/^ *//g")
#	set -g dnfgroup_installed "$dnfgroup_installed\n$i"
#end
#
## All
#for i in (dnf group list | grep -v ':' | sed -e "s/^ *//g")
#	set -g dnfgroup_all "$dnfgroup_all\n$i"
#end
#
#function __dnf_return_groups
#	switch $argv
#		case "available"
#			echo -e $dnfgroup_available
#		case "installed"
#			echo -e $dnfgroup_installed
#		case "all"
#			echo -e $dnfgroup_all
#	end
#end


# Return help
set -e dnfhelp
for i in (dnf help)
	set dnfhelp "$dnfhelp\n$i"
end

function __dnf_return_help
	set -l helptext ( echo -e $dnfhelp | grep -m 1 $argv | tr -s " " | cut -d " " -f 2-)
	if test (echo $helptext | wc -c) -le 0
		echo " "
	else
		echo $helptext
	end
end

# Define DNF commands
set cmds autoremove check check-update clean deplist distro-sync downgrade group help history info install list makecache mark 'provides whatprovides' reinstall 'remove erase' repoinfo repolist repoquery repository-packages search shell swap updateinfo 'upgrade upgrade-to update' upgrade-minimal

# Completion DNF commands
for cmd in $cmds
	if test (echo $cmd | wc -w) -gt 1
		set hlp (echo $cmd | cut -d " " -f 1)
		complete -c dnf -n '__fish_use_subcommand' -xa $cmd --description (__dnf_return_help $hlp)
	else
		complete -c dnf -n '__fish_use_subcommand' -xa $cmd --description (__dnf_return_help $cmd)
	end
end

# Package listing
complete -c dnf -n '__fish_dnf_package_installed' -xa "(__dnf_return_installed)"
complete -c dnf -n '__fish_dnf_package_notinstalled' -xa "(__dnf_return_notinstalled)"
complete -c dnf -n '__fish_dnf_package_allavailable' -xa "(__dnf_return_allavailable)"

#TODO: Group listing tests
# Group listing
#complete -c dnf -n '__fish_seen_subcommand_from group; and __fish_seen_subcommand_from info' -a "(__dnf_return_groups all)"
#complete -c dnf -n '__fish_seen_subcommand_from group; and __fish_seen_subcommand_from install' -a "(__dnf_return_groups available)"
#complete -c dnf -n '__fish_seen_subcommand_from group; and __fish_seen_subcommand_from remove upgrade' -a "(__dnf_return_groups installed)"


# Subcommands
# Clean
complete -c dnf -n '__fish_seen_subcommand_from clean' -xa dbcache --description 'Delete cache files generated from the repository metadata' 
complete -c dnf -n '__fish_seen_subcommand_from clean' -xa expire-cache --description 'Marks the repository metadata expired.'
complete -c dnf -n '__fish_seen_subcommand_from clean' -xa metadata --description 'Delete cached metadata files'
complete -c dnf -n '__fish_seen_subcommand_from clean' -xa packages --description 'Delete cached package files'
complete -c dnf -n '__fish_seen_subcommand_from clean' -xa all --description 'Delete all cache contents'

# Group
function __check_subcommand_group
	not __fish_seen_subcommand_from info install list remove upgrade mark
end

complete -c dnf -n '__check_subcommand_group; and __fish_seen_subcommand_from group' -xa info
complete -c dnf -n '__check_subcommand_group; and __fish_seen_subcommand_from group' -xa install
complete -c dnf -n '__check_subcommand_group; and __fish_seen_subcommand_from group' -xa list
complete -c dnf -n '__check_subcommand_group; and __fish_seen_subcommand_from group' -xa remove
complete -c dnf -n '__check_subcommand_group; and __fish_seen_subcommand_from group' -xa upgrade
complete -c dnf -n '__check_subcommand_group; and __fish_seen_subcommand_from group' -xa mark
complete -c dnf -n '__fish_seen_subcommand_from mark group; and not __fish_seen_subcommand_from info install list remove upgrade' -xa install
complete -c dnf -n '__fish_seen_subcommand_from mark group; and not __fish_seen_subcommand_from info install list remove upgrade' -xa remove


# History
function __check_subcommand_history
	not __fish_seen_subcommand_from info redo rollback undo userinstalled
end

complete -c dnf -n '__check_subcommand_history; and __fish_seen_subcommand_from history' -xa info
complete -c dnf -n '__check_subcommand_history; and __fish_seen_subcommand_from history' -xa redo
complete -c dnf -n '__check_subcommand_history; and __fish_seen_subcommand_from history' -xa rollback
complete -c dnf -n '__check_subcommand_history; and __fish_seen_subcommand_from history' -xa undo
complete -c dnf -n '__check_subcommand_history; and __fish_seen_subcommand_from history' -xa userinstalled


# List
function __check_subcommand_list
	not __fish_seen_subcommand_from group all installed available extras obsoletes recent updates autoremove
end

complete -c dnf -n '__check_subcommand_list; and __fish_seen_subcommand_from list' -xa all --description 'List all packages'
complete -c dnf -n '__check_subcommand_list; and __fish_seen_subcommand_from list' -xa installed --description 'List installed packages'
complete -c dnf -n '__check_subcommand_list; and __fish_seen_subcommand_from list' -xa available --description 'List installable packages'
complete -c dnf -n '__check_subcommand_list; and __fish_seen_subcommand_from list' -xa extras --description 'List packages not available in repositories'
complete -c dnf -n '__check_subcommand_list; and __fish_seen_subcommand_from list' -xa obsoletes --description 'List packages that are obsoleted by packages in repositories'
complete -c dnf -n '__check_subcommand_list; and __fish_seen_subcommand_from list' -xa recent --description 'List packages recently added into the repositories.'
complete -c dnf -n '__check_subcommand_list; and __fish_seen_subcommand_from list' -xa 'upgrades updates' --description 'List updateable packages'
complete -c dnf -n '__check_subcommand_list; and __fish_seen_subcommand_from list' -xa autoremove --description 'List autoremovable packages'

# Mark
function __check_subcommand_mark
	not __fish_seen_subcommand_from install remove group
end

complete -c dnf -n '__check_subcommand_mark; and __fish_seen_subcommand_from mark' -xa install
complete -c dnf -n '__check_subcommand_mark; and __fish_seen_subcommand_from mark' -xa remove
complete -c dnf -n '__check_subcommand_mark; and __fish_seen_subcommand_from mark' -xa group


# Short and long options
# TODO: Finish the completion of Options
complete -c dnf -s h -l help --description "Display help and exit"
complete -c dnf -s y --description "Assume yes to all questions"
complete -c dnf -s c --description "Configuration file" -r
complete -c dnf -s d --description "Set debug level" -x
complete -c dnf -s e --description "Set error level" -x
complete -c dnf -s t -l tolerant --description "Be tolerant of errors in commandline"
complete -c dnf -s R --description "Set maximum delay between commands" -x
complete -c dnf -s c --description "Run commands from cache"
complete -c dnf -l version --description "Display version and exit"
complete -c dnf -l installroot --description "Specify installroot" -r
complete -c dnf -l enablerepo --description "Enable repository" -r
complete -c dnf -l disablerepo --description "Disable repository" -r
complete -c dnf -l obsoletes --description "Enables obsolets processing logic"
complete -c dnf -l rss-filename --description "Output rss-data to file" -r
complete -c dnf -l exclude --description "Exclude specified package from updates" -a "(__fish_print_packages)"
