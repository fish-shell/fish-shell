function __fish_print_xdg_desktop_file_ids --description 'Print all available xdg desktop file IDs'
    find (__fish_print_xdg_applications_directories) -name \*.desktop \( -type f -or -type l \) -printf '%P\n' | tr / - | sort -u
end
