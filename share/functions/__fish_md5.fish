function __fish_md5
    if type -q md5sum
        # GNU systems
        if set -q argv[2]
            if test $argv[1] = -s
                echo $argv[2] | md5sum | string split ' ' -f 1
            else
                printf (_ "%s: Too many arguments %s\n") fish_md5 $argv >&2
            end
        else
            md5sum $argv[1] | string split ' ' -f 1
        end
        return 0
    else if type -q md5
        # BSD systems
        if set -q argv[2]
            if test $argv[1] = -s
                md5 -s $argv[2]
            else
                printf (_ "%s: Too many arguments %s\n") fish_md5 $argv >&2
            end
        else
            md5 -q $argv[1]
        end
        return 0
    end
    return 1
end
