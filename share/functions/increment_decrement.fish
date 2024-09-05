function increment_decrement
    # issue: -1 goes to -0 not 0
    # issue: cursor doesnt change position when losing a digit ex -10 to -9
    # add comments/documentation
    # refactor code: combine into one function and use local variables, reduce repetitive code (add1 subtract1), rename functions and variables
    if $argv = "add1"
        add1
    else
        subtract1
    end
end

function replace_buf
    set -l right_cut (echo $current_buffer | cut -c $cursor_position-)
    set -l new_right_buffer (echo $right_cut | sed "s/$first_number/$argv/")
    set -l left_cut (echo $current_buffer | cut -c -(math $cursor_position - 1) 2>/dev/null)

    if test -z "$left_cut"
        commandline -r \ $new_right_buffer
    else
        set -l new_buffer (echo $left_cut$new_right_buffer)
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

function add1
    set -g current_buffer (commandline -b)
    set -g cursor_position (commandline -C)
    set -g original_cursor_position (commandline -C)
    set -g cursor_character (echo $current_buffer | cut -c (math $cursor_position + 1))

    if not string match -qr '^[0-9]+$' $cursor_character
        commandline -f repaint
        return
    end

    get_char
    if string match -qr 'true' $isneg
        set -l new_number1 (echo $first_number | cut -c 2-)
        set -l new_number (math $new_number1 - 1)
        set -l firstchar (echo $new_number1 | cut -c 1)
        if string match -qr '0' $firstchar
            set -l formatted_number (printf "%0*d" (string length $new_number1) $new_number)
            replace_buf "-$formatted_number"
        else
            replace_buf "-$new_number"
        end
    else
        set -l new_number (math $first_number + 1)
        # set -l firstchar (echo $first_number | cut -c 1)
        set -l firstchar (echo $first_number | grep -o '[0-9]' | head -1)
        if string match -qr '0' $firstchar
            if string match -qr 'true' $negzero
                set -l new_number2 (echo $first_number | cut -c 2-)
                set -l new_number (math $new_number2 + 1)
                set -l formatted_number (printf "%0*d" (string length $new_number2) $new_number)
                replace_buf "$formatted_number"
                set -g negzero 'false'
            else
                set -l formatted_number (printf "%0*d" (string length $first_number) $new_number)
                replace_buf "$formatted_number"
            end
        else
            replace_buf "$new_number"
        end
    end
end

function subtract1
    set -g current_buffer (commandline -b)
    set -g cursor_position (commandline -C)
    set -g original_cursor_position (commandline -C)
    set -g cursor_character (echo $current_buffer | cut -c (math $cursor_position + 1))

    if not string match -qr '^[0-9]+$' $cursor_character
        return
    end

    get_char
    if string match -qr 'true' $isneg
        set -l new_number1 (echo $first_number | cut -c 2-)
        set -l new_number (math $new_number1 + 1)
        set -l firstchar (echo $new_number1 | cut -c 1)
        if string match -qr '0' $firstchar
            set -l formatted_number (printf "%0*d" (string length $new_number1) $new_number)
            replace_buf "-$formatted_number"
        else 
            replace_buf "-$new_number"
        end
    else
        set -l new_number (math $first_number - 1)
        set -l firstchar (echo $first_number | cut -c 1)
        if string match -qr '0' $firstchar
            set -l checknewpref (echo $new_number | grep -o '-')
            set -l checkoldpref (echo $first_number | grep -o '-')
            if test -z "$checknewpref"
                set -l formatted_number (printf "%0*d" (string length $first_number) $new_number)
                replace_buf "$formatted_number"
            else 
                if test -z "$checkoldpref"
                    set -l formatted_number (printf "%0*d" (math (string length $first_number)+1) $new_number)
                    replace_buf "$formatted_number"
                else
                    set -l formatted_number (printf "%0*d" (string length $first_number) $new_number)
                    replace_buf "$formatted_number"
                end
            end
        else
            replace_buf "$new_number"
        end
    end
end

function get_first_num
    set -l num (echo "$argv" | grep -Eo '(-?[0-9]+)' | head -1)
    echo $num
end

function is_number
    set -l argsv $argv
    if string match -qr '^[0-9]+$' $argsv
        set -g cursor_position (math $cursor_position - 1)
        get_char
    else
        if string match -qr '-' $argsv
            set -g isneg 'true'
            set -g cursor_position $cursor_position
        else
            set -g isneg 'false'
            set -g cursor_position (math $cursor_position + 1)
        end
        set -g char (echo "$current_buffer" | cut -c $cursor_position-)
        set -g first_number (get_first_num $char)
        set -l all0 (echo $first_number | grep -o '[1-9]')
        if string match -qr '-' $argsv
            if test -z "$all0"
                set -g isneg 'false'
                set -g negzero 'true'
            else
                set -g negzero 'false'
            end
        end
    end
end

function get_char
    set -g character (echo "$current_buffer" | cut -c $cursor_position 2>/dev/null)
    is_number "$character"
end

# bind -M insert \ca 'increment_decrement add1'
# bind -M insert \cx 'increment_decrement subtract1'
