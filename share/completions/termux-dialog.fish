set command termux-dialog

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -s l \
    -l list \
    -d '[l]ist all widgets and their options'

complete -c $command \
    -s t \
    -l title \
    -d 'Specify the [t]itle of a dialog' \
    -x

set subcommands_with_descriptions 'confirm\t"Show a confirmation"' \
    'checkbox\t"Select multiple values using checkboxes"' \
    'counter\t"Pick a number in specific range"' \
    'date\t"Pick a date"' \
    'radio\t"Pick a single value from radio buttons"' \
    'sheet\t"Pick a value from sliding bottom sheet"' \
    'spinner\t"Pick a single value from a dropdown spinner"' \
    'speech\t"Obtain speech using device microphone"' \
    'text\t"Input text"' \
    'time\t"Pick a time value"'

set subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)

complete -c $command \
    -a "$subcommands_with_descriptions" \
    -n "not __fish_seen_subcommand_from $subcommands" \

complete -c $command \
    -s i \
    -d "Specify the text h[i]nt of a dialog" \
    -n "__fish_seen_subcommand_from confirm speech text" \
    -x

complete -c $command \
    -s v \
    -d "Specify the comma delimited choices of a dialog" \
    -n "__fish_seen_subcommand_from checkbox radio sheet spinner" \
    -x

complete -c $command \
    -s r \
    -d "Specify the number [r]ange of a dialog" \
    -n "__fish_seen_subcommand_from counter" \
    -x

complete -c $command \
    -a '"dd-MM-yyyy k:m:s"\tdefault' \
    -s d \
    -d "Specify the [d]ate format of a dialog" \
    -n "__fish_seen_subcommand_from date" \
    -x

set text_condition "__fish_seen_subcommand_from text"

complete -c $command \
    -s m \
    -d "Enable the [m]ultiline input mode in a dialog" \
    -n "$text_condition; and not __fish_seen_argument -s n"

complete -c $command \
    -s n \
    -d "Enable the [n]umber input mode in a dialog" \
    -n "$text_condition; and not __fish_seen_argument -s m"

complete -c $command \
    -s p \
    -d "Enable the [p]assword input mode in a dialog" \
    -n $text_condition
