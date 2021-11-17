alias vi_dec 'vi_increment_decrement subtract'
alias vi_inc 'vi_increment_decrement add'

function vi_increment_decrement --description 'increment or decrement numbers with ctrl+x ctrl+a'
    set lnum (commandline --cursor)
    set lstr (commandline -b)
    set num 0
    set split_str (string split '' $lstr)
    set the_num (math $lnum + 1)
    set lchar $split_str[$the_num]
    set new_string ""

    if string match -qr '^[0-9]+$' $lchar
        if [ $argv = 'add' ]
            set new_num (math $lchar + 1)
        else if [ $argv = 'subtract' ]
            set new_num (math $lchar - 1)
        end

        for x in (string split '' $lstr)
            if [ $num = $lnum ]
                set new_string (echo $new_string$new_num | string collect)
            else
                set new_string (echo $new_string$x | string collect)
            end
            set num (math $num + 1)
        end
        commandline -r $new_string
        commandline -C $lnum
        commandline -f repaint
    else
        return
    end
end
