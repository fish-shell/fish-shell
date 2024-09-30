# Usage: __fish_complete_extensions extension1 extension2 ...
# Example: __fish_complete_extensions png jpg
function __fish_complete_extensions \
    --description 'Suggest provided extensions as $argv if the current token ends with a dot'

    set token (commandline -c -t)

    switch "$token"
        case '*.'
            for extension in $argv
                if string match --quiet --regex '\\\t|\t' -- $extension
                    echo -e "$token$extension"
                else
                    printf "%s%s\n" $token $extension
                end
            end
    end
end
