function __fish_make_cache_dir --description "Create and return XDG_CACHE_HOME"
    set -l xdg_cache_home $XDG_CACHE_HOME
    if test -z "$xdg_cache_home"; or string match -qv '/*' -- $xdg_cache_home; or set -q xdg_cache_home[2]
        set xdg_cache_home $HOME/.cache
    end

    # If we get an argument, we try to create that as a subdirectory.
    # So if you call `__fish_make_cache_dir completions`,
    # this creates e.g. ~/.cache/fish/completions
    if not path is -d $xdg_cache_home/fish/"$argv[1]"
        mkdir -m 700 -p $xdg_cache_home/fish/"$argv[1]"
    end; and echo $xdg_cache_home/fish/"$argv[1]"
end
