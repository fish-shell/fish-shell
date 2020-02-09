function __fish_append -d "Internal completion function for appending string to the commandline" --argument sep
    set -e argv[1]
    set str (commandline -tc | string replace -rf "(.*$sep)[^$sep]*" '$1' | string replace -r -- '--.*=' '')
    printf "%s\n" "$str"$argv
end


