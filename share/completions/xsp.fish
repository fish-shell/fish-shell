complete -c xsp -l help -d 'Show help'
complete -c xsp -l version -d 'Show version'

complete -c xsp -l address -x -d 'Use the IP address to listen on'
complete -c xsp -l port -x -d 'Use the port where the XSP server will listen to requests'
complete -c xsp -l backlog -x -d 'Use the backlog of connections to set on the listener socket'
complete -c xsp -l minThreads -x -d 'Specify minimum number of threads the threadpool allocates'
complete -c xsp -l filename -r -d 'Use unix socket file name to listen on'
complete -c xsp -l root -r -d 'Use root directory for XSP'
complete -c xsp -l appconfigfile -r -d 'Add application definitions from the XML configuration file'
complete -c xsp -l appconfigdir -r \
    -d 'Add application definitions from all XML files found in the specified directory'
complete -c xsp -l applications -r \
    -d 'Use a comma separated list of virtual directory and real directory for all the applications'
complete -c xsp -l master -d 'Use this instance by mod_mono to create ASP.NET applications on demand'
complete -c xsp -l no-hidden -d 'Do not protect hidden files/directories from being accessed by clients'
complete -c xsp -l https -d 'Enable HTTPS support on the server'
complete -c xsp -l https-client-accept -d 'Like --https enable HTTPS support on the server'
complete -c xsp -l https-client-require -d 'Like --https enable HTTPS support on the server'
complete -c xsp -l p12file -r -d 'Use to specify the PKCS#12 file to use'
complete -c xsp -l cert -r -d 'Use to specify the server X.509 certificate file'
complete -c xsp -l pkpwd -r -d 'Use password to decode the private key'

complete -c xsp -l protocol \
    -a 'Default\t"Auto-detect the client protocol and adjust the server protocol accordingly" Tls\t"Tls protocol" Ssl3\t"Ssl3 protocol"' \
    -x -d 'Use protocols available for encrypting the communications'

complete -c xsp -l terminate -d 'Gracefully terminates a running mod-mono-server instance'
complete -c xsp -l verbose -d 'Show more messages'
complete -c xsp -l pidfile -r -d 'Redirect xsp4 PID output in stdout to file'
