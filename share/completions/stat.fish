if stat --version 2>/dev/null >/dev/null # GNU
    complete -c stat -s L -l dereference -d 'follow links'
    complete -c stat -s f -l file-system -d 'display file system status instead of file status'
    complete -c stat -s c -l format -x -d 'use the specified FORMAT, with implicit newline'
    complete -c stat -l printf -x -d 'use specified FORMAT, with backslash escapes and no implicit trailing newline'
    complete -c stat -s t -l terse -d 'print the information in terse form'
    complete -c stat -l help -d 'display this help and exit'
    complete -c stat -l version -d 'output version information and exit'
else # OS X
    complete -c stat -s F -d "Display content type symbols similar to ls(1)"
    complete -c stat -s f -d "Display information using specified FORMAT" -r
    complete -c stat -s L -d "Use stat(2) instead of lsstat(2)"
    complete -c stat -s l -d "Display output in ls -lT format"
    complete -c stat -s n -d "Don't force a newline to appear at end of each piece of output"
    complete -c stat -s q -d "Suppress failure messages"
    complete -c stat -s r -d "Display raw information"
    complete -c stat -s s -d "Display informationin ``shell output'' suitable for initialising variables"
    complete -c stat -s t -d "Display timestamps using specified FORMAT" -r
    complete -c stat -s x -d "Verbose information, similar to some Linux distributions"
end
