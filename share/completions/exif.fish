function __fish_exif_target_file_tags
    for target in (string match -v -- '-*' (commandline -px)[2..])
        string replace -f '*' '' (exif --list-tags "$target" 2> /dev/null)[2..] | string replace -r '(\s+-){4}' '' | string split -m1 ' ' | string trim
    end
end

function __fish_exif_potential_targets
    set -l token (commandline -t)
    set -l matching_files (complete -C "__fish_command_without_completions $token")
    for file in $matching_files
        if test -d "$file"
            echo $file
        else if exif "$file" &>/dev/null
            echo $file
        else if not test -e "$file"
            # Handle filenames containing $.
            if exif $file &>/dev/null
                echo $file
            end
        end
    end
end

function __fish_exif_token_begins_with_arg
    not string match -- '-*' (commandline -t)
end

complete -c exif -f -a "(__fish_exif_potential_targets)" -n __fish_exif_token_begins_with_arg

for line in (exif --help)
    set -l short
    set -l long
    set -l description
    if set -l matches (string match -r '^\s+-([\w?]),\s--([\w=-]+)\s+(.*)$' "$line")
        set short $matches[2]
        set long $matches[3]
        set description $matches[4]
    else if set -l matches (string match -r '^\s+--([\w=-]+)\s+(.*)$' "$line")
        set long $matches[2]
        set description $matches[3]
    else
        continue
    end

    if set -l sub_parts (string split -n --max 1 '=' $long)
        set long "$sub_parts[1]"
        switch (string lower "$sub_parts[2]")
            case tag
                complete -c exif -s$short -l $long -d "$description" -x -a "(__fish_exif_target_file_tags)"
            case ifd
                complete -c exif -s$short -l $long -d "$description" -x -a "0 1 EXIF GPS Interoperability"
            case file
                complete -c exif -s$short -l $long -d "$description" -F
            case "*"
                complete -c exif -s$short -l $long -d "$description" -x
        end
    else
        complete -c exif -s$short -l $long -d "$description"
    end
end
