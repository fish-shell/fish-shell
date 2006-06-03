complete -c mv -s b -l backup -d (N_ "Make backup of each existing destination file")
complete -c mv -s f -l force -d (N_ "Do not prompt before overwriting")
complete -c mv -s i -l interactive -d (N_ "Prompt before overwrite")
complete -c mv -l reply -x -a "yes no query" -d (N_ "Answer for overwrite questions")
complete -c mv -l strip-trailing-slashes -d (N_ "Remove trailing slashes from source")
complete -c mv -s S -l suffix -r -d (N_ "Backup suffix")
complete -c mv -l target-directory -d (N_ "Target directory") -x -a "(__fish_complete_directory (commandline -ct) 'Target directory')"
complete -c mv -s u -l update -d (N_ "Do not overwrite newer files")
complete -c mv -s v -l verbose -d (N_ "Verbose mode")
complete -c mv -l help -d (N_ "Display help and exit")
complete -c mv -l version -d (N_ "Display version and exit")

