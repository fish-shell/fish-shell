complete -c chown -s c -l changes -d (_ "Output diagnostic for changed files")
complete -c chown -l dereference -d (_ "Dereferense symbolic links")
complete -c chown -s h -l no-dereference -d (_ "Do not dereference symbolic links")
complete -c chown -l from -d (_ "Change from owner/group")
complete -c chown -s f -l silent -d (_ "Supress errors")
complete -c chown -l reference -d (_ "Use same owner/group as file") -r
complete -c chown -s R -l recursive -d (_ "Operate recursively")
complete -c chown -s v -l verbose -d (_ "Output diagnostic for every file")
complete -c chown -s h -l help -d (_ "Display help and exit")
complete -c chown -l version -d (_ "Display version and exit")
complete -c chown -d (_ "Username") -a "(__fish_print_users):"
complete -c chown -d (_ "Username") -a "(echo (commandline -ct)|grep -o '.*:')(cat /etc/group |cut -d : -f 1)"
