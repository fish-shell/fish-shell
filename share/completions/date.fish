complete -c date -f -d "display or set date and time"
if date --version >/dev/null 2>/dev/null
    complete -c date -s d -l date -d "Display date described by string" -x
    complete -c date -s f -l file -d "Display date for each line in file" -r
    complete -c date -s I -l iso-8601 -d "Output in ISO-8601 format" -x -a "date hours minutes seconds"
    complete -c date -s s -l set -d "Set time" -x
    complete -c date -s R -l rfc-2822 -d "Output RFC-2822 date string"
    complete -c date -s r -l reference -d "Display last modification time of file" -r
    complete -c date -s u -l utc -d "Print/set UTC time" -f
    complete -c date -l universal -d "Print/set UTC time" -f
    complete -c date -s h -l help -d "Display help and exit" -f
    complete -c date -s v -l version -d "Display version and exit" -f
else
    complete -c date -s u -d 'Display or set UTC time' -f
    complete -c date -s j -d "Don't actually set the clock" -f
    complete -c date -s d -d "Set system's value for DST" -x

    set -l uname (uname -s)

    test "$uname" != OpenBSD
    and complete -c date -s n -d 'Set clock for local machine only' -f

    switch $uname
        case FreeBSD Darwin DragonFly
            # only -u is actually POSIX. Rest are BSD extensions:
            complete -c date -s r -d "Show file mtime, or format seconds" -r
            complete -c date -s v -d 'Adjust clock +/- by time specified' -f
        case NetBSD OpenBSD
            complete -c date -s a -d "Change clock slowly with adjtime" -x
            complete -c date -s r -d "Show date given seconds since epoch" -x
            if test "$uname" = NetBSD
                complete -c date -s d -d "Parse human-described date-time and show result" -x
                exit
            end
            complete -c date -s z -d "Specify timezone for output" -x
    end

    complete -c date -s t -d "Set system's minutes west of GMT" -x
    complete -c date -s f -d 'Use format string to parse date' -f
end

