#Completions for rm
complete -c rm -s d -l directory -d (N_ "Unlink directory (Only by superuser)")
complete -c rm -s f -l force -d (N_ "Never prompt before removal")
complete -c rm -s i -l interactive -d (N_ "Prompt before removal")
complete -c rm -s r -l recursive -d (N_ "Recursively remove subdirectories")
complete -c rm -s R -d (N_ "Recursively remove subdirectories")
complete -c rm -s v -l verbose -d (N_ "Explain what is done")
complete -c rm -s h -l help -d (N_ "Display help and exit")
complete -c rm -l version -d (N_ "Display version and exit")


