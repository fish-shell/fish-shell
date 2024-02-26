#
# Completions for the ls command and its aliases
#

# Shared ls switches
set -l uname (uname -s)
test "$uname" != SunOS
and complete -c ls -s S -d "Sort by size"

complete -c ls -s 1 -d "List one entry per line"
complete -c ls -s c -d "Sort by changed time, (-l) show ctime"
complete -c ls -s C -d "Force multi-column output"
complete -c ls -s f -d "Unsorted output, enables -a"
complete -c ls -s l -d "Long listing format"
complete -c ls -s m -d "Comma-separated format, fills across screen"
complete -c ls -s t -d "Sort by modified time, most recent first"
complete -c ls -s u -d "Sort by access time, (-l) show atime"
complete -c ls -s x -d "Multi-column output, horizontally listed"

# Test if we are using GNU ls
if ls --version >/dev/null 2>/dev/null
    complete -c ls -s a -l all -d "Show hidden"
    complete -c ls -s A -l almost-all -d "Show hidden except . and .."
    complete -c ls -s b -l escape -d "Octal escapes for non-graphic characters"
    complete -c ls -s d -l directory -d "List directories, not their content"
    complete -c ls -s F -l classify -d "Append filetype indicator (*/=>@|)"
    complete -c ls -s h -l human-readable -d "Human readable sizes"
    complete -c ls -s H -l dereference-command-line -d "Follow symlinks"
    complete -c ls -s i -l inode -d "Print inode number of files"
    complete -c ls -s k -d "Set blocksize to 1kB" # BSD ls -k enables blocksize *output*
    complete -c ls -s L -l dereference -d "Follow symlinks"
    complete -c ls -s n -l numeric-uid-gid -d "Long format, numeric UIDs and GIDs"
    complete -c ls -s p -l file-type -d "Append filetype indicator"
    complete -c ls -s q -l hide-control-chars -d "Replace non-graphic characters with '?'"
    complete -c ls -s r -l reverse -d "Reverse sort order"
    complete -c ls -s R -l recursive -d "List subdirectory recursively"
    complete -c ls -s s -l size -d "Print size of files"

    # GNU specific ls switches
    complete -c ls -l author -d "Print author"
    complete -c ls -s B -l ignore-backups -d "Ignore files ending with ~"
    complete -c ls -l block-size -x -d "Set block size"
    complete -c ls -l color -f -a "never always auto" -d "Use colors"
    complete -c ls -s D -l dired -d "Generate dired output"
    complete -c ls -l dereference-command-line-symlink-to-dir #-d "Follow directory symlinks from command line"
    complete -c ls -l format -x -a "across commas horizontal long single-column verbose vertical" -d "List format"
    complete -c ls -l full-time -d "Long format, full-iso time"
    complete -c ls -s G -l no-group -d "Don't print group information"
    complete -c ls -l group-directories-first -r -d "Group directories before files"
    complete -c ls -l hide -r -d "Do not list implied entries matching specified shell pattern"
    complete -c ls -s I -l ignore -r -d "Skip entries matching pattern"
    complete -c ls -l indicator-style -x -a "none classify file-type" -d "Append filetype indicator"
    complete -c ls -l lcontext -d "Display security context"
    complete -c ls -s N -l literal -d "Print raw entry names"
    complete -c ls -s o -d "Long format without groups"
    complete -c ls -s Q -l quote-name -d "Enclose entry in quotes"
    complete -c ls -l quoting-style -x -a "literal locale shell shell-always c escape" -d "Select quoting style"
    complete -c ls -l scontext -d "Display only security context and file name"
    complete -c ls -l si -d "Human readable sizes, powers of 1000"
    complete -c ls -l show-control-chars -d "Non-graphic characters printed as-is"
    complete -c ls -l sort -x -d "Sort criteria" -a "
			extension\t'Sort by file extension'
			none\tDon\'t\ sort
			size\t'Sort by size'
			time\t'Sort by modification time'
			version\t'Sort by version'
			status\t'Sort by file status modification time'
			atime\t'Sort by access time'
			access\t'Sort by access time'
			use\t'Sort by access time'
		"
    complete -c ls -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" -d "Assume tab stops at each COLS"
    complete -c ls -l time -x -d "Show time type" -a "
			time\t'Sort by modification time'
			access\t'Sort by access time'
			use\t'Sort by use time'
			ctime\t'Sort by file status modification time'
			status\t'Sort by status time'
		"
    complete -c ls -l time-style -x -a "full-iso long-iso iso locale" -d "Select time style"
    complete -c ls -s U -d "Do not sort"
    complete -c ls -s v -d "Sort by version"
    complete -c ls -s w -l width -x -d "Assume screen width"
    complete -c ls -s X -d "Sort by extension"
    complete -c ls -s Z -l context -d "Display security context so it fits on most displays"
    complete -c ls -l help -d "Display help and exit"
    complete -c ls -l version -d "Display version and exit"

