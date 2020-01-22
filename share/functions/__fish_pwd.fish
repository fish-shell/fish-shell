switch (uname)
    case 'CYGWIN_*'
        function __fish_pwd --description "Show current path"
            if status test-feature regex-easyesc
                pwd | string replace -r '^/cygdrive/(.)?' '\U$1:'
            else
                # TODO: Remove this once regex-easyesc becomes the default
                pwd | string replace -r '^/cygdrive/(.)?' '\\\U$1:'
            end
        end
    case '*'
        function __fish_pwd --description "Show current path"
            pwd
        end
end
