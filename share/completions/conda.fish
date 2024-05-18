# Fish shell completions for conda, the package management system
# https://conda.io/docs
# Based on `conda help` as per version 4.4.11
# Advanced completions:
# - completion of already defined keys for `conda config`
# - completion of already created environment where possible
#
# Note: conda ships with a completion for fish, automatically generated
# from `conda -h` but it is far cruder than the completions in this file,
# and it is by nature far too tricky to tweak to get the desired richness.
# Hence this effort.

# First, as part of conda's configuration, some complete would have been defined
# Let's erase them, so that we start from a blank state
complete -c conda -e

function __fish_conda_subcommand
    # This function does triple-duty:
    # If called without arguments, it will print the first subcommand.
    # If one exists, it returns false.
    # If it doesn't, it returns true.
    #
    # If called with arguments, it will check that those match the given subcommands,
    # and that there are no additional subcommands.
    set -l subcmds $argv
    set -q subcmds[1]
    set -l have_sub $status

    # get the commandline args without the "conda"
    set -l toks (commandline -xpc)[2..-1]

    # Remove any important options - if we had options with arguments,
    # they'd need to be listed here to be removed.
    argparse -i h/help v/version -- $toks 2>/dev/null
    # Return false if it fails - this shouldn't really happen,
    # so all bets are off
    or return 2

    # Remove all matching subcommands
    while set -q subcmds[1]
        # If the subcommand matches, or we have an option, go on.
        # (if the option took an argument we wouldn't know,
        #  so it needs to be argparse'd out above!)
        if test "$subcmds[1]" = "$argv[1]"
            set -e argv[1]
            set -e subcmds[1]
        else if string match -q -- '-*' $argv[1]
            set -e argv[1]
        else
            return 1
        end
    end

    # Skip any remaining options.
    while string match -q -- '-*' $argv[1]
        set -e argv[1]
    end

    # If we have no subcommand left,
    # we either matched all given subcommands or we need one.
    if not set -q argv[1]
        return $have_sub
    end

    echo -- $argv[1]

    # If we didn't have subcommands to check, return true
    # If we did, this is an additional command and so we return false.
    not test $have_sub -eq 0
end

function __fish_conda -a cmd
    complete -c conda -n "contains -- (__fish_conda_subcommand) $cmd" $argv[2..-1]
end

# Complete for the first argument only
function __fish_conda_top
    complete -c conda -n 'not __fish_conda_subcommand' $argv
end

function __fish_conda_config_keys
    conda config --show | string match -r '^\w+(?=:)'
end

function __fish_conda_environments
    conda env list | string match -rv '^#' | string match -r '^\S+'
end

# common options
complete -c conda -f
complete -c conda -s h -l help -d "Show help and exit"

# top-level options
__fish_conda_top -s V -l version -d "Show the conda version number and exit"

# top-level commands
__fish_conda_top -a clean -d "Remove unused packages and caches"
__fish_conda_top -a config -d "Modify configuration values in .condarc"
__fish_conda_top -a create -d "Create a new conda environment from a list of specified packages"
__fish_conda_top -a help -d "Displays a list of available conda commands and their help strings"
__fish_conda_top -a info -d "Display information about current conda install"
__fish_conda_top -a install -d "Installs a list of packages into a specified conda environment"
__fish_conda_top -a list -d "List linked packages in a conda environment"
__fish_conda_top -a package -d "Low-level conda package utility (EXPERIMENTAL)"
__fish_conda_top -a remove -d "Remove a list of packages from a specified conda environment"
__fish_conda_top -a uninstall -d "Alias for conda remove"
__fish_conda_top -a search -d "Search for packages and display associated information"
__fish_conda_top -a update -d "Updates conda packages to the latest compatible version"
__fish_conda_top -a upgrade -d "Alias for conda update"

# command added by sourcing ~/miniconda3/etc/fish/conf.d/conda.fish,
# which is the recommended way to use conda with fish
__fish_conda_top -a activate -d "Activate the given environment"
__fish_conda activate -x -a "(__fish_conda_environments)"
__fish_conda_top -a deactivate -d "Deactivate current environment, reactivating the previous one"

# common to all top-level commands

set -l __fish_conda_commands clean config create help info install list package remove uninstall search update upgrade
for cmd in $__fish_conda_commands
    __fish_conda $cmd -l json -d "Report all output as json"
    __fish_conda $cmd -l debug -d "Show debug output"
    __fish_conda $cmd -l verbose -s v -d "Use once for info, twice for debug, three times for trace"
