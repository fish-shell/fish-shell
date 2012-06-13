function __fish_print_xdg_mimeapps --description 'Print xdg mime applications'
	find ~/.local/share/applications/ /usr/share/applications/ -name \*.desktop \( -type f -or -type l \) -printf '%P\n' | sort -u

end
