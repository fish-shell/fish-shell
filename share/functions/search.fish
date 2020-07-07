function search --description "Search for text in any file within a directory"
    if test (count $argv) -ne 2
        printf (_ "%s: requires exactly two arguments (search match dir)\n") search
        return 1
    end
    if not test -d "$argv[2]"
        printf (_ "%s: directory % does not exist\n") search $argv[2]
        return 1
    end

    cd "$argv[2]"
    set pwd $pwd "$argv[2]"
    for file in (ls -p | grep -v /)
        set test (printf "%s" (cat "$file" | grep "$argv[1]"))
        if test -n "$test"
            echo ./$pwd$file
        end
    end

    set -e pwd[(count $pwd)]
    for dir in (ls -p | grep /)
        search "$argv[1]" "$dir"
    end

    cd ..
    return 0
end
