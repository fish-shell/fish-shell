# This function is typically bound to Alt-L, it is used to list the contents
# of the directory under the cursor.

function __fish_list_current_token -d "List contents of token under the cursor if it is a directory, otherwise list the contents of the current directory"
    set -l val "$(commandline -t | string replace -r '^~' "$HOME")"
    set -l cmd
    if test -d $val
        set cmd ls $val
    else
        set -l dir (dirname -- $val)
        if test $dir != . -a -d $dir
            set cmd ls $dir
        else
            set cmd ls
        end
    end
    __fish_echo $cmd
end
