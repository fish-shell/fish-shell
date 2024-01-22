# fish completion for rbenv

function __fish_rbenv_needs_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -eq 1
        return 0
    end

    return 1
end

function __fish_rbenv_using_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

function __fish_rbenv_executables
    rbenv exec --complete
end

function __fish_rbenv_installed_rubies
    rbenv versions --bare
end

function __fish_rbenv_official_rubies
    if command -sq ruby-build
        ruby-build --definitions
    else
        # Remove trailing spaces, otherwise completion options appear like
        # "\ \ option"
        rbenv install --list | sed "s/^[[:space:]]*//"
    end
end

function __fish_rbenv_prefixes
    rbenv prefix --complete
end

### commands
complete -f -c rbenv -n __fish_rbenv_needs_command -a commands -d 'List all available rbenv commands'
complete -f -c rbenv -n '__fish_rbenv_using_command commands' -a '--sh --no-sh'

### completions
complete -f -c rbenv -n __fish_rbenv_needs_command -a completions

### exec
complete -f -c rbenv -n __fish_rbenv_needs_command -a exec
complete -f -c rbenv -n '__fish_rbenv_using_command exec' -a '(__fish_rbenv_executables)'

### global
complete -f -c rbenv -n __fish_rbenv_needs_command -a global -d 'Set or show the global Ruby version'
complete -f -c rbenv -n '__fish_rbenv_using_command global' -a '(__fish_rbenv_installed_rubies)'

### help
complete -f -c rbenv -n __fish_rbenv_needs_command -a help

### hooks
complete -f -c rbenv -n __fish_rbenv_needs_command -a hooks -d 'List hook scripts for a given rbenv command'

### init
complete -f -c rbenv -n __fish_rbenv_needs_command -a init -d 'Configure the shell environment for rbenv'

### install
complete -f -c rbenv -n __fish_rbenv_needs_command -a install -d 'Install a Ruby version'
complete -f -c rbenv -n '__fish_rbenv_using_command install' -a '(__fish_rbenv_official_rubies)'

### uninstall
complete -f -c rbenv -n __fish_rbenv_needs_command -a uninstall -d 'Uninstall a Ruby version'
complete -f -c rbenv -n '__fish_rbenv_using_command uninstall' -a '(__fish_rbenv_installed_rubies)'

### local
complete -f -c rbenv -n __fish_rbenv_needs_command -a local -d 'Set or show the local application-specific Ruby version'
complete -f -c rbenv -n '__fish_rbenv_using_command local' -a '(__fish_rbenv_installed_rubies)'

### prefix
complete -f -c rbenv -n __fish_rbenv_needs_command -a prefix -d 'Display the directory where a Ruby version is installed'
complete -f -c rbenv -n '__fish_rbenv_using_command prefix' -a '(__fish_rbenv_prefixes)'

### rehash
complete -f -c rbenv -n __fish_rbenv_needs_command -a rehash -d 'Rehash rbenv shims (run this after installing executables)'

### root
complete -f -c rbenv -n __fish_rbenv_needs_command -a root -d 'Display the root directory where versions and shims are kept'

### shell
complete -f -c rbenv -n __fish_rbenv_needs_command -a shell -d 'Set or show the shell-specific Ruby version'
complete -f -c rbenv -n '__fish_rbenv_using_command shell' -a '--unset (__fish_rbenv_installed_rubies)'

### shims
complete -f -c rbenv -n __fish_rbenv_needs_command -a shims -d 'List existing rbenv shims'
complete -f -c rbenv -n '__fish_rbenv_using_command shims' -a --short

### version
complete -f -c rbenv -n __fish_rbenv_needs_command -a version -d 'Show the current Ruby version & how it was selected'

### version-file
complete -f -c rbenv -n __fish_rbenv_needs_command -a version-file -d 'Detect the file that sets the current rbenv version'

### version-file-read
complete -f -c rbenv -n __fish_rbenv_needs_command -a version-file-read

### version-file-write
complete -f -c rbenv -n __fish_rbenv_needs_command -a version-file-write

### version-name
complete -f -c rbenv -n __fish_rbenv_needs_command -a version-name -d 'Show the current Ruby version'

### version-origin
complete -f -c rbenv -n __fish_rbenv_needs_command -a version-origin -d 'Explain how the current Ruby version is set'

### versions
complete -f -c rbenv -n __fish_rbenv_needs_command -a versions -d 'List all Ruby versions known by rbenv'

### whence
complete -f -c rbenv -n __fish_rbenv_needs_command -a whence -d 'List all Ruby versions that contain the given executable'
complete -f -c rbenv -n '__fish_rbenv_using_command whence' -a --path

### which
complete -f -c rbenv -n __fish_rbenv_needs_command -a which -d 'Show the full path for the given Ruby executable'
complete -f -c rbenv -n '__fish_rbenv_using_command which' -a '(__fish_rbenv_executables)'
