# Completions for journalctl
# Author: SanskritFritz (gmail)

set -l all_units '(systemctl list-unit-files --no-pager --no-legend | while read -l unit_name unit_state; echo -e $unit_name\t$unit_state;end | sed "s/ //g")'

complete -c journalctl -f -s h -l help -d 'Prints a short help text and exits'
complete -c journalctl -f      -l version -d 'Prints a short version string and exits'
complete -c journalctl -f      -l no-pager -d 'Do not pipe output into a pager'
complete -c journalctl -f -s a -l all -d 'Show all fields in full'
complete -c journalctl -f -s f -l follow -d 'Show live tail of entries'
complete -c journalctl -f -s n -l lines -d 'Controls the number of journal lines'
complete -c journalctl -f      -l no-tail -d 'Show all lines, even in follow mode'
complete -c journalctl -f -s o -l output -d 'Controls the formatting' -xa 'short short-monotonic verbose export json json-pretty json-sse cat'
complete -c journalctl -f -s q -l quiet -d 'Suppress warning about normal user'
complete -c journalctl -f -s m -l merge -d 'Show entries interleaved from all journals'
complete -c journalctl -f -s b -l this-boot -d 'Show data only from current boot'
complete -c journalctl -f -s u -l unit -d 'Show data only of the specified unit' -xa "$all_units"
complete -c journalctl -f -s p -l priority -d 'Filter by priority' -xa 'emerg 0 alert 1 crit 2 err 3 warning 4 notice 5 info 6 debug 7'
complete -c journalctl -f -s c -l cursor -d 'Start from the passing cursor'
complete -c journalctl -f      -l since -d 'Entries on or newer than DATE' -xa 'yesterday today tomorrow now'
complete -c journalctl -f      -l until -d 'Entries on or older than DATE' -xa 'yesterday today tomorrow now'
complete -c journalctl -f -s F -l field -d 'Print all possible data values'
complete -c journalctl -f -s D -l directory -d 'Specify journal directory' -xa "(__fish_complete_directories (commandline -ct) 'Directory')"
complete -c journalctl -f      -l new-id128 -d 'Generate a new 128 bit ID'
complete -c journalctl -f      -l header -d 'Show internal header information'
complete -c journalctl -f      -l disk-usage -d 'Shows the current disk usage'
complete -c journalctl -f      -l setup-keys -d 'Generate Forward Secure Sealing key pair'
complete -c journalctl -f      -l interval -d 'Change interval for the sealing'
complete -c journalctl -f      -l verify -d 'Check journal for internal consistency'
complete -c journalctl -f      -l verify-key -d 'Specifies FSS key for --verify'
ctl -f      -l verify -d 'Check journal for internal consistency'
complete -c journalctl -f      -l verify-key -d 'Specifies FSS key for --verify'
