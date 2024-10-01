# Usage: __fish_complete_key_value_pairs key_value_pair_delimiter key_with_values1 key_with_values2 ...
# Example: __fish_complete_key_value_pairs = "type executable library"
function __fish_complete_key_value_pairs \
    --description 'Suggest provided options and their values as $argv where each except first argument is an option and space separated option values'

    set token (commandline -c -t)
    set delimiter $argv[1]
    set keys $argv[2..]
    set values $keys

    for index in (seq (count $keys))
        set definition $keys[$index]
        set keys[$index] "$(string match --regex '^\w+' -- $definition)"

        if string match --quiet --regex '^\w+\s+' -- $definition
            set values[$index] (string replace --regex '^\w+\s+' '' -- $definition)
        else
            set values[$index] ""
        end
    end

    for key in $keys
        set index (contains --index -- "$key" $keys)
        set value_list (string split " " -- $values[$index])
        echo $key=$value_list | string split " "
    end
end
