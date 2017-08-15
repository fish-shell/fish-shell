function __fish_print_xdg_applications_directories --description 'Print directories where desktop files are stored'
    set -l search_path ~/.local/share/applications /usr/share/applications
    if test -d /usr/local/share/applications
        set search_path $search_path /usr/local/share/applications
    end

    for p in $search_path
        echo $p
    end
end
