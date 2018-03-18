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

# Filter for `complete -n` selecting a command line
# with the given conda subcommand
function __fish_conda_needs_command -a cmd
    # We need to filter out -x and --xx options so as to complete
    # the arguments of said options
    test (string match -r '^\w+' -- (commandline -opc))[-1] = $cmd
end

# Complete using -n to select the given conda subcommand
# and passing the rest of the arguments to `complete`
# The goal here is to reduce clutter in the definitions below
# This function will be deleted at the end
function __ -a cmd
    complete -c conda -n "__fish_conda_needs_command $cmd" $argv[2..-1]
end

function __fish_conda_config_keys
    conda config --show | string match -r '^\w+(?=:)'
end

function __fish_conda_environments
    conda env list | string match -rv '^#' | string match -r '^\w+'
end

# common options
complete -c conda -f
complete -c conda -s h -l help -d "Show help and exit"

# top-level options
__ conda -s V -l version -d "Show the conda version number and exit"

# top-level commands
__ conda -a clean     -d "Remove unused packages and caches"
__ conda -a config    -d "Modify configuration values in .condarc"
__ conda -a create    -d "Create a new conda environment from a list of specified packages"
__ conda -a help      -d "Displays a list of available conda commands and their help strings"
__ conda -a info      -d "Display information about current conda install"
__ conda -a install   -d "Installs a list of packages into a specified conda environment"
__ conda -a list      -d "List linked packages in a conda environment"
__ conda -a package   -d "Low-level conda package utility (EXPERIMENTAL)"
__ conda -a remove    -d "Remove a list of packages from a specified conda environment"
__ conda -a uninstall -d "Alias for conda remove"
__ conda -a search    -d "Search for packages and display associated information"
__ conda -a update    -d "Updates conda packages to the latest compatible version"
__ conda -a upgrade   -d "Alias for conda update"

# common to all top-level commands

set -l __fish_conda_commands clean config create help info install list package remove uninstall search update upgrade
for cmd in $__fish_conda_commands
    __ $cmd -l json         -d "Report all output as json"
    __ $cmd -l debug        -d "Show debug output"
    __ $cmd -l verbose -s v -d "Use once for info, twice for debug, three times for trace"
end

# 'clean' command
__ clean -s y -l yes          -d "Do not ask for confirmation"
__ clean -l dry-run           -d "Only display what would have been done"
__ clean -s q -l quiet        -d "Do not display progress bar"
__ clean -s a -l all          -d "Remove all: same as -iltps"
__ clean -s i -l index-cache  -d "Remove index cache"
__ clean -s l -l lock         -d "Remove all conda lock files"
__ clean -s t -l tarballs     -d "Remove cached package tarballs"
__ clean -s p -l packages     -d "Remove unused cached packages (no check for symlinks)"
__ clean -s s -l source-cache -d "Remove files from the source cache of conda build"

# 'config' command

__ config -l system                                        -d "Write to the system .condarc file"
__ config -l env                                           -d "Write to the active conda environment .condarc file"
__ config -l file                                          -d "Write to the given file"
__ config -l show -x -a "(__fish_conda_config_keys)"       -d "Display configuration values"
__ config -l show-sources                                  -d "Display all identified configuration sources"
__ config -l validate                                      -d "Validate all configuration sources"
__ config -l describe -x -a "(__fish_conda_config_keys)"   -d "Describe configuration parameters"
__ config -l write-default                                 -d "Write the default configuration to a file"
__ config -l get -x -a "(__fish_conda_config_keys)"        -d "Get a configuration value"
__ config -l append                                        -d "Add one configuration value to the end of a list key"
__ config -l prepend                                       -d "Add one configuration value to the beginning of a list key"
__ config -l add                                           -d "Alias for --prepend"
__ config -l set -x -a "(__fish_conda_config_keys)"        -d "Set a boolean or string key"
__ config -l remove -x -a "(__fish_conda_config_keys)"     -d "Remove a configuration value from a list key"
__ config -l remove-key -x -a "(__fish_conda_config_keys)" -d "Remove a configuration key (and all its values)"
__ config -l stdin                                         -d "Apply configuration given in yaml format from stdin"

# 'help' command
__ "help"         -d "Displays a list of available conda commands and their help strings"
__ "help" -x -a "$__fish_conda_commands"

# 'info' command
__ info -l offline         -d "Offline mode, don't connect to the Internet."
__ info -s a -l all        -d "Show all information, (environments, license, and system information"
__ info -s e -l envs       -d "List all known conda environments"
__ info -s l -l license    -d "Display information about the local conda licenses list"
__ info -s s -l system     -d "List environment variables"
__ info -l base            -d "Display base environment path"
__ info -l unsafe-channels -d "Display list of channels with tokens exposed"

# The remaining commands share many options, so the definitions are written the other way around:
# the outer loop is on the options

# Option channel
for cmd in create install remove search update
    __ $cmd -s c -l channel -d 'Additional channel to search for packages'
end

