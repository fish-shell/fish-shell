function __fish_print_xdg_mimeapps --description 'Print xdg mime applications'
    find (__fish_print_xdg_applications_directories) -name \*.desktop \( -type f -or -type l \) -printf '%P\n' | sort -u

end
