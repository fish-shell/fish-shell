complete -c dd -xa '(__fish_complete_dd)'
complete -c dd -d 'display help and exit' -xa --help
complete -c dd -d 'output version information and exit' -xa --version

function __fish_complete_dd -d 'Complete dd operands'
    # set operand_string as a local variable containing the current command-line token.
    set -l operand_string (commandline -t)

    switch $operand_string
        case 'if=*' 'of=*'
            string replace = ' ' -- $operand_string | read -l operand value

            # Use normal file completions.
            printf "$operand=%s\n" (complete -C "__fish_command_without_completions $value")

        case 'iflag=*' 'oflag=*'
            string replace = ' ' -- $operand_string | read -l operand complete
            string match -q '*,' -- $complete
            or set complete ''

            printf "%s\t%s\n" "$operand=$complete""append" "append mode (makes sense only for output; conv=notrunc suggested)"
            printf "%s\t%s\n" "$operand=$complete""direct" "use direct I/O for data"
            printf "%s\t%s\n" "$operand=$complete""directory" "fail unless a directory"
            printf "%s\t%s\n" "$operand=$complete""dsync" "use synchronized I/O for data"
            printf "%s\t%s\n" "$operand=$complete""sync" "use synchronized I/O for data and metadata"
            printf "%s\t%s\n" "$operand=$complete""fullblock" "accumulate full blocks of input (iflag only)"
            printf "%s\t%s\n" "$operand=$complete""nonblock" "use non-blocking I/O"
            printf "%s\t%s\n" "$operand=$complete""noatime" "do not update access time"
            printf "%s\t%s\n" "$operand=$complete""nocache" "discard cached data"
            printf "%s\t%s\n" "$operand=$complete""noctty" "do not assign controlling terminal from file"
            printf "%s\t%s\n" "$operand=$complete""nofollow" "do not follow symbolic links"

        case 'conv=*'
            string replace = ' ' -- $operand_string | read -l operand complete
            string match -q '*,' -- $complete
            or set complete ''

            printf "%s\t%s\n" "$operand=$complete""ascii" "from EBCDIC to ASCII"
            printf "%s\t%s\n" "$operand=$complete""ebcdic" "from ASCII to EBCDIC"
            printf "%s\t%s\n" "$operand=$complete""ibm" "from ASCII to alternate EBCDIC"
            printf "%s\t%s\n" "$operand=$complete""block" "pad newline-terminated records with spaces to cbs-size"
            printf "%s\t%s\n" "$operand=$complete""unblock" "replace trailing spaces in cbs-size records with newline"
            printf "%s\t%s\n" "$operand=$complete""lcase" "change upper case to lower case"
            printf "%s\t%s\n" "$operand=$complete""ucase" "change lower case to upper case"
            printf "%s\t%s\n" "$operand=$complete""swab" "swap every pair of input bytes"
            printf "%s\t%s\n" "$operand=$complete""sync" "pad every input block with NULs to ibs-size; with block or ublock use spaces"
            printf "%s\t%s\n" "$operand=$complete""excl" "fail if the output file already exists"
            printf "%s\t%s\n" "$operand=$complete""nocreat" "do not create the output file"
            printf "%s\t%s\n" "$operand=$complete""notrunc" "do not truncate the output file"
            printf "%s\t%s\n" "$operand=$complete""noerror" "continue after read errors"
            printf "%s\t%s\n" "$operand=$complete""fdatasync" "physically write output file data before finishing"
            printf "%s\t%s\n" "$operand=$complete""fsync" "physically write output file data and metadata before finishing"

        case 'status=*'
            printf "%s\t%s\n" status=noxfer "suppress final transfer statistics"
            printf "%s\t%s\n" status=none "suppress everything but errors"
            printf "%s\t%s\n" status=progress "show periodic transfer statistics"

        case '*'
            printf "%s=\t%s\n" bs "read and write up to BYTES bytes at a time"
            printf "%s=\t%s\n" cbs "convert BYTES bytes at a time"
            printf "%s=\t%s\n" conv "convert the file as per the comma separated symbol list"
            printf "%s=\t%s\n" count "copy only BLOCKS input blocks"
            printf "%s=\t%s\n" ibs "read up to BYTES bytes at a time (default 512)"
            printf "%s=\t%s\n" if "read from FILE instead of stdin"
            printf "%s=\t%s\n" iflag "read as per the comma separated symbol list"
            printf "%s=\t%s\n" obs "write BYTES bytes at a time (default 512)"
            printf "%s=\t%s\n" of "write to FILE instead of stdout"
            printf "%s=\t%s\n" oflag "write as per the comma separated symbol list"
            printf "%s=\t%s\n" seek "skip BLOCKS obs-sized blocks at the start of output"
            printf "%s=\t%s\n" skip "skip BLOCKS ibs-sized blocks at the start of input"
            printf "%s=\t%s\n" status "set the level of information to print to stderr"
    end
end
