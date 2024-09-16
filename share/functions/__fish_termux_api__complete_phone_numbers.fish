function __fish_termux_api__complete_phone_numbers
    set -l command termux-contact-list
    set numbers ($command | jq --raw-output '.[].number')
    set names ($command | jq --raw-output '.[].name')

    for index in (seq (count $numbers))
        set -l number $numbers[$index]
        set -l name $names[$index]

        test -z "$number" && continue
        test -z "$name" && set name "Unknown name"

        printf "%s\t%s\n" $number $name
    end
end
