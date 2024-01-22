function __fish_complete_abook_formats --description 'Complete abook formats'
    set -l format
    abook --formats | while read -l x
        switch $x
            case "input formats:"
                set format input
            case "output formats:"
                set format output
            case "query-compatible output formats:"
                set format ignore
        end
        set --append "$format" (string replace -rf '\t(.*\t.*)' '$1' -- $x)
    end
    switch $argv[1]
        case in
            for l in $input
                echo $l
            end
        case out
            for l in $output
                echo $l
            end
        case '*'
            return 1
    end
end

complete -c abook -s h -d 'Show usage'
complete -c abook -s C -l config -d 'Use an alternative configuration file' -r
complete -c abook -l datafile -d 'Use an alternative addressbook file' -r
complete -c abook -l mutt-query -d 'Make a query for mutt' -x
complete -c abook -l add-email -d 'Read email message from stdin and add the sender'
complete -c abook -l add-email-quiet -d 'Same as --add-email. Without confirmation'
complete -c abook -l convert -d 'Convert address book files'

set -l convert 'contains -- --convert (commandline -px)'
complete -c abook -l informat -d 'Input file format' -xa '(__fish_complete_abook_formats in)' -n $convert
complete -c abook -l outformat -d 'Output file format' -xa '(__fish_complete_abook_formats out)' -n $convert
complete -c abook -l infile -d 'Input file (default: stdin)' -r -n $convert
complete -c abook -l outfile -d 'Output file (default: stdout)' -r -n $convert

complete -c abook -l formats -d 'Print available formats'
