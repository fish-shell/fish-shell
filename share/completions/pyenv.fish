# fish completion for pyenv

function __fish_pyenv_needs_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -eq 1
        return 0
    end
    return 1
end

function __fish_pyenv_using_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

function __fish_pyenv_executables
    pyenv exec --complete
end

function __fish_pyenv_installed_versions
    pyenv versions --bare
end

function __fish_pyenv_available_versions
    # Remove trailing spaces, otherwise completion options appear like
    # "\ \ option"
    pyenv install --list | sed "s/^[[:space:]]*//"
end

function __fish_pyenv_prefixes
    pyenv prefix --complete
end

### commands
complete -f -c pyenv -n __fish_pyenv_needs_command -a commands -d 'List all available pyenv commands'
complete -f -c pyenv -n '__fish_pyenv_using_command commands' -a '--sh --no-sh'

### completions
complete -f -c pyenv -n __fish_pyenv_needs_command -a completions

### exec
complete -f -c pyenv -n __fish_pyenv_needs_command -a exec
complete -f -c pyenv -n '__fish_pyenv_using_command exec' -a '(__fish_pyenv_executables)'

### global
complete -f -c pyenv -n __fish_pyenv_needs_command -a global -d 'Set or show the global Python version'
complete -f -c pyenv -n '__fish_pyenv_using_command global' -a '(__fish_pyenv_installed_versions)'

### help
complete -f -c pyenv -n __fish_pyenv_needs_command -a help

### hooks
complete -f -c pyenv -n __fish_pyenv_needs_command -a hooks -d 'List hook scripts for a given pyenv command'

### init
complete -f -c pyenv -n __fish_pyenv_needs_command -a init -d 'Configure the shell environment for pyenv'

### install
complete -f -c pyenv -n __fish_pyenv_needs_command -a install -d 'Install a Python version'
complete -f -c pyenv -n '__fish_pyenv_using_command install' -a '(__fish_pyenv_available_versions)'

### local
complete -f -c pyenv -n __fish_pyenv_needs_command -a local -d 'Set or show the local application-specific Python version'
complete -f -c pyenv -n '__fish_pyenv_using_command local' -a '(__fish_pyenv_installed_versions)'

### prefix
complete -f -c pyenv -n __fish_pyenv_needs_command -a prefix -d 'Display the directory where a Python version is installed'
complete -f -c pyenv -n '__fish_pyenv_using_command prefix' -a '(__fish_pyenv_prefixes)'

### rehash
complete -f -c pyenv -n __fish_pyenv_needs_command -a rehash -d 'Rehash pyenv shims (run this after installing executables)'

### root
complete -f -c pyenv -n __fish_pyenv_needs_command -a root -d 'Display the root directory where versions and shims are kept'

### shell
complete -f -c pyenv -n __fish_pyenv_needs_command -a shell -d 'Set or show the shell-specific Python version'
complete -f -c pyenv -n '__fish_pyenv_using_command shell' -a '--unset (__fish_pyenv_installed_versions)'

### shims
complete -f -c pyenv -n __fish_pyenv_needs_command -a shims -d 'List existing pyenv shims'
complete -f -c pyenv -n '__fish_pyenv_using_command shims' -a --short

### version
complete -f -c pyenv -n __fish_pyenv_needs_command -a version -d 'Show the current Python version & how it was selected'

### version-file
complete -f -c pyenv -n __fish_pyenv_needs_command -a version-file -d 'Detect the file that sets the current pyenv version'

### version-file-read
complete -f -c pyenv -n __fish_pyenv_needs_command -a version-file-read

### version-file-write
complete -f -c pyenv -n __fish_pyenv_needs_command -a version-file-write

### version-name
complete -f -c pyenv -n __fish_pyenv_needs_command -a version-name -d 'Show the current Python version'

### version-origin
complete -f -c pyenv -n __fish_pyenv_needs_command -a version-origin -d 'Explain how the current Python version is set'

### versions
complete -f -c pyenv -n __fish_pyenv_needs_command -a versions -d 'List all Python versions known by pyenv'

### whence
complete -f -c pyenv -n __fish_pyenv_needs_command -a whence -d 'List all Python versions that contain the given executable'
complete -f -c pyenv -n '__fish_pyenv_using_command whence' -a --path

### which
complete -f -c pyenv -n __fish_pyenv_needs_command -a which -d 'Show the full path for the given Python executable'
complete -f -c pyenv -n '__fish_pyenv_using_command which' -a '(__fish_pyenv_executables)'
