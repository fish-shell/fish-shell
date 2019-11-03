function __fish_complete_external_command
    command find $PATH/ -maxdepth 1 -perm -u+x 2>&- | string match -r '[^/]*$'
end
