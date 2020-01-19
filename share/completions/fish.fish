complete -c fish -s c -l command -d "Run specified command instead of interactive session" -x -a "(__fish_complete_command)"
complete -c fish -s C -l init-command -d "Run specified command before session" -x -a "(__fish_complete_command)"
complete -c fish -s h -l help -d "Display help and exit"
complete -c fish -s v -l version -d "Display version and exit"
complete -c fish -s n -l no-execute -d "Only parse input, do not execute"
complete -c fish -s i -l interactive -d "Run in interactive mode"
complete -c fish -s l -l login -d "Run as a login shell"
complete -c fish -s p -l profile -d "Output profiling information to specified file" -r
complete -c fish -s d -l debug -d "Specify verbosity level" -x -a "0\t'Warnings silenced'
1\t'Default'
2\t'Basic debug output'
3\t'More debug output'
4\t'Much more debug output'
5\t'Too much debug output'
(fish --print-debug-categories | string replace ' ' \t)"
complete -c fish -s D -l debug-stack-frames -d "Show specified # of frames with debug output" -x -a "(seq 128)\t\n"
complete -c fish -s P -l private -d "Do not persist history"

function __fish_complete_features
    set -l arg_comma (commandline -tc | string replace -rf '(.*,)[^,]*' '$1' | string replace -r -- '--.*=' '')
    set -l features (status features | string replace -rf '^([\w-]+).*\t(.*)$' '$1\t$2')
    printf "%s\n" "$arg_comma"$features #TODO: remove existing args
end
complete -c fish -s f -l features -d "Run with comma-separated feature flags enabled" -a "(__fish_complete_features)" -x
complete -c fish -l print-rusage-self -d "Print stats from getrusage at exit" -f
complete -c fish -l print-debug-categories -d "Print the debug categories fish knows" -f

complete -c fish -x -a "(__fish_complete_suffix .fish)"
