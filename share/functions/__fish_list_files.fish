# localization: skip(private)
function __fish_list_files
    set -l dir $argv[1]
    if set -q __fish_data_dir[1]
        # Construct a directory prefix without trailing slash.
        if test -n "$dir"
            set dir $__fish_data_dir/$dir
        else
            set dir $__fish_data_dir
        end
        set -l files $dir/**
        string replace -- $dir/ '' $files
    else
        status list-files $dir
    end
end