else
    ####              ls on eunichs                  ####
    # From latest checked-in man pages as of Nov 2018.
    # Reformatted with Open Group's ordering and spacing,
    # then sorted by prevelance, consolidating option
    # matches.

    #              [         IEEE 1003.1-2017 options         ]  [   extension options  ]
    # freebsd: ls -[ikqrs][glno][Aa][Cmx1][Fp][LH][Rd][Sft][cu]  [hbTBWwPUG ZyI,        ] [-D format] [--color=when] [file ...]
    # netbsd:  ls -[ikqrs][glno][Aa][Cmx1][Fp][L ][Rd][Sft][cu]  [hbTBWwP       XMO     ] [file ...]
    # macOS:   ls -[ikqrs][glno][Aa][Cmx1][Fp][LH][Rd][Sft][cu]  [hbTBWwPUG       Oe@   ] [file ...]
    # openbsd: ls -[ikqrs][glno][Aa][Cmx1][Fp][LH][Rd][Sft][cu]  [  TB                  ] [file ...]
    # solaris: ls -[i qrs][glno][Aa][Cmx1][Fp][LH][Rd][ ft][cu]  [hb               e@EvV] [file ...]

    # netbsd ls -O: only leaf files, no dirs | macos ls -O: include file flags in -l output
    # so: don't complete -H for netbsd, and return early after the ls -P completion.
    #     But not before adding their -X, -M, -O options. Same kind of thing for the other OSes.

    ## IEEE 1003.1-2017 standard options:

    complete -c ls -s i -d "Show inode numbers for files"
    test "$uname" != SunOS
    and complete -c ls -s k -d "for -s: Display sizes in kB, not blocks" # GNU sets block size with -k
    complete -c ls -s q -d "Replace non-graphic characters with '?'"
    complete -c ls -s r -d "Reverse sort order"
    complete -c ls -s s -d "Show file sizes"

    complete -c ls -s g -d "Show group instead of owner in long format"
    complete -c ls -s n -d "Long format, numerical UIDs and GIDs"
    contains "$uname" FreeBSD NetBSD OpenBSD DragonFly
    and complete -c ls -s o -d "Long format, show file flags" # annoying BSD
    or complete -c ls -s o -d "Long format, omit group names" # annoying POSIX

    complete -c ls -s A -d "Show hidden except . and .."
    complete -c ls -s a -d "Show hidden entries"

    # -C in common, -m in common, -x in common, -1 in common

    complete -c ls -s F -d "Append indicators. dir/ exec* link@ socket= fifo| whiteout%"
    complete -c ls -s p -d "Append directory indicators"

    complete -c ls -s L -d "Follow all symlinks Cancels -P option"
    test "$uname" != NetBSD
    and complete -c ls -s H -d "Follow symlink given on commandline" # not present on netbsd

    complete -c ls -s R -d "Recursively list subdirectories"
    complete -c ls -s d -d "List directories, not their content"

    # -S in common, -f in common, -t in common
    # -c in common, -u in common
    ## These options are not standardized:
    if test "$uname" != OpenBSD
        complete -c ls -s h -d "Human-readable sizes"
        complete -c ls -s b -d "C escapes for non-graphic characters"
        if test "$uname" = SunOS
            complete -c ls -s e -d "Like -l, but fixed time format with seconds"
            complete -c ls -s @ -d "Like -l, but xattrs shown instead of ACLs"
            complete -c ls -s E -d "Like -l, but fixed time format with nanoseconds"
            complete -c ls -s v -d "Like -l, but verbose ACL information shown as well as -l output"
            complete -c ls -s V -d "Like -l, but compact ACL information printed after -l output"
            exit 0
        end
    else
        exit 0 # OpenBSD
    end
    complete -c ls -s T -d "for -l: Show complete date and time"
    complete -c ls -s B -d "Octal escapes for non-graphic characters"
    complete -c ls -s W -d "Display whiteouts when scanning directories"
    complete -c ls -s w -d "Force raw printing of non-printable characters"
    complete -c ls -s P -d "Don't follow symlinks"
    switch "$uname"
        case NetBSD
            complete -c ls -s X -d "Don't cross mount points when recursing"
            complete -c ls -s M -d "for -l, -s: Format size/count with commas"
            complete -c ls -s O -d "Show only leaf files (not dirs), eliding other output"
            exit 0
        case Darwin
            complete -c ls -s O -d "for -l: Show file flags"
            complete -c ls -s e -d "for -l: Print ACL associated with file, if present"
            complete -c ls -s @ -d "for -l: Display extended attributes"
        case FreeBSD
            complete -c ls -s Z -d "Display each file's MAC label"
            complete -c ls -s y -d "for -t: Sort A-Z output in same order as time output"
            complete -c ls -s I -d "Prevent -A from being automatically set for root"
            complete -c ls -s , -d "for -l: Format size/count number groups with ,/locale"
            complete -r -c ls -s D -d "for -l: Format date with strptime string"
            complete -c ls -l color -f -a "auto always never" -d "Enable color output"
    end
    complete -c ls -s U -d "Sort (-t) by creation time and show time (-l)"
    complete -c ls -s G -d "Enable colorized output" # macos, freebsd.
end
