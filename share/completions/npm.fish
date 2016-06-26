# NPM (https://npmjs.org) completions for Fish shell
# __fish_npm_needs_* and __fish_npm_using_* taken from:
# https://stackoverflow.com/questions/16657803/creating-autocomplete-script-with-sub-commands
# see also Fish's large set of completions for examples:
# https://github.com/fish-shell/fish-shell/tree/master/share/completions

function __fish_npm_needs_command
    set cmd (commandline -opc)

    if [ (count $cmd) -eq 1 ]
        return 0
    end

    return 1
end

function __fish_npm_using_command
    set cmd (commandline -opc)

    if [ (count $cmd) -gt 1 ]
        if [ $argv[1] = $cmd[2] ]
            return 0
        end
    end

    return 1
end

function __fish_npm_needs_option
    switch (commandline -ct)
        case "-*"
            return 0
    end
    return 1
end

function __fish_complete_npm --description "Complete the commandline using npm's 'completion' tool"
    # Note that this function will generate undescribed completion options, and current fish
    # will sometimes pick these over versions with descriptions.
    # However, this seems worth it because it means automatically getting _some_ completions if npm updates.

    # Defining an npm alias that automatically calls nvm if necessary is a popular convenience measure.
    # Because that is a function, these local variables won't be inherited and the completion would fail
    # with weird output on stdout (!). But before the function is called, no npm command is defined,
    # so calling the command would fail.
    # So we'll only try if we have an npm command.
    if command -s npm >/dev/null
        # npm completion is bash-centric, so we need to translate fish's "commandline" stuff to bash's $COMP_* stuff
        # COMP_LINE is an array with the words in the commandline
        set -lx COMP_LINE (commandline -o)
        # COMP_CWORD is the index of the current word in COMP_LINE
        # bash starts arrays with 0, so subtract 1
        set -lx COMP_CWORD (math (count $COMP_LINE) - 1)
        # COMP_POINT is the index of point/cursor when the commandline is viewed as a string
        set -lx COMP_POINT (commandline -C)
        # If the cursor is after the last word, the empty token will disappear in the expansion
        # Readd it
        if test (commandline -ct) = ""
            set COMP_CWORD (math $COMP_CWORD + 1)
            set COMP_LINE $COMP_LINE ""
        end
        command npm completion -- $COMP_LINE ^/dev/null
    end
end

# use npm completion for most of the things,
# except options completion because it sucks at it.
# see: https://github.com/npm/npm/issues/9524
# and: https://github.com/fish-shell/fish-shell/pull/2366
complete -f -c npm -n 'not __fish_npm_needs_option' -a "(__fish_complete_npm)"

# list available npm scripts and their parial content
function __fish_npm_run
    # Like above, only try to call npm if there's a command by that name to facilitate aliases that call nvm.
    if command -s npm >/dev/null
        command npm run | string match -r -v '^[^ ]|^$' | string trim | while read -l name
            set -l trim 20
            read -l value
            echo "$value" | cut -c1-$trim | read -l value
            printf "%s\t%s\n" $name $value
        end
    end
end

# run
for c in run run-script
    complete -f -c npm -n "__fish_npm_using_command $c" -a "(__fish_npm_run)"
end

# cache
complete -f -c npm -n '__fish_npm_needs_command' -a 'cache' -d "Manipulates package's cache"
complete -f -c npm -n '__fish_npm_using_command cache' -a 'add' -d 'Add the specified package to the local cache'
complete -f -c npm -n '__fish_npm_using_command cache' -a 'clean' -d 'Delete  data  out of the cache folder'
complete -f -c npm -n '__fish_npm_using_command cache' -a 'ls' -d 'Show the data in the cache'

# config
for c in 'c' 'config'
  complete -f -c npm -n "__fish_npm_needs_command" -a "$c" -d 'Manage the npm configuration files'
  complete -f -c npm -n "__fish_npm_using_command $c" -a 'set' -d 'Sets the config key to the value'
  complete -f -c npm -n "__fish_npm_using_command $c" -a 'get' -d 'Echo the config value to stdout'
  complete -f -c npm -n "__fish_npm_using_command $c" -a 'delete' -d 'Deletes the key from all configuration files'
  complete -f -c npm -n "__fish_npm_using_command $c" -a 'list' -d 'Show all the config settings'
  complete -f -c npm -n "__fish_npm_using_command $c" -a 'ls' -d 'Show all the config settings'
  complete -f -c npm -n "__fish_npm_using_command $c" -a 'edit' -d 'Opens the config file in an editor'
end
# get, set also exist as shorthands
complete -f -c npm -n "__fish_npm_needs_command" -a 'get' -d 'Echo the config value to stdout'
complete -f -c npm -n "__fish_npm_needs_command" -a 'set' -d 'Sets the config key to the value'

# install
for c in 'install' 'isntall' 'i'
    complete -f -c npm -n '__fish_npm_needs_command' -a "$c" -d 'install a package'
    complete -f -c npm -n "__fish_npm_using_command $c" -l save-dev -d 'Save to devDependencies in package.json'
    complete -f -c npm -n "__fish_npm_using_command $c" -l save -d 'Save to dependencies in package.json'
    complete -f -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Install package globally'
end

