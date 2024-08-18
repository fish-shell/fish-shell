function __fish_complete_freedesktop_icons -d 'List installed icon names according to `https://specifications.freedesktop.org/icon-theme-spec/0.13/`'
    # The latest `icon-theme-spec` as of 2024-08-10 is 0.13
    # https://specifications.freedesktop.org/icon-theme-spec/latest/
    # https://specifications.freedesktop.org/icon-theme-spec/0.13/#directory_layout
    # 1. $HOME/.icons
    # 2. $XDG_DATA_DIRS/icons
    # 3. /usr/share/pixmaps
    set -l dirs (path filter -rd -- ~/.icons $XDG_DATA_DIRS/icons /usr/share/pixmaps)

    # https://specifications.freedesktop.org/icon-theme-spec/0.13/#definitions
    # .png, .svg and .xpm are valid icon file formats
    path filter -rf -- $dirs/**/*.{png,svg,xpm} \
        | path basename \
        | path change-extension ''
end
