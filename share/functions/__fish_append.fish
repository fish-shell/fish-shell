function __fish_append -d "Internal completion function for appending string to the commandline" --argument separator
    set -e argv[1]
    set str (commandline -tc| sed -ne "s/\(.*$separator\)[^$separator]*/\1/p"|sed -e "s/--.*=//")
    printf "%s\n" "$str"$argv "$str"(printf "%s\n" $argv|sed -e "s/\(\t\|\$\)/,\1/")
end


