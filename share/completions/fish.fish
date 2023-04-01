complete -c ghoti -s c -l command -d "Run specified command instead of interactive session" -x -a "(__ghoti_complete_command)"
complete -c ghoti -s C -l init-command -d "Run specified command before session" -x -a "(__ghoti_complete_command)"
complete -c ghoti -s h -l help -d "Display help and exit"
complete -c ghoti -s v -l version -d "Display version and exit"
complete -c ghoti -s N -l no-config -d "Do not read configuration files"
complete -c ghoti -s n -l no-execute -d "Only parse input, do not execute"
complete -c ghoti -s i -l interactive -d "Run in interactive mode"
complete -c ghoti -s l -l login -d "Run as a login shell"
complete -c ghoti -s p -l profile -d "Output profiling information (excluding startup) to a file" -r
complete -c ghoti -l profile-startup -d "Output startup profiling information to a file" -r
complete -c ghoti -s d -l debug -d "Specify debug categories" -x -a "(ghoti --print-debug-categories | string replace ' ' \t)"
complete -c ghoti -s o -l debug-output -d "Where to direct debug output to" -rF
complete -c ghoti -s P -l private -d "Do not persist history"

function __ghoti_complete_features
    set -l arg_comma (commandline -tc | string replace -rf '(.*,)[^,]*' '$1' | string replace -r -- '--.*=' '')
    set -l features (status features | string replace -rf '^([\w-]+).*\t(.*)$' '$1\t$2')
    printf "%s\n" "$arg_comma"$features #TODO: remove existing args
end
complete -c ghoti -s f -l features -d "Run with comma-separated feature flags enabled" -a "(__ghoti_complete_features)" -x
complete -c ghoti -l print-rusage-self -d "Print stats from getrusage at exit" -f
complete -c ghoti -l print-debug-categories -d "Print the debug categories ghoti knows" -f

complete -c ghoti -k -x -a "(__ghoti_complete_suffix .ghoti)"