end

# 'clean' command
__fish_conda clean -s y -l yes -d "Do not ask for confirmation"
__fish_conda clean -l dry-run -d "Only display what would have been done"
__fish_conda clean -s q -l quiet -d "Do not display progress bar"
__fish_conda clean -s a -l all -d "Remove all: same as -iltps"
__fish_conda clean -s i -l index-cache -d "Remove index cache"
__fish_conda clean -s l -l lock -d "Remove all conda lock files"
__fish_conda clean -s t -l tarballs -d "Remove cached package tarballs"
__fish_conda clean -s p -l packages -d "Remove unused cached packages (no check for symlinks)"
__fish_conda clean -s s -l source-cache -d "Remove files from the source cache of conda build"

# 'config' command

__fish_conda config -l system -d "Write to the system .condarc file"
__fish_conda config -l env -d "Write to the active conda environment .condarc file"
__fish_conda config -l file -d "Write to the given file" -F
__fish_conda config -l show -x -a "(__fish_conda_config_keys)" -d "Display configuration values"
__fish_conda config -l show-sources -d "Display all identified configuration sources"
__fish_conda config -l validate -d "Validate all configuration sources"
__fish_conda config -l describe -x -a "(__fish_conda_config_keys)" -d "Describe configuration parameters"
__fish_conda config -l write-default -d "Write the default configuration to a file"
__fish_conda config -l get -x -a "(__fish_conda_config_keys)" -d "Get a configuration value"
__fish_conda config -l append -d "Add one configuration value to the end of a list key"
__fish_conda config -l prepend -d "Add one configuration value to the beginning of a list key"
__fish_conda config -l add -d "Alias for --prepend"
__fish_conda config -l set -x -a "(__fish_conda_config_keys)" -d "Set a boolean or string key"
__fish_conda config -l remove -x -a "(__fish_conda_config_keys)" -d "Remove a configuration value from a list key"
__fish_conda config -l remove-key -x -a "(__fish_conda_config_keys)" -d "Remove a configuration key (and all its values)"
__fish_conda config -l stdin -d "Apply configuration given in yaml format from stdin"

# 'help' command
__fish_conda help -d "Displays a list of available conda commands and their help strings"
__fish_conda help -x -a "$__fish_conda_commands"

# 'info' command
__fish_conda info -l offline -d "Offline mode, don't connect to the Internet."
__fish_conda info -s a -l all -d "Show all information, (environments, license, and system information)"
__fish_conda info -s e -l envs -d "List all known conda environments"
__fish_conda info -s l -l license -d "Display information about the local conda licenses list"
__fish_conda info -s s -l system -d "List environment variables"
__fish_conda info -l base -d "Display base environment path"
__fish_conda info -l unsafe-channels -d "Display list of channels with tokens exposed"

# The remaining commands share many options, so the definitions are written the other way around:
# the outer loop is on the options

# Option channel
for cmd in create install remove search update
    __fish_conda $cmd -s c -l channel -d 'Additional channel to search for packages'
end

# Option channel-priority
for cmd in create install update
    __fish_conda $cmd -l channel-priority -d 'Channel priority takes precedence over package version'
end

# Option clobber
for cmd in create install update
    __fish_conda $cmd -l clobber -d 'Allow clobbering of overlapping file paths (no warnings)'
end

# Option clone
__fish_conda create -l clone -x -a "(__fish_conda_environments)" -d "Path to (or name of) existing local environment"

# Option copy
for cmd in create install update
    __fish_conda $cmd -l copy -d 'Install all packages using copies instead of hard- or soft-linking'
end

# Option download-only
for cmd in create install update
    __fish_conda $cmd -l download-only -d 'Solve an environment: populate caches but no linking/unlinking into prefix'
end

# Option dry-run
for cmd in create install remove update
    __fish_conda $cmd -l dry-run -d 'Only display what would have been done'
end

# Option file
for cmd in create install update
    __fish_conda $cmd -l file -d 'Read package versions from the given file' -F
end

# Option force
for cmd in create install remove update
    __fish_conda $cmd -l force -d 'Force install (even when package already installed)'
end

# Option insecure
for cmd in create install remove search update
    __fish_conda $cmd -l insecure -d 'Allow conda to perform "insecure" SSL connections and transfers'
end

