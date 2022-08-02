# Provide a menu of the directories recently navigated to and ask the user to
# choose one to make the new current working directory (cwd).

function cdh --description "Menu based cd command"
    # See if we've been invoked with an argument. Presumably from the `cdh` completion script.
    # If we have just treat it as `cd` to the specified directory.
    if set -q argv[1]
        cd $argv
        return
    end

    if set -q argv[2]
        echo (_ "cdh: Expected zero or one arguments") >&2
        return 1
    end

    set -l all_dirs $dirprev $dirnext
    if not set -q all_dirs[1]
        echo (_ 'No previous directories to select. You have to cd at least once.') >&2
        return 0
    end

    # Reverse the directories so the most recently visited is first in the list.
    # Also, eliminate duplicates; i.e., we only want the most recent visit to a
    # given directory in the selection list.
    set -l uniq_dirs
    for dir in $all_dirs[-1..1]
        if not contains $dir $uniq_dirs
            set -a uniq_dirs $dir
        end
    end

    set -l letters a b c d e f g h i j k l m n o p q r s t u v w x y z
    set -l dirc (count $uniq_dirs)
    if test $dirc -gt (count $letters)
        set -l msg (_ 'This should not happen. Have you changed the cd function?')
        printf "$msg\n" >&2
        set -l msg (_ 'There are %s unique dirs in your history but I can only handle %s')
        printf "$msg\n" $dirc (count $letters) >&2
        return 1
    end

    # Print the recent directories, oldest to newest. Since we previously
    # reversed the list, making the newest entry the first item in the array,
    # we count down rather than up.
    for i in (seq $dirc -1 1)
        set -l dir $uniq_dirs[$i]
        set -l label_color normal
        set -q fish_color_cwd
        and set label_color $fish_color_cwd
        set -l dir_color_reset (set_color normal)
        set -l dir_color
        if test "$dir" = "$PWD"
            set dir_color (set_color $fish_color_history_current)
        end

        set -l home_dir (string match -r "^$HOME(/.*|\$)" "$dir")
        if set -q home_dir[2]
            set dir "~$home_dir[2]"
        end
        printf '%s %s %2d) %s %s%s%s\n' (set_color $label_color) $letters[$i] $i (set_color normal) $dir_color $dir $dir_color_reset
    end

    # Ask the user which directory from their history they want to cd to.
    set -l msg (_ 'Select directory by letter or number: ')
    read -l -p "echo '$msg'" choice
    if test -z "$choice"
        return 0
    else if string match -q -r '^[a-z]$' $choice
        # Convert the letter to an index number.
        set choice (contains -i $choice $letters)
    end

    set -l msg (_ 'Error: expected a number between 1 and %d or letter in that range, got "%s"')
    if string match -q -r '^\d+$' $choice
        if test $choice -ge 1 -a $choice -le $dirc
            cd $uniq_dirs[$choice]
            return
        else
            printf "$msg\n" $dirc $choice >&2
            return 1
        end
    else
        printf "$msg\n" $dirc $choice >&2
        return 1
    end
end
