complete -c mv -s b -l backup -d (_ "Make backup of each existing destination file")
complete -c mv -s f -l force -d (_ "Do not prompt before overwriting")
complete -c mv -s i -l interactive -d (_ "Prompt before overwrite")
complete -c mv -l reply -x -a "yes no query" -d (_ "Answer for overwrite questions")
complete -c mv -l strip-trailing-slashes -d (_ "Remove trailing slashes from source")
complete -c mv -s S -l suffix -r -d (_ "Backup suffix")
complete -c mv -l target-directory -d (_ "Target directory") -x -a "(__fish_complete_directory (commandline -ct) 'Target directory')"
complete -c mv -s u -l update -d (_ "Do not overwrite newer files")
complete -c mv -s v -l vervose -d (_ "Verbose mode")
complete -c mv -l help -d (_ "Display help and exit")
complete -c mv -l version -d (_ "Display version and exit")

