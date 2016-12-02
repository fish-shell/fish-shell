# Tab completion for docker (https://github.com/docker/docker).
# Version: 1.13.0-rc2

complete -e -c docker

for line in (docker --help | \
             string match -r '^\s+\w+\s+[^\n]+' | \
             string trim)
  set -l doc (echo $line | string split -m 1 ' ')
  complete -c docker -n '__fish_use_subcommand' -xa $doc[1] --description $doc[2]
end

complete -c docker -l config -r                -d 'Location of client config file'
complete -c docker -s D -l debug               -d 'Enable debug mode'
complete -c docker -s H -l host -a list        -d 'Daemon socket(s) to connect to'
complete -c docker -l tls                      -d 'Use TLS; implied by --tlsverify'
complete -c docker -l tlscacert -r             -d 'Trust certs signed only by this CA'
complete -c docker -l tlscert   -r             -d 'Path to TLS certificate file'
complete -c docker -l tlskey    -r             -d 'Path to TLS key file'
complete -c docker -l tlsverify                -d 'Use TLS and verify the remote'
complete -c docker -s v -l version          -d 'Print version information and quit'
complete -c docker -l help                  -d 'Print usage'
complete -c docker -l log-level -a 'debug info warn error fatal' -x -d 'Set the logging level'
