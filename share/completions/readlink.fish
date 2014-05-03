
complete -c readlink -s f -l canonicalize          --description 'Canonicalize, follow symlinks, last can be missing'
complete -c readlink -s e -l canonicalize-existing --description 'Canonicalize, follow symlinks, none can be missing'
complete -c readlink -s m -l canonicalize-missing  --description 'Canonicalize, follow symlinks, all can be missing'
complete -c readlink -s n -l no-newline            --description 'Do not output the trailing newline'
complete -c readlink -s q -l quiet -s s -l silent  --description 'Suppress most error messages'
complete -c readlink -s v -l verbose               --description 'Report error messages'
complete -c readlink      -l help                  --description 'Display this help and exit'
complete -c readlink      -l version               --description 'Output version information and exit'
