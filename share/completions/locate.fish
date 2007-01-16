
complete -c locate -s b -l basename --description "Match only the base name against the specified patterns"
complete -c locate -s c -l count --description "Instead of writing file names on standard output, write the number of matching entries only"
complete -c locate -s d -l database --description "Replace the default database with specified path" -r
complete -c locate -s e -l existing --description "Print only entries that refer to files existing at the time locate is run"
complete -c locate -s L -l follow --description "Follow symlinks when checking for existing files"
complete -c locate -s h -l help --description "Display help and exit"
complete -c locate -s i -l ignore-case --description "Ignore case distinctions when matching patterns"
complete -c locate -s l -s n -l limit --description "Exit successfully after finding specified number of entires" -r
complete -c locate -s m -l mmap --description "Ignored"
complete -c locate -s P -s H -l nofollow --description "Do not follow symlinks when checking for existing files"
complete -c locate -s 0 -l null --description "Separate  the  entries  on  output  using the ASCII NUL character instead of writing each entry on a separate line"
complete -c locate -s S -l statistics --description "Write statistics about each read database to standard output"
complete -c locate -s q -l quiet --description "Write no messages about errors encountered"
complete -c locate -s r -l regexp --description "Search for specified basic regex" -r
complete -c locate -l regex --description "Interpret all patterns as extended regexps"
complete -c locate -s s -l stdio --description "Ignored"
complete -c locate -s V -l version --description "Display version and exit"
complete -c locate -s w -l wholename --description "Match only the whole path name against the specified patterns"
