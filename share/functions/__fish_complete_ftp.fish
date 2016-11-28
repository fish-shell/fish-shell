function __fish_complete_ftp -d 'Complete ftp, pftp' --argument-names ftp
    complete -c $ftp -xa "(__fish_print_hostnames)" -d 'Hostname'
    complete -c $ftp -s 4 -d 'Use only IPv4 to contact any host'
    complete -c $ftp -s 6 -d 'Use IPv6 only'
    complete -c $ftp -s p -d 'Use passive mode for data transfers'
    complete -c $ftp -s A -d 'Use active mode for data transfers'
    complete -c $ftp -s i -d 'Turn off interactive prompting during multiple file transfers.'
    complete -c $ftp -s n -d 'Restrain ftp from attempting "auto-login" upon initial connection'
    complete -c $ftp -s e -d 'Disable command editing and history support'
    complete -c $ftp -s g -d 'Disable file name globbing'
    complete -c $ftp -s m -d 'Do not explicitly bind data and control channels to same interface'
    complete -c $ftp -s v -d 'Verbose. Show all server responses and data transfer stats'
    complete -c $ftp -s d -d 'Enable debugging'
end
