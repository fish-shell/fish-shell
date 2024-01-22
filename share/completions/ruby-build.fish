function __fish_ruby-build_needs_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -eq 1
        return 0
    end
    return 1
end

function __fish_ruby-build_definitions
    ruby-build --definitions
end

complete -f -c ruby-build -n __fish_ruby-build_needs_command -a '(__fish_ruby-build_definitions)' -d Definition

complete -f -c ruby-build -n __fish_ruby-build_needs_command -l keep -s k -d 'Do not remove source tree after installation'
complete -f -c ruby-build -n __fish_ruby-build_needs_command -l verbose -s v -d 'Verbose mode: print compilation status to stdout'
complete -f -c ruby-build -n __fish_ruby-build_needs_command -l definitions -d 'List all built-in definitions'

complete -f -c ruby-build -n __fish_ruby-build_needs_command -l help -s h -d 'Display help information'
