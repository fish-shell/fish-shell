function fish_config --description "Launch fish's web based configuration"
    set -lx __fish_bin_dir $__fish_bin_dir
    if command -sq python3
        python3 "$__fish_datadir/tools/web_config/webconfig.py" $argv
    else if command -sq python2
        python2 "$__fish_datadir/tools/web_config/webconfig.py" $argv
    else if command -sq python
        python "$__fish_datadir/tools/web_config/webconfig.py" $argv
    end
end
