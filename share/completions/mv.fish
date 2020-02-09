set -l uname (uname -s)

## GNU mv
if mv --version >/dev/null 2>/dev/null
    # --backup requires an argument, -b does not accept an argument
    complete -c mv -l backup -r -d "Backup each existing destination file" \
        -x -ka "none\t'Never make backups'
                     off\t'Never make backups'
                     numbered\t'Make numbered backups'
                     t\t'Make numbered backups'
                     existing\t'Numbered backups if any exist, else simple'
                     nil\t'Numbered backups if any exist, else simple'
                     simple\t'Make simple backups'
                     never\t'Make simple backups'"
    complete -c mv -s b -d "Backup each existing destination file"
    complete -c mv -s f -l force -d "Don't prompt to overwrite"
    complete -c mv -s i -l interactive -d "Prompt to overwrite"
    complete -c mv -s n -l no-clobber -d "Don't overwrite existing"
    # --reply has been deprecated for over a decade, and now GNU mv does not accept this option.
    # Better to use -f instead of --reply=yes.
    #   complete -c mv -l reply -x -a "yes no query" -d "Answer for overwrite questions"
    complete -c mv -l strip-trailing-slashes -d "Remove trailing '/' from source args"
    complete -c mv -s S -l suffix -x -d "Override default backup suffix"
    complete -c mv -s t -l target-directory -d "Move all source args into DIR" \
        -x -a "(__fish_complete_directories (commandline -ct) 'Directory')"
    complete -c mv -s T -l no-target-directory -d "Treat DEST as a normal file"
    complete -c mv -s u -l update -d "Don't overwrite newer"
    complete -c mv -s v -l verbose -d "Print filenames as it goes"
    test "$uname" = Linux
    and complete -c mv -s Z -l context -d "Set SELinux context to default"

    complete -c mv -l help -d "Print help and exit"
    complete -c mv -l version -d "Print version and exit"
    ## BSD-ish mv
else #[posix][ext]
    # freebsd:   mv [-fi][nvh] src dst
    # dragonfly: mv [-fi][nvh] src dst
    # macos:     mv [-fi][nv ] src dst
    # netbsd:    mv [-fi][ v ] src dst
    # openbsd:   mv [-fi][ v ] src dst
    # solaris:   mv [-fi][   ] src dst

    # POSIX options
    complete -c mv -s f -d "Don't prompt to overwrite"
    complete -c mv -s i -d "Prompt to overwrite existing"

    test uname = SunOS # -fi
    and exit 0

    # Extensions
    complete -c mv -s v -d "Print filenames as it goes"

    contains "$uname" NetBSD OpenBSD # -fiv
    and exit 0

    complete -c mv -s n -d "Don't overwrite existing"

    test "$uname" = Darwin # -fivn
    and exit 0

    complete -c mv -s h -d "Don't follow target if it links to a dir"
end
