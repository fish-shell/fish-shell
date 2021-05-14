# Pulseaudio's pacmd
# This covers the most useful commands
set -l commands (pacmd help 2>/dev/null | string match -r '^ +[-\w]+' | string trim)
complete -f -c pacmd
complete -f -c pacmd -w pactl

# These descriptions are a bit wordy and unnecessary
# Sample: set-source-port Change the port of a source (args: index|name, port-name)
# Or: list-source-outputs List source outputs
complete -f -c pacmd -n "not __fish_seen_subcommand_from $commands" -a "(pacmd help 2>/dev/null | string match ' *' | string trim | string replace -r '\s+' '\t')"

# Since we wrapped pactl, we can also use functions defined there
complete -f -c pacmd -n "__fish_seen_subcommand_from describe-module" -a '(__fish_pa_complete_type modules)'
complete -f -c pacmd -n "__fish_seen_subcommand_from kill-client" -a '(__fish_pa_complete_type clients | string match -r "^[0-9].*")' # match because this only takes an index

for t in client sink-input source-output
    complete -f -c pacmd -n "__fish_seen_subcommand_from kill-$t" -a '(__fish_pa_complete_type '$t's | string match -r "^[0-9].*")' # match because this only takes an index
end

complete -f -c pactl -n "__fish_seen_subcommand_from suspend" -a '0 false off' -d Resume
complete -f -c pactl -n "__fish_seen_subcommand_from suspend" -a '1 true on' -d Suspend
