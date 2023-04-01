# Completions for the `apt` command

# macOS has a /usr/bin/apt that is something else entirely:
# (apt - Returns the path to a Java home directory from the current user's settings)
if [ "$(uname -s)" = Darwin -a "$(command -s apt)" = /usr/bin/apt ]
    exit 1
end

set -l all_subcmds update upgrade full-upgrade search list install show remove edit-sources purge changelog autoremove depends rdepends
set -l pkg_subcmds install upgrade full-upgrade show search purge changelog policy depends rdepends autoremove
set -l installed_pkg_subcmds remove
set -l handle_file_pkg_subcmds install

function __ghoti_apt_subcommand -V all_subcmds
    set -l subcommand $argv[1]
    set -e argv[1]
    complete -f -c apt -n "__ghoti_is_first_token" -a $subcommand $argv
end

function __ghoti_apt_option
    set -l subcommand $argv[1]
    set -e argv[1]
    complete -f -c apt -n "__ghoti_seen_subcommand_from $subcommand" $argv
end

function __ghoti_apt_list_repos
    # A single `string` invocation can't read from multiple files and so we use `cat`
    # but /etc/apt/sources.list.d/ may or may not contain any files so using a ghoti
    # wildcard glob may complain loudly if no files match the pattern so we use `find`.
    # The trailing `sort -u` is largely decorative.
    cat (find /etc/apt/sources.list /etc/apt/sources.list.d/ -name "*.list") | string replace -rf '^\s*deb *(?:\[.*?\])? (?:[^ ]+) +([^ ]+) .*' '$1' | sort -u
end

complete -c apt -f

# We use -k to keep PWD directories (from the .deb completion) after packages, so we need to sort the packages
complete -k -c apt -n "__ghoti_seen_subcommand_from $handle_file_pkg_subcmds" -kxa '(__ghoti_complete_suffix .deb)'
complete -k -c apt -n "__ghoti_seen_subcommand_from $pkg_subcmds" -kxa '(__ghoti_print_apt_packages | sort)'
complete -c apt -n "__ghoti_seen_subcommand_from $installed_pkg_subcmds" -kxa '(__ghoti_print_apt_packages --installed | sort)'

complete -c apt -n "__ghoti_seen_subcommand_from install" -l no-install-recommends
# This advanced flag is the safest way to upgrade packages that otherwise would have been kept back
complete -c apt -n "__ghoti_seen_subcommand_from upgrade" -l with-new-pkgs

# Support flags
complete -f -c apt -s h -l help -d 'Display help'
complete -f -c apt -s v -l version -d 'Display version and exit'

# General options
complete -x -c apt -s o -l option -d 'Set a configuration option'
complete -r -c apt -s c -l config-file -d 'Configuration file'
complete -x -c apt -s t -d 'Install from specific repository' -x -a '(__ghoti_apt_list_repos)'

# List
__ghoti_apt_subcommand list -d 'List packages'
__ghoti_apt_option list -l installed -d 'Installed packages'
__ghoti_apt_option list -l upgradable -d 'Upgradable packages'
__ghoti_apt_option list -l all-versions -d 'Show all versions of any package'

# Search
__ghoti_apt_subcommand search -r -d 'Search for packages'

# Search
__ghoti_apt_subcommand show -r -d 'Show package information'

# Install
__ghoti_apt_subcommand install -r -d 'Install packages'
__ghoti_apt_option install -l reinstall -d 'Reinstall package'

# Remove
__ghoti_apt_subcommand remove -r -d 'Remove packages'

# Edit sources
__ghoti_apt_subcommand edit-sources -d 'Edit sources list'

# Update
__ghoti_apt_subcommand update -x -d 'Update package list'

# Upgrade
__ghoti_apt_subcommand upgrade -r -d 'Upgrade packages'

# Full Upgrade
__ghoti_apt_subcommand full-upgrade -r -d 'Upgrade packages, removing others when needed'

# Purge
__ghoti_apt_subcommand purge -x -d 'Remove packages and delete their config files'

# Changelog
__ghoti_apt_subcommand changelog -r -d 'Download and display package changelog'

# Autoremove
__ghoti_apt_subcommand autoremove -d 'Remove packages no longer needed as dependencies'

# Policy
__ghoti_apt_subcommand policy -x -d 'Display source or package priorities'

# Depends
__ghoti_apt_subcommand depends -r -d 'List package dependencies'

# Rdepends
__ghoti_apt_subcommand rdepends -r -d 'List package reverse dependencies'

functions -e __ghoti_apt_subcommand __ghoti_apt_option
