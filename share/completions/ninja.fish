function __fish_ninja
    set -l saved_args $argv
    set -l dir .
    if argparse -i C/dir= -- (commandline -xpc)
        command ninja -C$_flag_C $saved_args
    end
end

function __fish_print_ninja_tools
    __fish_ninja -t list | string match -v '*:' | string replace -r '\s+(\w+).*' '$1'
end

function __fish_print_ninja_targets
    __fish_ninja -t targets 2>/dev/null | string replace -r ':.*' ''
    __fish_ninja -t targets all 2>/dev/null | string replace -r ':.*' '' | string match -e -r '\.(?:o|so|a)$'
end
complete -c ninja -f -a '(__fish_print_ninja_targets)' -d target
complete -x -c ninja -s t -x -a "(__fish_print_ninja_tools)" -d subtool
complete -x -c ninja -s C -x -a "(__fish_complete_directories (commandline -ct))" -d "change to specified directory"
complete -c ninja -s f -k -x -a "(__fish_complete_suffix .ninja)" -d "specify build file [default=build.ninja]"
complete -f -c ninja -s n -d "dry run"
complete -f -c ninja -s v -d "show all command lines while building"
complete -f -c ninja -s j -d "number of jobs to run in parallel [default derived from CPUs]"
complete -f -c ninja -s l -d "do not start if load average > N"
complete -f -c ninja -s k -d "keep going until N jobs fail [default=1]"
complete -f -c ninja -s h -d "show help"
complete -f -c ninja -s d -d "enable debugging, specify debug mode"
complete -f -c ninja -s w -d "adjust warnings, specify flags"
complete -f -c ninja -s v -l verbose -d "show all command lines while building"
complete -f -c ninja -l version -d "print ninja version"
