# localization: skip(private)
function __fish_make_cache_dir --description "Create and return XDG_CACHE_HOME"
    set -l xdg_cache_home $XDG_CACHE_HOME
    if test -z "$xdg_cache_home"; or string match -qv '/*' -- $xdg_cache_home; or set -q xdg_cache_home[2]
        set xdg_cache_home $HOME/.cache
    end

    # If we get an argument, we try to create that as a subdirectory.
    # So if you call `__fish_make_cache_dir completions`,
    # this creates e.g. ~/.cache/fish/completions
    if not path is -d $xdg_cache_home/fish/"$argv[1]"
        set -l mkdir_options -m 700

        # Can't set the permission in Cygwin on a `noacl` mount
        if __fish_uname | string match -qr "^(MSYS|CYGWIN)"
            # Find the first existing parent so we can `stat` it and get its mountpoint
            set -l existing_parent $xdg_cache_home/fish/"$argv[1]"
            while not path is -d $existing_parent
                set existing_parent (path dirname $existing_parent)
            end

            if mount | string match "*on $(stat -c %m -- $existing_parent) *" | string match -qr "[(,]noacl[),]"
                set mkdir_options
            end
        end

        mkdir $mkdir_options -p $xdg_cache_home/fish/"$argv[1]"
    end; and echo $xdg_cache_home/fish/"$argv[1]"
end
