
complete -c readlink -s f -l canonicalize -d 'Canonicalize, follow symlinks, last can be missing'
complete -c readlink -s e -l canonicalize-existing -d 'Canonicalize, follow symlinks, none can be missing'
complete -c readlink -s m -l canonicalize-missing -d 'Canonicalize, follow symlinks, all can be missing'
complete -c readlink -s n -l no-newline -d 'Do not output the trailing newline'
complete -c readlink -s q -l quiet -s s -l silent -d 'Suppress most error messages'
complete -c readlink -s v -l verbose -d 'Report error messages'
complete -c readlink -l help -d 'Display this help and exit'
complete -c readlink -l version -d 'Output version information and exit'
