function __fish_cdh_args
    set -l all_dirs $dirprev $dirnext
    set -l uniq_dirs

    # This next bit of code doesn't do anything useful at the moment since the fish pager always
    # sorts, and eliminates duplicate, entries. But we do this to mimic the modal behavor of `cdh`
    # and in hope that the fish pager behavior will be changed to preserve the order of entries.
    for dir in $all_dirs[-1..1]
        if not contains $dir $uniq_dirs
            set uniq_dirs $uniq_dirs $dir
        end
    end

    for dir in $uniq_dirs
        set -l home_dir (string match -r "$HOME(/.*|\$)" "$dir")
        if set -q home_dir[2]
            set dir "~$home_dir[2]"
        end
        echo $dir
    end
end

complete -c cdh -kxa '(__fish_cdh_args)'
