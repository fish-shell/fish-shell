#Completions for rm
complete -c rm -s d -l directory -d (_ "Unlink directory (Only by superuser)")
complete -c rm -s f -l force -d (_ "Never prompt before removal")
complete -c rm -s i -l interactive -d (_ "Prompt before removal")
complete -c rm -s r -l recursive -d (_ "Recursively remove subdirectories")
complete -c rm -s R -d (_ "Recursively remove subdirectories")
complete -c rm -s v -l verbose -d (_ "Explain what is done")
complete -c rm -s h -l help -d (_ "Display help and exit")
complete -c rm -l version -d (_ "Display version and exit")


