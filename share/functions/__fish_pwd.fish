function __fish_pwd --description "Show current path"
    # It is assumed regex-easyesc feature is enabled by default.
    string replace --regex '^/cygdrive/(.)?' '\U$1:' -- $PWD
end