# Option channel-priority
for cmd in create install update
    __ $cmd -l channel-priority -d 'Channel priority takes precedence over package version'
end

# Option clobber
for cmd in create install update
    __ $cmd -l clobber -d 'Allow clobbering of overlapping file paths (no warnings)'
end

# Option clone
__ create -l clone -x -a "(__fish_conda_environments)" -d "Path to (or name of) existing local environment"

# Option copy
for cmd in create install update
    __ $cmd -l copy -d 'Install all packages using copies instead of hard- or soft-linking'
end

# Option download-only
for cmd in create install update
    __ $cmd -l download-only -d 'Solve an environment: populate caches but no linking/unlinking into prefix'
end

# Option dry-run
for cmd in create install remove update
    __ $cmd -l dry-run -d 'Only display what would have been done'
end

# Option file
for cmd in create install update
    __ $cmd -l file -d 'Read package versions from the given file'
end

# Option force
for cmd in create install remove update
    __ $cmd -l force -d 'Force install (even when package already installed)'
end

# Option insecure
for cmd in create install remove search update
    __ $cmd -l insecure -d 'Allow conda to perform "insecure" SSL connections and transfers'
end

# Option mkdir
for cmd in create install update
    __ $cmd -l mkdir -d 'Create the environment directory if necessary'
end

# Option name
for cmd in create install list remove search update
    if test $cmd = create
        __ $cmd -s n -l name -d "Name of new environment"
    else
        __ $cmd -s n -l name -x -a "(__fish_conda_environments)" -d "Name of existing environment"
    end
end

# Option no-channel-priority
for cmd in create install update
    __ $cmd -l no-channel-priority -l no-channel-pri -l no-chan-pri -d 'Package version takes precedence over channel priority'
end

# Option no-default-packages
__ create -l no-default-packages -d 'Ignore create_default_packages in the .condarc file'

# Option no-deps
for cmd in create install update
    __ $cmd -l no-deps -d 'Do not install, update, remove, or change dependencies'
end

# Option no-pin
for cmd in create install remove update
    __ $cmd -l no-pin -d 'Ignore pinned file'
end

# Option no-show-channel-urls
for cmd in create install list update
    __ $cmd -l no-show-channel-urls -d "Don't show channel urls"
end

# Option no-update-dependencies
for cmd in create install update
    __ $cmd -l no-update-dependencies -l no-update-deps -d "Don't update dependencies"
end

# Option offline
for cmd in create install remove search update
    __ $cmd -l offline -d "Offline mode, don't connect to the Internet"
end

# Option only-deps
for cmd in create install update
    __ $cmd -l only-deps -d "Only install dependencies"
end

# Option override-channels
for cmd in create install remove search update
    __ $cmd -l override-channels -d "Do not search default or .condarc channels"
end

# Option prefix
for cmd in create install list remove search update
    __ $cmd -s p -l prefix -d "Full path to environment prefix"
end

# Option quiet
for cmd in create install remove update
    __ $cmd -s q -l quiet -d "Do not display progress bar"
end

# Option show-channel-urls
for cmd in create install list update
    __ $cmd -l show-channel-urls -d "Show channel urls"
end

# Option update-dependencies
for cmd in create install update
    __ $cmd -l update-dependencies -l update-deps -d "Update dependencies"
end

# Option use-index-cache
for cmd in create install remove search update
    __ $cmd -s C -l use-index-cache -d "Use cache of channel index files, even if it has expired"
end

# Option use-local
for cmd in create install remove search update
    __ $cmd -l use-local -d "Use locally built packages"
end

# Option yes
for cmd in create install remove update
    __ $cmd -s y -l yes -d "Do not ask for confirmation"
end

# Option revision
__ install -l revision -d "Revert to the specified REVISION"

# Option canonical
__ list -s c -l canonical -d "Output canonical names of packages only. Implies --no-pip"

# Option explicit
__ list -l explicit -d "List explicitly all installed conda with URL (output usable by conda create --file)"

# Option export
__ list -s e -l export -d "Output requirement string only (output usable by conda create --file)"

# Option full-name
__ list -s f -l full-name -d "Only search for full names, i.e., ^<regex>\$"

# Option md5
__ list -l md5 -d "Add MD5 hashsum when using --explicit"

# Option no-pip
__ list -s c -l canonical -d "Output canonical names of packages only. Implies --no-pip"

# Option revisions
__ list -s r -l revisions -d "List the revision history and exit"

# Option all
__ remove -l all -d "Remove all packages, i.e., the entire environment"
__ update -l all -d "Update all installed packages in the environment"

# Option features
__ remove -l features -d "Remove features (instead of packages)"

# Option info
__ search -s i -l info -d "Provide detailed information about each package"

# Option platform
set -l platforms {osx,linux,win}-{32,64}
__ search -l platform -x -a "$platforms" -d "Search the given platform"

# Option reverse-dependency
__ search -l reverse-dependency -d "Perform a reverse dependency search"

# clean up
functions -e __
