function funcdel
    functions -e $argv
    rm $__fish_config_dir/functions/$argv.fish
end
