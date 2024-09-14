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
    -d 'Specify a dialog [t]itle'

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
    -d "Specify a text h[i]nt" \
    -n "__fish_seen_subcommand_from confirm speech text" \
    -x

complete -c $command \
    -s v \
    -d "Specify comma delimited choices" \
    -n "__fish_seen_subcommand_from checkbox radio sheet spinner" \
    -x

complete -c $command \
    -s r \
    -d "Specify a number [r]ange" \
    -n "__fish_seen_subcommand_from counter" \
    -x

complete -c $command \
    -a '"dd-MM-yyyy k:m:s"\tdefault' \
    -s d \
    -d "Specify a [d]ate format" \
    -n "__fish_seen_subcommand_from date" \
    -x

set text_condition "__fish_seen_subcommand_from text"

complete -c $command \
    -s m \
    -d "Enable [m]ultiline input mode" \
    -n "$text_condition; and not __fish_seen_argument -s n" \
    -x

complete -c $command \
    -s n \
    -d "Enable [n]umber input mode" \
    -n "$text_condition; and not __fish_seen_argument -s m" \
    -x

complete -c $command \
    -s p \
    -d "Enable [p]assword input mode" \
    -n $text_condition \
    -x
