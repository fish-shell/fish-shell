function withd --description 'Run command in a specified directory'
    set -l oldpwd $PWD
    cd $argv[1]
    or return 1

    set -e argv[1]
    eval $argv
    set -l ret $status
    
    cd $oldpwd
    return $status
end