# list
for c in 'la' 'list' 'll' 'ls'
  complete -f -c npm -n '__fish_npm_needs_command' -a "$c" -d 'List installed packages'
  complete -f -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'List packages in the global install prefix instead of in the current project'
  complete -f -c npm -n "__fish_npm_using_command $c" -l json -d 'Show information in JSON format'
  complete -f -c npm -n "__fish_npm_using_command $c" -l long -d 'Show extended information'
  complete -f -c npm -n "__fish_npm_using_command $c" -l parseable -d 'Show parseable output instead of tree view'
  complete -x -c npm -n "__fish_npm_using_command $c" -l depth -d 'Max display depth of the dependency tree'
end

# owner
complete -f -c npm -n '__fish_npm_needs_command' -a 'owner' -d 'Manage package owners'
complete -f -c npm -n '__fish_npm_using_command owner' -a 'ls' -d 'List package owners'
complete -f -c npm -n '__fish_npm_using_command owner' -a 'add' -d 'Add a new owner to package'
complete -f -c npm -n '__fish_npm_using_command owner' -a 'rm' -d 'Remove an owner from package'

# remove
for c in 'r' 'remove' 'rm' 'un' 'uninstall' 'unlink'
    complete -f -c npm -n '__fish_npm_needs_command' -a "$c" -d 'remove package'
    complete -x -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'remove global package'
    complete -x -c npm -n "__fish_npm_using_command $c" -l save -d 'Package will be removed from your dependencies'
    complete -x -c npm -n "__fish_npm_using_command $c" -l save-dev -d 'Package will be removed from your devDependencies'
    complete -x -c npm -n "__fish_npm_using_command $c" -l save-optional -d 'Package will be removed from your optionalDependencies'
end

# search
for c in 'find' 's' 'se' 'search'
    complete -f -c npm -n '__fish_npm_needs_command' -a "$c" -d 'Search for packages'
    complete -x -c npm -n "__fish_npm_using_command $c" -l long -d 'Display full package descriptions and other long text across multiple lines'
end

# update
for c in 'up' 'update'
    complete -f -c npm -n '__fish_npm_needs_command' -a "$c" -d 'Update package(s)'
    complete -f -c npm -n "__fish_npm_using_command $c" -s g -l global -d 'Update global package(s)'
end

# misc shorter explanations
complete -f -c npm -n '__fish_npm_needs_command' -a 'adduser add-user login' -d 'Add a registry user account'
complete -f -c npm -n '__fish_npm_needs_command' -a 'bin' -d 'Display npm bin folder'
complete -f -c npm -n '__fish_npm_needs_command' -a 'bugs issues' -d 'Bugs for a package in a web browser maybe'
complete -f -c npm -n '__fish_npm_needs_command' -a 'ddp dedupe find-dupes' -d 'Reduce duplication'
complete -f -c npm -n '__fish_npm_needs_command' -a 'deprecate' -d 'Deprecate a version of a package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'docs home' -d 'Docs for a package in a web browser maybe'
complete -f -c npm -n '__fish_npm_needs_command' -a 'edit' -d 'Edit an installed package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'explore' -d 'Browse an installed package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'faq' -d 'Frequently Asked Questions'
complete -f -c npm -n '__fish_npm_needs_command' -a 'help-search' -d 'Search npm help documentation'
complete -f -c npm -n '__fish_npm_using_command help-search' -l long -d 'Display full package descriptions and other long text across multiple lines'
complete -f -c npm -n '__fish_npm_needs_command' -a 'info v view' -d 'View registry info'
complete -f -c npm -n '__fish_npm_needs_command' -a 'link ln' -d 'Symlink a package folder'
complete -f -c npm -n '__fish_npm_needs_command' -a 'outdated' -d 'Check for outdated packages'
complete -f -c npm -n '__fish_npm_needs_command' -a 'pack' -d 'Create a tarball from a package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'prefix' -d 'Display NPM prefix'
complete -f -c npm -n '__fish_npm_needs_command' -a 'prune' -d 'Remove extraneous packages'
complete -c npm -n '__fish_npm_needs_command' -a 'publish' -d 'Publish a package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'rb rebuild' -d 'Rebuild a package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'root ' -d 'Display npm root'
complete -f -c npm -n '__fish_npm_needs_command' -a 'run-script run' -d 'Run arbitrary package scripts'
complete -f -c npm -n '__fish_npm_needs_command' -a 'shrinkwrap' -d 'Lock down dependency versions'
complete -f -c npm -n '__fish_npm_needs_command' -a 'star' -d 'Mark your favorite packages'
complete -f -c npm -n '__fish_npm_needs_command' -a 'stars' -d 'View packages marked as favorites'
complete -f -c npm -n '__fish_npm_needs_command' -a 'start' -d 'Start a package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'stop' -d 'Stop a package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'submodule' -d 'Add a package as a git submodule'
complete -f -c npm -n '__fish_npm_needs_command' -a 't tst test' -d 'Test a package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'unpublish' -d 'Remove a package from the registry'
complete -f -c npm -n '__fish_npm_needs_command' -a 'unstar' -d 'Remove star from a package'
complete -f -c npm -n '__fish_npm_needs_command' -a 'version' -d 'Bump a package version'
complete -f -c npm -n '__fish_npm_needs_command' -a 'whoami' -d 'Display npm username'
