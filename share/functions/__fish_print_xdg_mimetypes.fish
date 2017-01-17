function __fish_print_xdg_mimetypes --description 'Print XDG mime types'
    for file in {~/.local,/usr}/share/applications/mimeinfo.cache
        if test -r $file
            string match -v "[MIME Cache]" <$file | string replace = \t
        end
    end
end
