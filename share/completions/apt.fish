# Completions for the `apt` command

# macOS has a /usr/bin/apt that is something else entirely:
# (apt - Returns the path to a Java home directory from the current user's settings)
if [ "$(uname -s)" = Darwin -a "$(command -s apt)" = /usr/bin/apt ]
    exit 1
end

set -l all_subcmds update upgrade full-upgrade search list install show remove edit-sources purge changelog autoremove autopurge depends rdepends why-not satisfy history-list history-rollback history-info history-redo history-undo help

# subcommands that take a package name as an argument
set -l pkg_subcmds install upgrade full-upgrade show search changelog policy depends rdepends why-not # why-not subcmd makes more sense with only non installed packages, but figuring out how to list those is left as an exercise to the reader
# subcommands that take a package name as an argument but cannot or don't make sense to operate on non insatlled packages, should be mutually exclusive with the above list
set -l installed_pkg_subcmds reinstall purge remove autoremove autopurge why

# subcommands to suggest loose .deb files (that are present in the CWD) in the completions
set -l handle_file_pkg_subcmds install

# sub commands that all seem to be inherited from apt-get and share common options, although some combinations of them seems questionable, e.g remove --download-only
set -l option_group1 install remove purge upgrade full-upgrade dist-upgrade source build-dep reinstall satisfy clean distclean autoremove autopurge

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
    # cat (find /etc/apt/sources.list /etc/apt/sources.list.d/ -name "*.list") | string replace -rf '^\s*deb *(?:\[.*?\])? (?:[^ ]+) +([^ ]+) .*' '$1' | sort -u
    set -l repos
    if test -f /etc/apt/sources.list
        set -a repos (cat /etc/apt/sources.list | string replace -rf '^\s*deb *(?:\[.*?\])? (?:[^ ]+) +([^ ]+) .*' '$1')
    end

    set -l lists (find /etc/apt/sources.list.d/ -name "*.list")
    if test -n $lists
        set -a repos (cat $lists | string replace -rf '^\s*deb *(?:\[.*?\])? (?:[^ ]+) +([^ ]+) .*' '$1')
    end

    # New format of apt sources, the old one and the new one may coexist in a given system
    # TODO for the given input `Suites: sid experimental` this line doesn't work properly
    set -a repos (cat (find /etc/apt/sources.list.d/ -name "*.sources") | string replace -rf '^Suites: +([^ ]+)' '$1')

    echo $repos | string replace ' ' '
' | sort -u
end

complete -c apt -f

# We use -k to keep PWD directories (from the .deb completion) after packages, so we need to sort the packages
complete -k -c apt -n "__fish_seen_subcommand_from $handle_file_pkg_subcmds" -kxa '(__fish_complete_suffix .deb)'
complete -k -c apt -n "__fish_seen_subcommand_from $pkg_subcmds" -kxa '(__fish_print_apt_packages | sort)'
complete -c apt -n "__fish_seen_subcommand_from $installed_pkg_subcmds" -kxa '(__fish_print_apt_packages --installed | sort)'

complete -c apt -n "__fish_seen_subcommand_from $option_group1" -l no-install-recommends -d 'Do not install recommended packages'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -l no-install-suggests -d 'Do not install suggested packages'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -s d -l download-only -d 'Download Only'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -s f -l fix-broken -d 'Correct broken dependencies'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -s m -l fix-missing -d 'Ignore missing packages'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -l no-download -d 'Disable downloading packages'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -s q -l quiet -d 'Quiet mode'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -s s -l simulate -l just-print -l dry-run -l recon -l no-act -d 'Perform a simulation'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -s y -l yes -l assume-yes -d 'Automatic yes to prompts'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -l assume-no -d 'Automatic no to prompts'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -l install-recommends -d 'Install recommended packages'
complete -c apt -n "__fish_seen_subcommand_from $option_group1" -l install-suggests -d 'Install suggested packages'
# This advanced flag is the safest way to upgrade packages that otherwise would have been kept back
complete -c apt -n "__fish_seen_subcommand_from upgrade" -l with-new-pkgs

complete -c apt -n "__fish_seen_subcommand_from install reinstall remove purge autoremove autopurge upgrade dist-upgrade full-upgrade" -l comment -d 'Add comment to this transaction'

complete -c apt -n "__fish_seen_subcommand_from source" -s b -l build -l compile -d 'Compile source packages after download'

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

# Autopurge
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

# Why
__fish_apt_subcommand why -x -d 'See why a given package is installed'

# Why not
__fish_apt_subcommand why-not -r -d 'See why a given package is not installable'

# History list
__fish_apt_subcommand history-list -d 'List transaction list'

# History info
__fish_apt_subcommand history-info -d 'Show info of a transaction'

# History redo
__fish_apt_subcommand history-redo -d 'Redo transaction'

# History undo
__fish_apt_subcommand history-undo -d 'Undo transaction'

# History rollback
__fish_apt_subcommand history-rollback -d 'Rollback transaction'

# Help
__fish_apt_subcommand help -d 'Print help page'

functions -e __fish_apt_subcommand __fish_apt_option
