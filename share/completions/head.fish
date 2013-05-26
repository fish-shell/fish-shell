complete -c head -s c -l bytes -d 'Print the first N bytes of each file; with the leading '-', print all but the last N bytes of each file' -r
complete -c head -s n -l lines -d 'print the first N lines instead of the first 10; with the leading '-', print all but the last N lines of each file' -r
complete -c head -s q -l quiet -l silent -d 'Never print headers giving file names'
complete -c head -s v -l verbose -d 'Always print headers giving file names'
complete -f -c head -l version -d 'Output version information and exit'
complete -f -c head -l help -d 'Display help and exit'
