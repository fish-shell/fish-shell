function __fish_print_xdg_applications_directories --description 'Print directories where desktop files are stored'
    set -l data_home $XDG_DATA_HOME
    if test -z "$data_home"
        set data_home $HOME/.local/share/
    end

    set -l data_dirs $XDG_DATA_DIRS
    if test -z "$data_dirs"
        set data_dirs /usr/local/share/:/usr/share/
    end

    set data_dirs $data_home:$data_dirs

    # We don't know if the dir ended in a "/" or not, but duplicate slashes are okay.
    for path in (string split : $data_dirs)/applications
        if test -d $path
            echo $path
        end
    end
end
