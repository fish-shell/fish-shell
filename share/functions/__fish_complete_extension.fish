function __fish_complete_extension \
    --description 'Suggest provided extensions as $argv if the current token ends with a dot'

    set token (commandline -c -t)

    switch "$token"
        case '*.'
            for extension in $argv
                printf "%s%s\n" $token $extension
            end
    end
end
