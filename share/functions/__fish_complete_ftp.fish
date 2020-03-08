function __fish_complete_ftp -d 'Complete ftp, pftp' --argument-names ftp
    # Common across all ftp implementations
    complete -c $ftp -xa "(__fish_print_hostnames)" -d Hostname
    complete -c $ftp -s 4 -d 'Use IPv4 only'
    complete -c $ftp -s 6 -d 'Use IPv6 only'
    complete -c $ftp -s A -d 'Use active mode for data transfers'
    complete -c $ftp -s d -d 'Enable debugging'
    complete -c $ftp -s e -d 'Disable command editing and history support'
    complete -c $ftp -s g -d 'Disable file name globbing'
    complete -c $ftp -s i -d 'Disable interactive prompting during multiple file transfers.'
    complete -c $ftp -s p -d 'Use passive mode for data transfers'
    complete -c $ftp -s t -d 'Enable packet tracing'
    complete -c $ftp -s n -d 'Disable autologin'

    switch (uname -s)
        case FreeBSD NetBSD OpenBSD
            complete -c $ftp -s o -d "Set output for auto-fetched files"
            complete -c $ftp -s a -d "Anonymous login"
            complete -c $ftp -s P -d 'Set port number' -x
            complete -c $ftp -s r -d 'Time between retry attempts' -x
            complete -c $ftp -s s -d 'Local address to bind to' -xa "(__fish_print_addresses)"
    end

    switch (uname -s)
        case FreeBSD NetBSD Linux
            complete -c $ftp -s N -d "Specify netrc file" -r
            complete -c $ftp -s v -d 'Enable verbose and progress meter'
    end

    switch (uname -s)
        case FreeBSD NetBSD
            complete -c $ftp -s f -d "Force cache reload"
            complete -c $ftp -s q -d "Seconds to wait before quitting stalled connection" -x
            complete -c $ftp -s R -d 'Restart all non-proxied auto-fetches'
            complete -c $ftp -s T -d 'Set max transfer rate' -x
            complete -c $ftp -s u -d 'Upload file to URL' -r
            complete -c $ftp -s V -d 'Disable verbose and progress meter'
    end

    switch (uname -s)
        case NetBSD
            complete -c $ftp -s x -d "Set buffer size" -x
        case OpenBSD
            complete -c $ftp -s N -d "Set command name for error reports" -x
            complete -c $ftp -s V -d "Disable verbose"
            complete -c $ftp -s v -d "Enable verbose"
            complete -c $ftp -s M -d "Disable progress meter"
            complete -c $ftp -s m -d "Enable progress meter"
            complete -c $ftp -s C -d "Continue a previously interrupted file transfer"
            complete -c $ftp -s c -d "Netscape-like cookiejar file" -r
            complete -c $ftp -s D -d "Short title for start of progress meter" -x
            complete -c $ftp -s E -d "Disable EPSC/EPRT command on IPv4 connections"
            complete -c $ftp -s k -d "Keepalive interval in seconds" -x
            complete -c $ftp -s S -d "SSL/TLS options"
            complete -c $ftp -s U -d "Set useragent" -x
            complete -c $ftp -s w -d "Time to wait before aborting slow connections" -x
        case Linux
            complete -c $ftp -s V -d "Print version"
            complete -c $ftp -l prompt -d "Print a prompt even if not on a tty"
            complete -c $ftp -s '?' -l help -d "Print help"
    end
end
