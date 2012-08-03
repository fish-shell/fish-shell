complete -c watch -s b -l beep             -d 'Beep if command has a non-zero exit'
complete -c watch -s c -l color            -d 'Interpret ANSI color sequences'
complete -c watch -s d -l differences      -d 'Highlight changes between updates'
complete -c watch -s d -l differences=permanent  -d 'Highlight changes between updates'
complete -c watch -s e -l errexit          -d 'Exit if command has a non-zero exit'
complete -c watch -s g -l chgexit          -d 'Exit when output from command changes'
complete -c watch -s n -l interval         -d 'Seconds to wait between updates' -x
complete -c watch -s p -l precise          -d 'Attempt run command in precise intervals'
complete -c watch -s t -l no-title         -d 'Turn off header'
complete -c watch -s x -l exec             -d 'Pass command to exec instead of "sh -c"'
complete -c watch -s h -l help             -d 'Display this help and exit'
complete -c watch -s v -l version          -d 'Output version information and exit'
complete -c watch -xa '(__fish_complete_subcommand -- -n --interval)'

