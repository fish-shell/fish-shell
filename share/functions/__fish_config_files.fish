# localization: skip(private)
# NOTE: This outputs a mix of absolute and relative paths!
function __fish_config_files
    set -l _flag_user_dir
    argparse user-dir= -- $argv
    or return
    set -l prefix $argv[1]
    set -l suffix $argv[2]
    set -e argv[1..2]
    set -l paths
    if not set -q argv[1]
        set paths $_flag_user_dir/*$suffix
    else
        set paths (path filter $_flag_user_dir/$argv$suffix)
    end
    if not set -q argv[1]
        set -a paths (status list-files $prefix)
    else
        set -a paths (status list-files "$prefix/"$argv$suffix)
    end
    string join \n $paths
end
