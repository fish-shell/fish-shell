
function __fish_complete_abook_formats --description 'Complete abook formats'
    set -l pat
    switch $argv[1]
        case in
            set pat '/output:/,$d; /input:\|^$/d'
        case out
            set pat '/input:/,/output:/d; /^$/d'
        case '*'
            return 1
    end
    abook --formats | sed -e $pat -e 's/^\s\+//'
end

complete -c abook -s h -d 'Show usage'
complete -c abook -s C -l config -d 'Use an alternative configuration file' -r
complete -c abook -l datafile -d 'Use an alternative addressbook file' -r
complete -c abook -l mutt-query -d 'Make a query for mutt' -x
complete -c abook -l add-email -d 'Read email message from stdin and add the sender'
complete -c abook -l add-email-quiet -d 'Same as --add-email. Without confirmation'
complete -c abook -l convert -d 'Convert address book files'

set -l convert 'contains -- --convert (commandline -po)'
complete -c abook -l informat -d 'Input file format' -xa '(__fish_complete_abook_formats in)' -n $convert
complete -c abook -l outformat -d 'Output file format' -xa '(__fish_complete_abook_formats out)' -n $convert
complete -c abook -l infile -d 'Input file (default: stdin)' -r -n $convert
complete -c abook -l outfile -d 'Output file (default: stdout)' -r -n $convert

complete -c abook -l formats -d 'Print available formats'
