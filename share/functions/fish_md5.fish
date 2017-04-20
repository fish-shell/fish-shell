function fish_md5
    if type -q md5sum
        # GNU systems
        echo (echo $argv[1] | md5sum | string split ' ')[1]
        return 0
    else if type -q md5
        # BSD systems
        md5 -q $argv[1]
        return 0
    end
    return 1
end
