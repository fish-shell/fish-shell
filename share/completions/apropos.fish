function __fish_complete_apropos
    if test (commandline -ct)
        switch (commandline -ct)
            case '-**'

            case '*'
                apropos $str 2>/dev/null | string replace -rf -- "^(.*$str([^ ]*).*)" "$str\$2\t\$1"
        end
    end
end

complete -xc apropos -a '(__fish_complete_apropos)' -d "whatis entry"

complete -f -c apropos -s '?' -l help -d "Display help and exit"
complete -f -c apropos -l usage -d "Display short usage message"
complete -f -c apropos -s d -l debug -d "Print debugging info"
complete -f -c apropos -s v -l verbose -d "Verbose mode"
complete -f -c apropos -s r -l regex -d "Keyword as regex (default)"
complete -f -c apropos -s w -l wildcard -d "Keyword as wildcards"
complete -f -c apropos -s e -l exact -d "Keyword as exactly match"
complete -x -c apropos -s m -l system -d "Search for other system"
complete -x -c apropos -s M -l manpath -a "(__fish_complete_directories (commandline -ct))" -d Manpath
complete -r -c apropos -s C -l config-file -d "Specify a configuration file"
complete -f -c apropos -s V -l version -d "Display version and exit"
complete -f -c apropos -s a -l and -d "Match all keywords"
complete -f -c apropos -s l -l long -d "Do not trim output to terminal width"
complete -x -c apropos -s s -l sections -l section -d "Search only these sections (colon-separated)"
complete -x -c apropos -s L -l locale -a "(command -sq locale; and locale -a)" -d "Set locale"
