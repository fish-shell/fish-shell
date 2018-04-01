function __fish_print_xdg_mimetypes --description 'Print XDG mime types'
    cat (__fish_print_xdg_applications_directories)/mimeinfo.cache 2>/dev/null | string match -v '[MIME Cache]' | string replace = \t
end
