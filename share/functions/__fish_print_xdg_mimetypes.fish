function __fish_print_xdg_mimetypes --description 'Print XDG mime types'
    cat ~/.local/share/applications/mimeinfo.cache /usr/share/applications/mimeinfo.cache ^/dev/null | string match -v '[MIME Cache]' | string replace = \t
end
