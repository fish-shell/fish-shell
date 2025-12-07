# localization: skip(private)
function __fish_man1_pages
    if __fish_tried_to_embed_manpages
        status list-files man/man1
    else
        files=$__fish_man_dir/man1/* string join -- \n $files
    end |
        path basename |
        string replace -r -- '.1(.gz)?$' ''
end
