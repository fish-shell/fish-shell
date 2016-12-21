function __fish_print_xdg_mimetypes --description 'Print XDG mime types'
    sed -e '/\[MIME Cache\]/d; y!=!\t!' ~/.local/share/applications/mimeinfo.cache /usr/share/applications/mimeinfo.cache
end
