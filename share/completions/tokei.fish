# Written against `tokei --help`, version 10.0.1 compiled with support for CBOR, JSON, and YAML

function __fish_tokei_supported_serializations
    # Expecting a line like:
    # tokei 10.0.1 compiled with serialization support: cbor, json, yaml
    command tokei --help | grep 'with serialization support' | string split --fields 2 : | string trim | string split ', '
end

complete -c tokei -s f -l files -d 'Print out statistics for individual files'
complete -c tokei -f -s h -l help -d 'Print help information'
complete -c tokei -l hidden -d 'Count hidden files, too'
complete -c tokei -f -s l -l languages -d 'Print out supported languages'
complete -c tokei -l no-ignore -d 'Don’t respect ignore files'
complete -c tokei -l no-ignore-parent -d 'Don’t respect ignore files in parent directories'
complete -c tokei -l no-ignore-vcs -d 'Don’t respect version-control ignore files'
complete -c tokei -f -s V -l version -d 'Print version information'
complete -c tokei -s v -l verbose -d 'Increase log output level'

# Options
complete -c tokei -x -s c -l columns -d 'Set column width for terminal output'
complete -c tokei -x -s e -l exclude -d 'Ignore all files and directories containing this word'
complete -c tokei -r -s i -l input -d 'Gives statistics from a previous Tokei run'
complete -c tokei -x -s o -l output -a '(__fish_tokei_supported_serializations)' -d 'Choose output format'
complete -c tokei -x -s s -l sort -ka 'files lines code comments blanks' -d 'Sort languages based on column'
complete -c tokei -x -s t -l type -a '(command tokei --languages)' -d 'Filters output by language type, comma-separated'
