# The control program for pulseaudio
# See not just pactl(1) but also pulse-cli-syntax(1)
# Note: Streams are called "sink-input" (for playback) and "source-output" (for recording)

# TODO: Moar commands
# load-module set-card-profile set-{sink,source}-port set-port-latency-offset set-sink-formats
# TODO: Figure out how to get available card profiles and ports

set -l commands stat info list exit {upload,play,remove}-sample {load,unload}-module \
move-{sink-input,source-output} suspend-{sink,source} set-{card-profile,default-sink,sink-port,source-port,port-latency-offset} \
set-{sink,source,sink-input,source-output}-{volume,mute} set-sink-formats subscribe

function __fish_pa_complete_type
	pactl list short $argv
	# The default is to show the number, then the name and then some info - also show the name, then the number as it's a bit friendlier
	pactl list short $argv | string replace -r  '(\w+)\t([-\w]+)' '$2\t$1'
end

function __fish_pa_print_type
	pactl list short $argv
	# Pa allows both a numerical index and a name
	pactl list short $argv | string replace -r  '(\w+)\t.*' '$1'
end 
complete -f -e -c pactl
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a "$commands"

complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a stat -d 'Show statistics about memory usage'
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a info -d 'Show info about the daemon'
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a list -d 'Show all loaded things of the specified type'
complete -f -c pactl -n "__fish_seen_subcommand_from list" -a "modules sinks sources sink-inputs source-outputs clients samples cards"
complete -f -c pactl -n "__fish_seen_subcommand_from list" -a "short" -d "Show shorter output"
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a exit -d 'Ask the daemon to exit'

complete -c pactl -n "not __fish_seen_subcommand_from $commands" -a upload-sample -d 'Upload a file to the sample cache'
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a play-sample -d 'Play a sample from the cache'
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a remove-sample -d 'Remove a sample from the cache'
complete -f -c pactl -n "__fish_seen_subcommand_from play-sample remove-sample" -a '(__fish_pa_complete_type samples)'

# TODO: "list short modules" only shows _loaded_ modules - figure out how to find unloaded ones for load-module
complete -f -c pactl -n "__fish_seen_subcommand_from unload-module" -a '(__fish_pa_complete_type modules)'

complete -f -c pactl -n "__fish_seen_subcommand_from move-sink-input; and not __fish_seen_subcommand_from (__fish_pa_print_type sink-inputs)" \
-a '(__fish_pa_complete_type sink-inputs)'
complete -f -c pactl -n "__fish_seen_subcommand_from move-sink-input; and __fish_seen_subcommand_from (__fish_pa_print_type sink-inputs)" \
-a '(__fish_pa_complete_type sinks)'
complete -f -c pactl -n "__fish_seen_subcommand_from move-source-output; and not __fish_seen_subcommand_from (__fish_pa_print_type source-outputs)" \
-a '(__fish_pa_complete_type source-outputs)'
complete -f -c pactl -n "__fish_seen_subcommand_from move-source-output; and __fish_seen_subcommand_from (__fish_pa_print_type source-outputs)" \
-a '(__fish_pa_complete_type sources)'

# Suspend
for t in source sink
	complete -f -c pactl -n "__fish_seen_subcommand_from suspend-$t; and not __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a "(__fish_pa_complete_type "$t"s)"
	complete -f -c pactl -n "__fish_seen_subcommand_from suspend-$t; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a '0' -d "Resume"
	complete -f -c pactl -n "__fish_seen_subcommand_from suspend-$t; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a '1' -d "Suspend"
end

# Volume and mute
for t in source sink source-output sink-input
	complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-volume; and not __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a "(__fish_pa_complete_type "$t"s)"
	complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-volume; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a "(seq 0 100)%"
	complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-mute; and not __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a "(__fish_pa_complete_type "$t"s)"
	complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-mute; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a "1 true on" -d "Muted"
	complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-mute; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a "0 false off" -d "Unmuted"
	complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-mute; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
	-a "toggle"
end

complete -f -c pactl -n "__fish_seen_subcommand_from set-default-sink" -a "(__fish_pa_complete_type sinks)"

complete -f -c pactl -l version -d 'Print a short version and exit'
complete -f -c pactl -s h -l help -d 'Print help text and exit'
