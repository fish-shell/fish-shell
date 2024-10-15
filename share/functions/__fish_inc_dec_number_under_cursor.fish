# Usage:
    # This function can be bound to key shortcuts (like Ctrl+A for increase and Ctrl+X for decrease) in fish shell to modify numbers quickly under the cursor.
    # Key bindings for increasing and decreasing numbers in insert/vi mode mode.
    # bind -M insert \ca '__fish_inc_dec_number_under_cursor increase'
    # bind -M insert \cx '__fish_inc_dec_number_under_cursor decrease'

    function __fish_inc_dec_number_under_cursor
        # Capture the current command line buffer and cursor position.
        set current_buffer (commandline -b)  # Get the entire current command line as a string.
        set cursor_position (commandline -C)  # Get the current cursor position.
        set original_cursor_position (commandline -C)  # Store the original cursor position for restoring later.
        
        # Get the character directly under the cursor.
        set cursor_character (echo $current_buffer | cut -c (math $cursor_position + 1))
    
        # If the character under the cursor is not a digit, repaint the command line and exit.
        if not string match -qr '^[0-9]+$' $cursor_character
            commandline -f repaint
            return
        end
    
        # Capture the character at the current cursor position (for safety, 2>/dev/null hides errors if position is invalid).
        set character (echo "$current_buffer" | cut -c $cursor_position 2>/dev/null)
    
        # Move the cursor backwards to find the start of the number under the cursor.
        while string match -qr '^[0-9]+$' $character  # Check if the character is a digit.
            set cursor_position (math $cursor_position - 1)  # Move the cursor one position left.
            set character (echo "$current_buffer" | cut -c $cursor_position 2>/dev/null)  # Update character.
        end
    
        # If the character is '-', keep cursor position (for negative numbers).
        if not string match -qr '-' $character
            set cursor_position (math $cursor_position + 1)  # Otherwise, move the cursor back to the first digit of the number.
        end
    
        # Capture everything from the cursor position onward.
        set char (echo "$current_buffer" | cut -c $cursor_position-)
        
        # Extract the first number (including potential negative sign) from the string.
        set old_number (echo (echo "$char" | grep -Eo '(-?[0-9]+)' | head -1))
    
        # Decide whether to increase or decrease the number based on the argument passed.
        if [ "$argv[1]" = "increase" ]
            set new_number (math $old_number + 1)  # Increase the number by 1.
        else
            set new_number (math $old_number - 1)  # Decrease the number by 1.
        end
    
        # Handle formatting for negative numbers.
        if string match -qr -- '-' "$new_number"
            set new_number2 (echo $new_number | cut -c 2-)  # Remove the negative sign for formatting.
            set prefix '-'  # Store the negative sign as a prefix.
        else
            set prefix ''  # No prefix for positive numbers.
            set new_number2 $new_number
        end
    
        # Strip the negative sign from the old number if it exists.
        if string match -qr -- '-' $old_number
            set old_number2 (echo $old_number | cut -c 2-)  # Remove the negative sign.
        else
            set old_number2 $old_number  # No modification for positive numbers.
        end
    
        # Preserve leading zeros if the old number had them (e.g., 007 -> 008).
        if string match -qr '0' (echo $old_number | grep -o '[0-9]' | head -1)
            set formatted_number (printf "%0*d" (string length $old_number2) $new_number2)
            set new_number "$prefix$formatted_number"  # Combine the prefix (if any) with the formatted number.
        end
    
        # Modify the part of the buffer from the cursor position onward.
        set right_cut (echo $current_buffer | cut -c $cursor_position-)
        set new_right_buffer (echo $right_cut | sed "s/$old_number/$new_number/")  # Replace old number with new number.
    
        # Capture the left side of the buffer (before the number).
        set left_cut (echo $current_buffer | cut -c -(math $cursor_position - 1) 2>/dev/null)
    
        # Rebuild the buffer with the new number.
        if test -z "$left_cut"
            commandline -r -- $new_right_buffer  # If there's no left part, just replace the right part.
        else
            set new_buffer (echo $left_cut$new_right_buffer)  # Combine left and right parts with the new number.
            commandline -r -- $new_buffer
        end
    
        # # Reposition the cursor back to the original position.
        commandline -C $original_cursor_position  

        commandline -f repaint  # Repaint the command line to reflect the changes.
    end
    
