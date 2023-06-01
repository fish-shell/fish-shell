# openocd is a utility for programming microcontrollers, especially popular with
# developers targeting ARM devices. It has a number of pre-installed configuration
# scripts to control its connection behavior which are in its installation directory
# and may be specified at the command line via relative paths as if you were cd'd
# into that directory.

# Retrieve the likely compile-time PREFIX for openocd based off of where the binary
# is located. e.g. if openocd is /usr/local/bin/openocd return /usr/local
function __fish_openocd_prefix
    string replace /bin/openocd "" (command -s openocd)
end

# The results of this function are as if __fish_complete_suffix were called
# while cd'd into the openocd scripts directory
function __fish_complete_openocd_path
    __fish_complete_suffix --prefix=(__fish_openocd_prefix)/share/openocd/scripts "$argv[1]"
end

complete -c openocd -f # at no point does openocd take arbitrary arguments
complete -c openocd -s h -l help -d "display help"
complete -c openocd -s v -l version -d "display version info"
complete -c openocd -s f -l file -k -xa '(__fish_complete_openocd_path .cfg)'
complete -c openocd -s d -l debug -d "run under debug level 3"
complete -c openocd -s d -l debug -d "run under debug level n" -xa '(seq 1 9)'
complete -c openocd -s l -l log_output -d "redirect output to file" -r
complete -c openocd -s c -l command -d "run command"
complete -c openocd -s s -l search -d "search path for config files and scripts" -xa '(__fish_complete_directories)'
