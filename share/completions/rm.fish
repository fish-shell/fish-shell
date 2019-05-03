#Completions for rm
if rm --version >/dev/null 2>/dev/null # GNU
    complete -c rm -s d -l directory -d "Unlink directories"
    complete -c rm -s f -l force -d "Never prompt for removal"
    complete -c rm -s i -l interactive -d "Prompt for removal"
    complete -c rm -s I -d "Prompt to remove >3 files"
    complete -c rm -s r -l recursive -d "Recursively remove subdirs"
    complete -c rm -s R -d "Recursively remove subdirs"
    complete -c rm -s v -l verbose -d "Explain what is done"
    complete -c rm -s h -l help -d "Display help"
    complete -c rm -l version -d "Display rm version"
else
    set -l uname (uname -s)
    # solaris:   rm [-fi        ] file ...
    # openbsd:   rm [-fidPRrv   ] file ...
    # macos:     rm [-fidPRrvW  ] file ...
    # netbsd:    rm [-fidPRrvWx ] file ...
    # freebsd:   rm [-fidPRrvWxI] file ...
    # dragonfly: rm [-fidPRrvWxI] file ... 

    complete -c rm -s f -d "Never prompt for removal"
    complete -c rm -s i -d "Prompt for removal"
    test "$uname" = SunOS
    and exit 0
    complete -c rm -s d -d "Remove directories as well"
    complete -c rm -s P -d "Overwrite before removal"
    complete -c rm -s R -s r -d "Recursively remove subdirs"
    complete -c rm -s v -d "Explain what is done"
    test "$uname" = OpenBSD
    and exit 0
    complete -c rm -s W -d "Undelete given filenames"
    test "$uname" = Darwin
    and exit 0
    complete -c rm -s x -d "Don't traverse mount points"
    test "$uname" = NetBSD
    and exit 0
    complete -c rm -s I -d "Prompt to remove >=3 files"
end
