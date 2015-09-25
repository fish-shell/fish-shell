complete -c pkgfile -l version         -d 'show program\'s version number and exit'
complete -c pkgfile -s h -l help       -d 'show this help message and exit'
complete -c pkgfile -s b -l binaries   -d 'only show files in a {s}bin/ directory. Works with -s, -l'
complete -c pkgfile -s c -l case-sensitive  -d 'make searches case sensitive'
complete -c pkgfile -s g -l glob       -d 'allow the use of * and ? as wildcards'
complete -c pkgfile -s r -l regex      -d 'allow the use of regex in searches'
complete -c pkgfile -s R -l repo       -d 'search only in the specified repository' -xa '(__fish_print_pacman_repos)'
complete -c pkgfile -s v -l verbose    -d 'enable verbose output'
complete -c pkgfile -s i -l info       -d 'provides information about the package owning a file' -r
complete -c pkgfile -s l -l list       -d 'list files of a given package; similar to "pacman -Ql"' -xa "(pacman -Sl | cut --delim ' ' --fields 2- | tr ' ' \t | sort)"
complete -c pkgfile -s s -l search     -d 'search which package owns a file' -r
complete -c pkgfile -s u -l update     -d 'update to the latest filelist. This requires write permission to /var/cache/pkgtools/lists'
