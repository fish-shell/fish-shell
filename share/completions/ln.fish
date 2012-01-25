# completions for ln
# Author: SanskritFritz (gmail)

complete -c ln -f -s s -l symbolic                   -d 'make symbolic links instead of hard links'
complete -c ln -f      -l backup=CONTROL             -d 'make a backup of each existing destination file'
complete -c ln -f -s b                               -d 'like --backup but does not accept an argument'
complete -c ln -f -s d -l directory                  -d 'allow  the  superuser to attempt to hard link directories'
complete -c ln -f -s f -l force                      -d 'remove existing destination files'
complete -c ln -f -s i -l interactive                -d 'prompt whether to remove destinations'
complete -c ln -f -s L -l logical                    -d 'make hard links to symbolic link references'
complete -c ln -f -s n -l no-dereference             -d 'treat destination that is a symlink to a directory as if it were a normal file'
complete -c ln -f -s P -l physical                   -d 'make hard links directly to symbolic links'
complete -c ln -f -s S -l suffix=SUFFIX              -d 'override the usual backup suffix'
complete -c ln -f -s t -l target-directory=DIRECTORY -d 'specify the DIRECTORY in which to create the links'
complete -c ln -f -s T -l no-target-directory        -d 'treat LINK_NAME as a normal file'
complete -c ln -f -s v -l verbose                    -d 'print name of each linked file'
complete -c ln -f      -l help                       -d 'display this help and exit'
complete -c ln -f      -l version                    -d 'output version information and exit'
