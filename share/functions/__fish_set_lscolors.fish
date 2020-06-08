function __fish_set_lscolors --description 'Set $LS_COLORS if possible'
    if ! set -qx LS_COLORS && set -l cmd (command -s {g,}dircolors)[1]
        set -l colorfile
        for file in ~/.dir_colors ~/.dircolors /etc/DIR_COLORS
            if test -f $file
                set colorfile $file
                break
            end
        end
        # Here we rely on the legacy behavior of `dircolors -c` producing output
        # suitable for csh in order to extract just the data we're interested in.
        set -gx LS_COLORS ($cmd -c $colorfile | string split ' ')[3]
        # The value should always be quoted but be conservative and check first.
        if string match -qr '^([\'"]).*\1$' -- $LS_COLORS
            set LS_COLORS (string match -r '^.(.*).$' $LS_COLORS)[2]
        end
    end
end
