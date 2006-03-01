complete -c chown -s c -l changes -d (N_ "Output diagnostic for changed files")
complete -c chown -l dereference -d (N_ "Dereferense symbolic links")
complete -c chown -s h -l no-dereference -d (N_ "Do not dereference symbolic links")
complete -c chown -l from -d (N_ "Change from owner/group")
complete -c chown -s f -l silent -d (N_ "Supress errors")
complete -c chown -l reference -d (N_ "Use same owner/group as file") -r
complete -c chown -s R -l recursive -d (N_ "Operate recursively")
complete -c chown -s v -l verbose -d (N_ "Output diagnostic for every file")
complete -c chown -s h -l help -d (N_ "Display help and exit")
complete -c chown -l version -d (N_ "Display version and exit")
complete -c chown -d (N_ "Username") -a "(__fish_print_users):"
complete -c chown -d (N_ "Username") -a "(echo (commandline -ct)|grep -o '.*:')(cat /etc/group |cut -d : -f 1)"
