function change_number
    set -l cursor_position (commandline -C)
    set -l current_buffer (commandline -b)
    set -l count 0
    set -l split_str (string split '' $current_buffer)
    set -l char_position (math $cursor_position + 1)
    set -l buffer_char $split_str[$char_position]
    set new_string ""
    
    string match -qr '^[0-9]+$' $buffer_char
    or return

    if test "$argv[1]" = increase
        set new_num (math $buffer_char + 1)
    else if test "$argv[1]" = decrease
        set new_num (math $buffer_char - 1)
    end

    for x in (string split '' $current_buffer)
        if [ $count = $cursor_position ]
            set new_string $new_string$new_num
        else
            set new_string $new_string$x
        end
        set count (math $count + 1)
    end
    
    commandline -r $new_string
    commandline -C $cursor_position
    commandline -f repaint
end
