function __fish_append -d "Internal completion function for appending string to the commandline" --argument-names sep
    set -e argv[1]
    set -l str (commandline -tc | string replace -rf "(.*$sep)[^$sep]*" '$1' | string replace -r -- '--.*=' '')
    printf "%s\n" "$str"$argv
end