# Option mkdir
for cmd in create install update
    __fish_conda $cmd -l mkdir -d 'Create the environment directory if necessary'
end

# Option name
__fish_conda create -s n -l name -d "Name of new environment"
for cmd in install list remove search update
    __fish_conda $cmd -s n -l name -x -a "(__fish_conda_environments)" -d "Name of existing environment"
end

# Option no-channel-priority
for cmd in create install update
    __fish_conda $cmd -l no-channel-priority -l no-channel-pri -l no-chan-pri -d 'Package version takes precedence over channel priority'
end

# Option no-default-packages
__fish_conda create -l no-default-packages -d 'Ignore create_default_packages in the .condarc file'

# Option no-deps
for cmd in create install update
    __fish_conda $cmd -l no-deps -d 'Do not install, update, remove, or change dependencies'
end

# Option no-pin
for cmd in create install remove update
    __fish_conda $cmd -l no-pin -d 'Ignore pinned file'
end

# Option no-show-channel-urls
for cmd in create install list update
    __fish_conda $cmd -l no-show-channel-urls -d "Don't show channel urls"
end

# Option no-update-dependencies
for cmd in create install update
    __fish_conda $cmd -l no-update-dependencies -l no-update-deps -d "Don't update dependencies"
end

# Option offline
for cmd in create install remove search update
    __fish_conda $cmd -l offline -d "Offline mode, don't connect to the Internet"
end

# Option only-deps
for cmd in create install update
    __fish_conda $cmd -l only-deps -d "Only install dependencies"
end

# Option override-channels
for cmd in create install remove search update
    __fish_conda $cmd -l override-channels -d "Do not search default or .condarc channels"
end

# Option prefix
for cmd in create install list remove search update
    __fish_conda $cmd -s p -l prefix -d "Full path to environment prefix"
end

# Option quiet
for cmd in create install remove update
    __fish_conda $cmd -s q -l quiet -d "Do not display progress bar"
end

# Option show-channel-urls
for cmd in create install list update
    __fish_conda $cmd -l show-channel-urls -d "Show channel urls"
end

# Option update-dependencies
for cmd in create install update
    __fish_conda $cmd -l update-dependencies -l update-deps -d "Update dependencies"
end

# Option use-index-cache
for cmd in create install remove search update
    __fish_conda $cmd -s C -l use-index-cache -d "Use cache of channel index files, even if it has expired"
end

# Option use-local
for cmd in create install remove search update
    __fish_conda $cmd -l use-local -d "Use locally built packages"
end

# Option yes
for cmd in create install remove update
    __fish_conda $cmd -s y -l yes -d "Do not ask for confirmation"
end

# Option revision
__fish_conda install -l revision -d "Revert to the specified REVISION"

# Option canonical
__fish_conda list -s c -l canonical -d "Output canonical names of packages only. Implies --no-pip"

# Option explicit
__fish_conda list -l explicit -d "List explicitly all installed conda with URL (output usable by conda create --file)"

# Option export
__fish_conda list -s e -l export -d "Output requirement string only (output usable by conda create --file)"

# Option full-name
__fish_conda list -s f -l full-name -d "Only search for full names, i.e., ^<regex>\$"

# Option md5
__fish_conda list -l md5 -d "Add MD5 hashsum when using --explicit"

# Option no-pip
__fish_conda list -s c -l canonical -d "Output canonical names of packages only. Implies --no-pip"

# Option revisions
__fish_conda list -s r -l revisions -d "List the revision history and exit"

# Option all
__fish_conda remove -l all -d "Remove all packages, i.e., the entire environment"
__fish_conda update -l all -d "Update all installed packages in the environment"

# Option features
__fish_conda remove -l features -d "Remove features (instead of packages)"

# Option info
__fish_conda search -s i -l info -d "Provide detailed information about each package"

# Option platform
set -l __fish_conda_platforms {osx,linux,win}-{32,64}
__fish_conda search -l platform -x -a "$__fish_conda_platforms" -d "Search the given platform"

# Option reverse-dependency
__fish_conda search -l reverse-dependency -d "Perform a reverse dependency search"

__fish_conda_top -a env -d "Conda options for environments"
complete -c conda -n "__fish_conda_subcommand env" -a create -d "Create a new environment"
complete -c conda -n "__fish_conda_subcommand env" -a list -d "List all conda environments"
complete -c conda -n "__fish_conda_subcommand env create" -s f -l file -rF -d "Create environment from yaml file"
