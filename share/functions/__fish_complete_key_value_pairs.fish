# Usage: __fish_complete_key_value_pairs key_value_pair_delimiter key_with_values1 key_with_values2 ...
# Example: __fish_complete_key_value_pairs = "type executable library"
function __fish_complete_key_value_pairs \
    --description 'Suggest provided options and their values as $argv where each except first argument is an option and space separated option values'

    set token (commandline -c -t)
    set delimiter $argv[1]
    set keys $argv[2..]
    set values $keys

    set token (string replace --regex "^--?\w+$delimiter" '' -- $token)

    for index in (seq (count $keys))
        set definition $keys[$index]
        set keys[$index] (string match --regex '^\w+' -- $definition)

        if string match --quiet --regex '^\w+\s+' -- $definition
            set values[$index] (string replace --regex '^\w+\s+' '' -- $definition)
        else
            set values[$index] ""
        end
    end

    set token (string replace --regex "$delimiter.*\$" '' -- $token)

    if set index (contains --index -- "$token" $keys)
        for value in (string split " " $values[$index])
            printf '%s%s%s\n' $token $delimiter $value
        end
    else
        for key in (string split " " $keys)
            printf '%s%s\n' $key $delimiter
        end
    end
end
