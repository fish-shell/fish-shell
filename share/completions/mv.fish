
# magic completion safety check (do not remove this comment)
if not type -q mv
    exit
end
complete -c mv -s b -l backup -d "Make backup of each existing destination file"
complete -c mv -s f -l force -d "Do not prompt before overwriting"
complete -c mv -s i -l interactive -d "Prompt before overwrite"
complete -c mv -l reply -x -a "yes no query" -d "Answer for overwrite questions"
complete -c mv -l strip-trailing-slashes -d "Remove trailing slashes from source"
complete -c mv -s S -l suffix -r -d "Backup suffix"
complete -c mv -l target-directory -d "Target directory" -x -a "(__fish_complete_directories (commandline -ct) 'Target directory')"
complete -c mv -s u -l update -d "Do not overwrite newer files"
complete -c mv -s v -l verbose -d "Verbose mode"
complete -c mv -l help -d "Display help and exit"
complete -c mv -l version -d "Display version and exit"

