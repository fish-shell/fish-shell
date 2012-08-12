complete -c dd -xa '(__fish_complete_dd)'
complete -c dd -d 'display help and exit' -xa '--help'
complete -c dd -d 'output version information and exit' -xa '--version'

function __fish_complete_dd --description 'Complete dd operands'

    # set operand_string as a local variable containing the current command-line token.
    set -l operand_string (commandline -t)

    switch $operand_string

        case 'if=*' 'of=*'
            # the read command uses $IFS to tokenise stdin input
            set -l IFS =
            # $operand now contains the left side of the operator, $value the right
            echo $operand_string | read -l operand value

            for entry in $value*
                # if $entry is a directory, append a '/'
                if test -d $entry
                    echo $operand"="$entry/
                else
                    echo $operand"="$entry
                end
            end

        case 'iflag=*' 'oflag=*'
            set -l flags append direct directory dsync sync fullblock nonblock noatime nocache noctty nofollow

            set -l IFS =
            echo $operand_string | read -l operand value

            set -l IFS ' '
            echo $value | sed -e 's/\(.*\)\(,\)/\1 \2/' | read -l complete comma

            # check if there is only one option
            if test $comma = ''
                set complete ''
            else
                set complete $complete,
            end

            for flag in $flags
                echo $operand"="$complete$flag
            end

        case 'conv=*'
            set -l convs ascii ebcdic ibm block unblock lcase ucase swab sync excl nocreat notrunc noerror fdatasync fsync

            set -l IFS =
            echo $operand_string | read -l operand value

            set -l IFS ' '
            echo $value | sed -e 's/\(.*\)\(,\)/\1 \2/' | read -l complete comma

            if test $comma = ''
                set complete ''
            else
                set complete $complete,
            end

            for conv in $convs
                echo $operand"="$complete$conv
            end

        case 'status=*'
            echo status=noxfer

        case '*'
            set -l operands bs cbs conv count ibs if iflag obs of oflag seek skip status
            for operand in $operands
                echo $operand=
            end
    end
end
