function __fish_print_xdg_mimetypes --description 'Print XDG mime types'
    cat ~/.local/share/applications/mimeinfo.cache | grep -v "\[MIME Cache\]" | tr = \t
	cat /usr/share/applications/mimeinfo.cache | grep -v "\[MIME Cache\]" | tr = \t

end
