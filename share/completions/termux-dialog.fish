set -l command termux-dialog

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s l -l list -d 'List all widgets and their options'
complete -c $command -s t -l title -x -d 'Specify the title of a dialog'

set -l subcommands_with_descriptions 'confirm\t"Show a confirmation"' \
    'checkbox\t"Select multiple values using checkboxes"' \
    'counter\t"Pick a number in specific range"' \
    'date\t"Pick a date"' \
    'radio\t"Pick a single value from radio buttons"' \
    'sheet\t"Pick a value from sliding bottom sheet"' \
    'spinner\t"Pick a single value from a dropdown spinner"' \
    'speech\t"Obtain speech using device microphone"' \
    'text\t"Input text"' \
    'time\t"Pick a time value"'

set -l subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)

complete -c $command -a "$subcommands_with_descriptions" \
    -n "not __fish_seen_subcommand_from $subcommands"

complete -c $command -s i -x \
    -d "Specify the text hint of a dialog" \
    -n "__fish_seen_subcommand_from confirm speech text"

complete -c $command -s v -x \
    -d "Specify the comma delimited choices of a dialog" \
    -n "__fish_seen_subcommand_from checkbox radio sheet spinner"

complete -c $command -s r -x \
    -d "Specify the number range of a dialog" \
    -n "__fish_seen_subcommand_from counter"

complete -c $command -s d -x \
    -a '"dd-MM-yyyy k:m:s"\tdefault' \
    -d "Specify the date format of a dialog" \
    -n "__fish_seen_subcommand_from date"

set text_condition "__fish_seen_subcommand_from text"

complete -c $command -s m \
    -d "Enable the multiline input mode in a dialog" \
    -n "$text_condition; and not __fish_seen_argument -s n"

complete -c $command -s n \
    -d "Enable the number input mode in a dialog" \
    -n "$text_condition; and not __fish_seen_argument -s m"

complete -c $command -s p \
    -d "Enable the password input mode in a dialog" \
    -n $text_condition
