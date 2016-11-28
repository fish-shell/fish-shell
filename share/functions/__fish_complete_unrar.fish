
function __fish_complete_unrar -d "Peek inside of archives and list all files"

    set -l cmd (commandline -poc)
    set -e cmd[1]
    for i in $cmd
        switch $i
            case '-*'
                continue

            case '*.rar'
                if test -f $i
                    set -l file_list (unrar vb $i)
                    printf (_ "%s\tArchived file\n") $file_list
                end
                return
        end
    end
end


