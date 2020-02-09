function __fish_pipestatus_with_signal --description "Print arguments from \$pipestatus replacing values with signal names where appropriate"
    for pstat in $argv
        __fish_status_to_signal $pstat
    end
end
