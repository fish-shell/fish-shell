# localization: skip(private)
function __fish_print_xdg_data_directories --description 'Print XDG data directories'
    set -f data_home $XDG_DATA_HOME
    if test -z "$data_home"
        set data_home $HOME/.local/share/
    end

    set -f data_dirs $XDG_DATA_DIRS
    if test -z "$data_dirs"
        set data_dirs /usr/local/share/:/usr/share/
    end

    set data_dirs $data_home:$data_dirs

    for path in (string split : $data_dirs)
        if test -d $path
            echo $path
        end
    end
end
