function fish_md5
    if type -q md5sum
        # GNU systems
        echo (md5sum $argv[1] | string split ' ')[1]
        return 0
    else if type -q md5
        # BSD systems
        md5 -q $argv[1]
        return 0
    end
    return 1
end
