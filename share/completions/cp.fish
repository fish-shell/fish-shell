if cp --version 2>/dev/null >/dev/null # GNU cp
    complete -c cp -s a -l archive -d "Same as -dpR"
    complete -c cp -l attributes-only -d "Copy just the attributes"
    complete -c cp -s b -l backup -d "Make backup of each existing destination file" -a "none off numbered t existing nil simple never"
    complete -c cp -l copy-contents -d "Copy contents of special files when recursive"
    complete -c cp -s d -d "Same as --no-dereference --preserve=link"
    complete -c cp -s f -l force -d "Do not prompt before overwriting"
    complete -c cp -s i -l interactive -d "Prompt before overwrite"
    complete -c cp -s H -d "Follow command-line symbolic links"
    complete -c cp -s l -l link -d "Link files instead of copying"
    complete -c cp -l strip-trailing-slashes -d "Remove trailing slashes from source"
    complete -c cp -s S -l suffix -r -d "Backup suffix"
    complete -c cp -s t -l target-directory -d "Target directory" -x -a "(__fish_complete_directories (commandline -ct) 'Target directory')"
    complete -c cp -s u -l update -d "Do not overwrite newer files"
    complete -c cp -s v -l verbose -d "Verbose mode"
    complete -c cp -l help -d "Display help and exit"
    complete -c cp -l version -d "Display version and exit"
    complete -c cp -s L -l dereference -d "Always follow symbolic links"
    complete -c cp -s n -l no-clobber -d "Do not overwrite an existing file"
    complete -c cp -s P -l no-dereference -d "Never follow symbolic links"
    complete -c cp -s p -d "Same as --preserve=mode,ownership,timestamps"
    complete -c cp -f -l preserve -d "Preserve ATTRIBUTES if possible" -xa "mode ownership timestamps links all"
    complete -c cp -f -l no-preserve -r -d "Don't preserve ATTRIBUTES" -xa "mode ownership timestamps links all"
    complete -c cp -l parents -d "Use full source file name under DIRECTORY"
    complete -c cp -s r -s R -l recursive -d "Copy directories recursively"
    complete -c cp -l reflink -d "Control clone/CoW copies" -a "always auto never"
    complete -c cp -l remove-destination -d "First remove existing destination files"
    complete -c cp -f -l sparse -r -d "Control creation of sparse files" -xa "always auto never"
    complete -c cp -s s -l symbolic-link -d "Make symbolic links instead of copying"
    complete -c cp -s T -l no-target-directory -d "Treat DEST as a normal file"
    complete -c cp -s x -l one-file-system -d "Stay on this file system"
    complete -c cp -s Z -d "Set SELinux context of copy to default type"
    complete -c cp -s X -l context -r -d "Set SELinux context of copy to CONTEXT"
else # BSD/macOS
    set -l uname (uname -s)
    # Solaris:   cp [-R | r [H | L | P ]] [-fi ] [-p        ]
    # openbsd:   cp	[-R |   [H | L | P ]] [-fi ] [-pv       ]
    # macos:     cp [-R |   [H | L | P ]] [-fin] [-pvalxscX ] # -c: clone -X: copy xattrs
    # netbsd:    cp [-R |   [H | L | P ]] [-fi ] [-pval    N] # -l: hard link instead of copy -N: don't copy file flags
    # dragonfly: cp [-R |   [H | L | P ]] [-fin] [-pvalx    ] # -x: don't traverse mount points
    # freebsd:   cp	[-R |   [H | L | P ]] [-fin] [-pvalxs   ] # -s: symlink instead of copy
    if test "$uname" = SunOS # annoying
        complete -c cp -s r -d "Copy directories recursively"
        complete -c cp -s R -d "Like -r, but replicating pipes instead of reading pipes"
    else
        complete -c cp -s R -d "Copy directories recursively"
    end
    complete -c cp -s H -d "-R: Follow symlink arguments"
    complete -c cp -s L -d "-R: Follow all symlinks"
    complete -c cp -s P -d "-R: Don't follow symlinks (default)"

    complete -c cp -s f -d "Don't confirm to overwrite"
    complete -c cp -s i -d "Prompt before overwrite"
    not contains "$uname" SunOS OpenBSD NetBSD
    and complete -c cp -s n -d "Don't overwrite existing"

    complete -c cp -s p -d "Preserve attributes of source"
    if test "$uname" = SunOS
        exit 0
    end
    complete -c cp -s v -d "Print filenames as they're copied"
    if test "$uname" = OpenBSD
        exit 0
    end
    complete -c cp -s a -d "Archive mode (-pPR)"
    if test "$uname" = Darwin
        complete -c cp -s c -d "Clone using clonefile(2)"
        complete -c cp -s X -d "Omit xattrs, resource forks"
    end
    complete -c cp -s l -d "Hard link instead of copying"
    if test "$uname" = NetBSD
        complete -c cp -s N -d "Don't copy file flags"
        exit 0
    end
    complete -c cp -s x -d "Don't traverse mount points"
    if test "$uname" = FreeBSD -o "$uname" = Darwin
        complete -c cp -s s -d "Symlink instead of copying"
    end
end
