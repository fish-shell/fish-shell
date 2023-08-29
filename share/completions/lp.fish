__fish_complete_lpr lp
complete -c lpr -k -xa "(__fish_complete_suffix .pdf .ps)"
complete -c lp -s d -d 'Prints files to the named printer' -xa '(__fish_print_lpr_printers)'
complete -c lp -s i -d 'Specifies an existing job to modify' -x
complete -c lp -s n -d 'Sets the number of copies to print from 1 to 100' -x
complete -c lp -s q -d 'Sets the job priority from 1 (lowest) to 100 (highest)'
complete -c lp -s s -d 'Do not report the resulting job IDs (silent mode)'
complete -c lp -s t -d 'Sets the job name' -x
complete -c lp -s H -d 'Specifies  when the job should be printed' -xa 'hold immediate restart resume HH:MM'
complete -c lp -s P -d 'Specify the page ranges' -x
