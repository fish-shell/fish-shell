__fish_complete_lpr lpr
complete -c lpr -k -xa "(__fish_complete_suffix .pdf .ps)"
complete -c lpr -s H -x -d 'Specifies an alternate server' -xa '(__fish_print_hostnames)'
complete -c lpr -s C -s J -s T -x -d 'Sets the job name'
#complete -c lpr -o '\\#' -d 'Sets the number of copies to print from 1 to 100' -xa
complete -c lpr -s h -d 'Disables banner printing'
complete -c lpr -s l -d 'Send print file without filtering'
complete -c lpr -s p -d 'Format the print file with a shaded header: date, time, job name, and page no.'
complete -c lpr -s q -d 'Hold job for printing'
complete -c lpr -s r -d 'Delete the specified print file after printing'
