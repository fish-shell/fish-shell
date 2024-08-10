function __fish_complete_freedesktop_icons -d 'List installed icon names according to `https://specifications.freedesktop.org/icon-theme-spec/0.13/`'
    # The latest `icon-theme-spec` as of 2024-08-10 is 0.13
    # https://specifications.freedesktop.org/icon-theme-spec/latest/
    # https://specifications.freedesktop.org/icon-theme-spec/0.13/

    # https://specifications.freedesktop.org/icon-theme-spec/0.13/#directory_layout
    # 1. $HOME/.icons
    # 2. $XDG_DATA_DIRS/icons
    # 3. /usr/share/pixmaps
    set -l dirs
    test -d $HOME/.icons; and set -a dirs $HOME/.icons
    if set -q XDG_DATA_DIRS
        # Can be a list of directories so we append each
        for d in $XDG_DATA_DIRS
            test -d $d/icons; and set -a dirs $d/icons
        end
    end
    test -d /usr/share/pixmaps; and set -a dirs /usr/share/pixmaps

    for d in $dirs
        # https://specifications.freedesktop.org/icon-theme-spec/0.13/#definitions
        # .png, .svg and .xpm are valid icon file formats
        printf '%s\n' $d/**/*.{png,svg,xpm}
    end \
        | path basename \
        | path change-extension '' \
        | command sort --unique
end
