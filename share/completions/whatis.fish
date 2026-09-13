complete -xc whatis -a "(__fish_complete_man)"

set -l is_gnu
__fish_supports_version whatis; and set is_gnu --is-gnu

__fish_gnu_complete -c whatis -s d -l debug -d Debug $is_gnu

if test -n "$is_gnu"
    complete -c whatis -s v -l verbose -d Verbose
    complete -c whatis -s r -l regex -d "Interpret each keyword as a regex"
    complete -c whatis -s w -l wildcard -d "Allow wildcards"
    complete -c whatis -s l -l long -d "Do not trim output to terminal width"
    complete -rc whatis -s C -l config-file -d "Configuration file"
    complete -xc whatis -s L -l locale -a "(command -sq locale; and locale -a)" -d "Set locale"
    complete -xc whatis -s m -l systems -d "Set system"
    complete -xc whatis -s M -l manpath -a "(__fish_complete_directories (commandline -ct))" -d Manpath
    complete -xc whatis -s s -l sections -l section -d "Search only these sections (colon-separated)"
    complete -c whatis -s '?' -l help -l usage -d "Display help and exit"
    complete -c whatis -s V -l version -d "Print program version"
else
    complete -c whatis -s s -x -d "Search only the given manual section"
end
