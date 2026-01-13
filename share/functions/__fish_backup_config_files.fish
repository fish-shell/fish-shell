# localization: skip(private)
function __fish_backup_config_files
    for relative_filename in $argv
        path is $__fish_config_dir/$relative_filename
        and cp $__fish_config_dir/$relative_filename{,.bak}
    end
end
