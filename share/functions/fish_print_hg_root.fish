function fish_print_hg_root
    # If hg isn't installed, there's nothing we can do
    if not command -sq hg
        return 1
    end

    # Find an hg directory above $PWD
    # without calling `hg root` because that's too slow
    set -l root
    set -l dir "$(pwd -P 2>/dev/null)"
    or return 1

    while not contains -- "$dir" "" / .
        if test -f $dir'/.hg/dirstate'
            echo $dir/.hg
            return 0
        end
        # Go up one directory
        set dir (path dirname -- $dir)
    end

    return 1
end
