function __fish_inc_dec_number_under_cursor
    set current_buffer (commandline -b)
    set cursor_position (commandline -C)
    set original_cursor_position (commandline -C)
    set cursor_character (echo $current_buffer | cut -c (math $cursor_position + 1))

    if not string match -qr '^[0-9]+$' $cursor_character
        commandline -f repaint
        return
    end

    set character (echo "$current_buffer" | cut -c $cursor_position 2>/dev/null)

    while true
        if string match -qr '^[0-9]+$' $character
            set cursor_position (math $cursor_position - 1)
            set character (echo "$current_buffer" | cut -c $cursor_position 2>/dev/null)
        else
            break
        end
    end

    if string match -qr '-' $character
        set cursor_position $cursor_position
    else
        set cursor_position (math $cursor_position + 1)
    end

    set char (echo "$current_buffer" | cut -c $cursor_position-)
    set old_number (echo (echo "$char" | grep -Eo '(-?[0-9]+)' | head -1))

    if [ "$argv[1]" = "increase" ]
        set new_number (math $old_number + 1)
    else
        set new_number (math $old_number - 1)
    end

    if string match -qr '-' \ "$new_number"
        set new_number2 (echo $new_number | cut -c 2-)
        set prefix '-'
    else
        set prefix ''
        set new_number2 $new_number
    end

    if string match -qr '-' \ $old_number
        set old_number2 (echo $old_number | cut -c 2-)
    else
        set old_number2 $old_number
    end

    if string match -qr '0' (echo $old_number | grep -o '[0-9]' | head -1)
        set formatted_number (printf "%0*d" (string length $old_number2) $new_number2)
        set new_number "$prefix$formatted_number"
    else
        set new_number "$new_number"
    end

    set right_cut (echo $current_buffer | cut -c $cursor_position-)
    set new_right_buffer (echo $right_cut | sed "s/$old_number/$new_number/")
    set left_cut (echo $current_buffer | cut -c -(math $cursor_position - 1) 2>/dev/null)

    if test -z "$left_cut"
        commandline -r \ $new_right_buffer
    else
        set new_buffer (echo $left_cut$new_right_buffer)
        commandline -r \ $new_buffer
    end

    # commandline -C $original_cursor_position
    commandline -f beginning-of-line
    commandline -f delete-char
    for i in (seq $original_cursor_position)
        commandline -f forward-char
    end
    commandline -f repaint
end

# bind -M insert \ca '__fish_inc_dec_number_under_cursor increase'
# bind -M insert \cx '__fish_inc_dec_number_under_cursor decrease'
