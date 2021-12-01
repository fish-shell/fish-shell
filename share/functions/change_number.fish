function change_number 
    set -l cursor_position (commandline -C)
    set -l current_buffer (commandline -b)
    set -l count 0
    set -l split_str (string split '' \ $current_buffer)
    set list_size (count $split_str)
    set -l split_str $split_str[2..$list_size]
    set -l char_position (math $cursor_position + 1)
    set -l buffer_char $split_str[$char_position]
    set new_string ""
    set full_num ""
    set new_num ""
    
    string match -qr '^[0-9]+$' $buffer_char
    or return

    set last_num (math $char_position + 1) 

    while string match -qr '^[0-9]+$' $split_str[$last_num]
        set last_num (math $last_num + 1)
        set cursor_position (math $cursor_position + 1)
    end

    set append false
    set neg false
    set z_count 0

    for x in $split_str
        if [ $x = '-' ]
            set full_num $full_num$x
        else if string match -qr '^[0-9]+$' $x
            if [ $x = 0 ]
                if [ $count != $cursor_position ]
                    if not $append
                        set z_count (math $z_count + 1)
                    end
                end
            end
            set full_num $full_num$x

        else
            set new_string $new_string$full_num$x
            set full_num ''
            set z_count 0
        end

        if $append
            set new_string $new_string$x
        end
            
        if [ $count = $cursor_position ]
            set append true
            set math_str (math $full_num + 1)
            if string match -qr '-' \ $math_str
                set neg true
            else
                set neg false
            end

            if test "$argv[1]" = increase
                set math_str (math $full_num + 1)
                if string match -qr '-' \ $math_str
                    set neg true
                else
                    set neg false
                end

                if [ $full_num = \-1 ]
                    set new_num (printf "%0"(math $z_count + 0)"d" (math $full_num + 1))
                    echo $new_num
                else
                    if $neg
                        set new_num (printf "%0"(math $z_count + 2)"d" (math $full_num + 1))
                    else
                        set new_num (printf "%0"(math $z_count + 1)"d" (math $full_num + 1))
                    end
                end

            else if test "$argv[1]" = decrease
                set math_str (math $full_num - 1)
                if string match -qr '-' \ $math_str
                    set neg true
                else
                    set neg false
                end

                if $neg
                    set new_num (printf "%0"(math $z_count + 2)"d" (math $full_num - 1))
                else
                    set new_num (printf "%0"(math $z_count + 1)"d" (math $full_num - 1))
                end
            end
            set new_string $new_string$new_num
            set full_num ''
        end
        set count (math $count + 1)
    end 
    set new_split (string split '' \ $new_string)
    set new_list_size (count $new_split)
    set new_split $new_split[2..$new_list_size]
    if [ $new_split[(math $cursor_position + 1)] = '-' ]
        set cursor_position (math $cursor_position + 1)
    else if string match -qr '^[0-9]+$' $new_split[$char_position] 
        set cursor_position $cursor_position
    else
        set cursor_position (math $cursor_position - 1) 
    end
    
    if [ $new_split[1] = '-' ]
        commandline -r \ $new_string
        commandline -f beginning-of-line
        commandline -f delete-char
        commandline -f forward-char
        # commandline -f end-of-line
    else
        commandline -r $new_string
    end
    commandline -C $cursor_position
    commandline -f repaint
end

