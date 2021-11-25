function change_number
    set -l cursor_position (commandline -C)
    set -l current_buffer (commandline -b)
    set -l num 0
    set -l split_str (string split '' $current_buffer)
    set -l the_num (math $cursor_position + 1)
    set -l line_char $split_str[$the_num]
    set -l new_string ""
    
    string match -qr '^[0-9]+$' $line_char
    or return

    if test "$argv[1]" = increase
        set new_num (math $line_char + 1)
    else if test "$argv[1]" = decrease
        set new_num (math $line_char - 1)
    end

    for x in (string split '' $current_buffer)
        if [ $num = $cursor_position ]
            set -l new_string "$new_string$new_num"
        else
            set -l new_string "$new_string$x"
        end
        set num (math $num + 1)
    end
    
    commandline -r $new_string
    commandline -C $cursor_position
    commandline -f repaint
end
