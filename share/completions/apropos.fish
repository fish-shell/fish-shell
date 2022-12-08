function __fish_complete_apropos
    set -f str (commandline -ct | string escape --style=regex)
    switch "$str"
        case '-**'

        case '*'
            __fish_apropos "^$str" 2>/dev/null | string replace -rf -- '^([^(\s]+) ?\([,\w]+\)\s+-?\s?(.*)$' '$1\t$2'
    end
end

complete -xc apropos -a '(__fish_complete_apropos)' -d "whatis entry"

if apropos --version &>/dev/null
    complete -f -c apropos -s '?' -l help -d "Display help and exit"
    complete -f -c apropos -l usage -d "Display short usage message"
    complete -f -c apropos -s d -l debug -d "Print debugging info"
    complete -f -c apropos -s v -l verbose -d "Verbose mode"
    complete -f -c apropos -s r -l regex -d "Keyword as regex (default)"
    complete -f -c apropos -s w -l wildcard -d "Keyword as wildcards"
    complete -f -c apropos -s e -l exact -d "Keyword as exactly match"
    complete -x -c apropos -s m -l system -d "Search for other system"
    complete -x -c apropos -s M -l manpath -a "(__fish_complete_directories (commandline -ct))" -d Manpath
    complete -r -c apropos -s C -l config-file -d "Specify a configuration file"
    complete -f -c apropos -s V -l version -d "Display version and exit"
    complete -f -c apropos -s a -l and -d "Match all keywords"
    complete -f -c apropos -s l -l long -d "Do not trim output to terminal width"
    complete -x -c apropos -s s -l sections -l section -d "Search only these sections (colon-separated)"
    complete -x -c apropos -s L -l locale -a "(command -sq locale; and locale -a)" -d "Set locale"
else
    set -l uname (uname -s)
    complete apropos -s s -d 'specify manual section' -x # common option

    contains $uname Darwin
    and complete apropos -s d -d "print debug info" -f

    if contains $uname FreeBSD DragonFlyBSD OpenBSD NetBSD
        complete apropos -s C -d "specify man config file" -r
        complete apropos -s S -d "search for given arch" -f
    end

    if contains $uname FreeBSD DragonFlyBSD OpenBSD
        complete apropos -s a -d "print complete manual pages" -f
        complete apropos -s m -d "prepend extra paths to find db" -r
        complete apropos -s M -d "specify paths to search for db" -r
        complete apropos -s k -d "full expression syntax (default)" -f
        complete apropos -s f -d "only match whole words, in titles" -f
        complete apropos -s O -d "show values associated given key" -x
    else if contains $uname NetBSD
        complete apropos -s1 -d "search section 1" -f
        complete apropos -s2 -d "search section 2" -f
        complete apropos -s3 -d "search section 3" -f
        complete apropos -s4 -d "search section 4" -f
        complete apropos -s5 -d "search section 5" -f
        complete apropos -s6 -d "search section 6" -f
        complete apropos -s7 -d "search section 7" -f
        complete apropos -s8 -d "search section 8" -f
        complete apropos -s9 -d "search section 9" -f
        complete apropos -s h -d "HTML formatting" -f
        complete apropos -s i -d "terminal escape formatting" -f
        complete apropos -s s -d "disable context, formatting" -f
        complete apropos -s m -d "show match context (default)" -f
        complete apropos -s M -d "do not show match context" -f
        complete apropos -s n -d "limit number of results" -f
        complete apropos -s P -d "format for pager" -f
        complete apropos -s p -d "format & pipe through pager" -f
        complete apropos -s r -d "disable formatting" -f
    end
end
