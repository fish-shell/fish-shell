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

function __fish_apt_subcommand -V all_subcmds
    set -l subcommand $argv[1]
    set -e argv[1]
    complete -f -c apt -n __fish_is_first_token -a $subcommand $argv
end

function __fish_apt_option
    set -l subcommand $argv[1]
    set -e argv[1]
    complete -f -c apt -n "__fish_seen_subcommand_from $subcommand" $argv
end

function __fish_apt_list_repos
    # A single `string` invocation can't read from multiple files and so we use `cat`
    # but /etc/apt/sources.list.d/ may or may not contain any files so using a fish
    # wildcard glob may complain loudly if no files match the pattern so we use `find`.
    # The trailing `sort -u` is largely decorative.
    cat (find /etc/apt/sources.list /etc/apt/sources.list.d/ -name "*.list") | string replace -rf '^\s*deb *(?:\[.*?\])? (?:[^ ]+) +([^ ]+) .*' '$1' | sort -u
end

complete -c apt -f

# We use -k to keep PWD directories (from the .deb completion) after packages, so we need to sort the packages
complete -k -c apt -n "__fish_seen_subcommand_from $handle_file_pkg_subcmds" -kxa '(__fish_complete_suffix .deb)'
complete -k -c apt -n "__fish_seen_subcommand_from $pkg_subcmds" -kxa '(__fish_print_apt_packages | sort)'
complete -c apt -n "__fish_seen_subcommand_from $installed_pkg_subcmds" -kxa '(__fish_print_apt_packages --installed | sort)'

complete -c apt -n "__fish_seen_subcommand_from install" -l no-install-recommends -d 'Do not install recommended packages'
complete -c apt -n "__fish_seen_subcommand_from install" -l no-install-suggests -d 'Do not install suggested packages'
complete -c apt -n "__fish_seen_subcommand_from install" -s d -l download-only -d 'Download Only'
complete -c apt -n "__fish_seen_subcommand_from install" -s f -l fix-broken -d 'Correct broken dependencies'
complete -c apt -n "__fish_seen_subcommand_from install" -s m -l fix-missing -d 'Ignore missing packages'
complete -c apt -n "__fish_seen_subcommand_from install" -l no-download -d 'Disable downloading packages'
complete -c apt -n "__fish_seen_subcommand_from install" -s q -l quiet -d 'Quiet mode'
complete -c apt -n "__fish_seen_subcommand_from install" -s s -l simulate -l just-print -l dry-run -l recon -l no-act -d 'Perform a simulation'
complete -c apt -n "__fish_seen_subcommand_from install" -s y -l yes -l assume-yes -d 'Automatic yes to prompts'
complete -c apt -n "__fish_seen_subcommand_from install" -l assume-no -d 'Automatic no to prompts'
complete -c apt -n "__fish_seen_subcommand_from install" -l install-recommends -d 'Install recommended packages'
complete -c apt -n "__fish_seen_subcommand_from install" -l install-suggests -d 'Install suggested packages'
# This advanced flag is the safest way to upgrade packages that otherwise would have been kept back
complete -c apt -n "__fish_seen_subcommand_from upgrade" -l with-new-pkgs

# Support flags
complete -f -c apt -s h -l help -d 'Display help'
complete -f -c apt -s v -l version -d 'Display version and exit'

# General options
complete -x -c apt -s o -l option -d 'Set a configuration option'
complete -r -c apt -s c -l config-file -d 'Configuration file'
complete -x -c apt -s t -d 'Install from specific repository' -x -a '(__fish_apt_list_repos)'

# List
__fish_apt_subcommand list -d 'List packages'
__fish_apt_option list -l installed -d 'Installed packages'
__fish_apt_option list -l manual-installed -d 'Manually installed packages'
__fish_apt_option list -l upgradable -d 'Upgradable packages'
__fish_apt_option list -s a -l all-versions -d 'Show all versions of any package'
__fish_apt_option list -s v -l verbose -d 'Verbose output'

# Search
__fish_apt_subcommand search -r -d 'Search for packages'

# Show
__fish_apt_subcommand show -r -d 'Show package information'

# Showsrc
__fish_apt_subcommand showsrc -r -d 'Show source package information'

# Source
__fish_apt_subcommand source -r -d 'Download source package'

# Install
__fish_apt_subcommand install -r -d 'Install packages'
__fish_apt_option install -l reinstall -d 'Reinstall package'

# Reinstall
__fish_apt_subcommand reinstall -r -d 'Reinstall packages'

# Remove
__fish_apt_subcommand remove -x -d 'Remove packages'

# Edit sources
__fish_apt_subcommand edit-sources -d 'Edit sources list'

# Update
__fish_apt_subcommand update -x -d 'Update package list'

# Upgrade
__fish_apt_subcommand upgrade -r -d 'Upgrade packages'

# Full Upgrade
__fish_apt_subcommand full-upgrade -r -d 'Upgrade packages, removing others when needed'
__fish_apt_subcommand dist-upgrade -r -d 'Upgrade packages, removing others when needed'

# Purge
__fish_apt_subcommand purge -x -d 'Remove packages and delete their config files'

# Changelog
__fish_apt_subcommand changelog -r -d 'Download and display package changelog'

# Autoremove
__fish_apt_subcommand autoremove -d 'Remove packages no longer needed as dependencies'

# Autoremove
__fish_apt_subcommand autopurge -d 'Remove packages no longer needed as dependencies and delete their config files'

# Clean
__fish_apt_subcommand clean -d 'Remove downloaded packages from cache'

# Autoclean
__fish_apt_subcommand autoclean -d 'Remove obsolete packages from cache'

# Policy
__fish_apt_subcommand policy -x -d 'Display source or package priorities'

# Depends
__fish_apt_subcommand depends -r -d 'List package dependencies'

# Rdepends
__fish_apt_subcommand rdepends -r -d 'List package reverse dependencies'

# Download
__fish_apt_subcommand download -x -d 'Download packages'

# Build-dep
__fish_apt_subcommand build-dep -x -d 'Install packages needed to build the given package'

# Help
__fish_apt_subcommand help -d 'Print help page'

functions -e __fish_apt_subcommand __fish_apt_option
