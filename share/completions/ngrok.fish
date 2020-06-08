# Commands
complete -c ngrok -f -a authtoken -d "Save authtoken to configuration file"
complete -c ngrok -f -a credits -d "Prints author and licensing information"
complete -c ngrok -f -a http -d "Start an HTTP tunnel"
complete -c ngrok -f -a start -d "Start tunnels by name from the configuration file"
complete -c ngrok -f -a tcp -d "Start a TCP tunnel"
complete -c ngrok -f -a tls -d "Start a TLS tunnel"
complete -c ngrok -f -a update -d "Update ngrok to the latest version"
complete -c ngrok -f -a version -d "Print the version string"
complete -c ngrok -f -a help -d "Shows a list of commands or help for one command"

# General Options
complete -c ngrok -l help -e -f
complete -c ngrok -l authtoken -r -d "ngrok.com authtoken identifying a user"
complete -c ngrok -l config -r -d "path to config files; they are merged if multiple"
complete -c ngrok -l log -x -a "false stderr stdout" -d "path to log file, 'stdout', 'stderr' or 'false'"
complete -c ngrok -l log-format -x -a "term logfmt json" -d "log record format: 'term', 'logfmt', 'json'"
complete -c ngrok -l log-level -r -a info -d "logging level"
complete -c ngrok -l region -x -a "us eu au ap" -d "ngrok server region [us , eu, au, ap] (default: us)"

# http & tls's options
complete -c ngrok -l hostname -r -d "host tunnel on custom hostname (requires DNS CNAME)"
complete -c ngrok -l subdomain -r -d "host tunnel on a custom subdomain"

# http's optons
complete -c ngrok -l auth -r -d "enforce basic auth on tunnel endpoint, 'user:password'"
complete -c ngrok -l bind-tls -x -a "both https http" -d "listen for http, https or both: true/false/both"
complete -c ngrok -l host-header -r -d "set Host header; if 'rewrite' use local address hostname"
complete -c ngrok -l inspect -d "enable/disable http introspection"

# tls's options
complete -c ngrok -l client-cas -r -d "path to TLS certificate authority to verify client certs"
complete -c ngrok -l crt -r -d "path to a TLS certificate for TLS termination"
complete -c ngrok -l key -r -d "path to a TLS key for TLS termination"

# start's options
complete -c ngrok -l all -d "start all tunnels in the configuration file"
complete -c ngrok -l none -d "start running no tunnels"

# tcp's options
complete -c ngrok -l remote-addr -r -d "bind remote address (requires you reserve an address)"

# update's options
complete -c ngrok -l channel -x -a "stable beta" -d "update channel (stable, beta)"
