
complete -c locate -s b -l basename -d (N_ "Match only the base name against the specified patterns")
complete -c locate -s c -l count -d (N_ "Instead of writing file names on standard output, write the number of matching entries only")
complete -c locate -s d -l database -d (N_ "Replace the default database with specified path") -r
complete -c locate -s e -l existing -d (N_ "Print only entries that refer to files existing at the time locate is run")
complete -c locate -s L -l follow -d (N_ "Follow symlinks when checking for existing files")
complete -c locate -s h -l help -d (N_ "Display help and exit")
complete -c locate -s i -l ignore-case -d (N_ "Ignore case distinctions when matching patterns")
complete -c locate -s l -s n -l limit -d (N_ "Exit successfully after finding specified number of entires") -r
complete -c locate -s m -l mmap -d (N_ "Ignored")
complete -c locate -s P -s H -l nofollow -d (N_ "Do not follow symlinks when checking for existing files")
complete -c locate -s 0 -l null -d (N_ "Separate  the  entries  on  output  using the ASCII NUL character instead of writing each entry on a separate line")
complete -c locate -s S -l statistics -d (N_ "Write statistics about each read database to standard output")
complete -c locate -s q -l quiet -d (N_ "Write no messages about errors encountered")
complete -c locate -s r -l regexp -d (N_ "Search for specified basic regex") -r
complete -c locate -l regex -d (N_ "Interpret all patterns as extended regexps")
complete -c locate -s s -l stdio -d (N_ "Ignored")
complete -c locate -s V -l version -d (N_ "Display version and exit")
complete -c locate -s w -l wholename -d (N_ "Match only the whole path name against the specified patterns")
