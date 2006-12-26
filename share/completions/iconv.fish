

complete -c iconv -s f -l from-code -d (N_ "Convert from specified encoding") -x -a "(iconv --list|sed -e 's|//||')"
complete -c iconv -s t -l to-code -d (N_ "Convert to specified encoding") -x -a "(iconv --list|sed -e 's|//||')"
complete -c iconv -l list -d (N_ "List known coded character sets")
complete -c iconv -s o -l output -d (N_ "Output file") -r
complete -c iconv -l verbose -d (N_ "Print progress information")
complete -c iconv -l help -d (N_ "Display version and exit")
complete -c iconv -l version -d (N_ "Display help and exit")

