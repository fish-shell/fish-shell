
function __fish_complete_tar -d "Peek inside of archives and list all files"

    set -l args (commandline -poc)
    while count $args >/dev/null
        switch $args[1]
            case '-*f' '--file'
                set -e args[1]
                if test -f $args[1]
                    set -l file_list (tar -atf $args[1] 2> /dev/null)
                    if test -n "$file_list"
                        printf (_ "%s\tArchived file\n") $file_list
                    end
                    return
                end
            case '*'
                set -e args[1]
                continue
        end
    end
end
