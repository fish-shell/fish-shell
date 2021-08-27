function __fish_exif_target_file_tags
    for target in (string match -v -- '-*' (commandline -po)[2..])
        string replace -f '*' '' (exif --list-tags "$target" 2> /dev/null)[2..] | string replace -r '(\s+-){4}' '' | string split -m1 ' ' | string trim
    end
end

function __fish_exif_potential_targets
    set -l matching_files (commandline -t)*
    for file in $matching_files
        if test -d $file
            echo $file/
        else if exif $file &>/dev/null
            echo $file
        end
    end
end

function __fish_exif_token_begins_with_arg
    not string match -- '-*' (commandline -t)
end

complete -c exif -f -a "(__fish_exif_potential_targets)" -n __fish_exif_token_begins_with_arg

for line in (exif --help)
    set -l parts
    set -l short
    if string match -r '^\s+-[\w\|\?],\s--(\w|-|=)+\s+.*$' "$line"
        set parts (string split ', ' -- (string trim "$line"))
        set short (string replace -r '^-' '' -- "$parts[1]")
        set parts (string split -nm 1 ' ' -- "$parts[2]")
    else if string match -r '^\s+--(\w|-|=)+\s+.*$' "$line"
        set parts (string split -nm 1 ' ' -- (string trim "$line"))
    else
        continue
    end

    set -l long (string replace -r '^--' '' -- "$parts[1]")
    set -l description (string trim "$parts[2]")

    if string match -r '[\w-]+=[\w]' "$long"
        set sub_parts (string split -nm 1 '=' "$long")
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
