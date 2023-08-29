# The control program for pulseaudio
# See not just pactl(1) but also pulse-cli-syntax(1)
# Note: Streams are called "sink-input" (for playback) and "source-output" (for recording)

# TODO: Moar commands
# set-port-latency-offset set-sink-formats

# These are the actual commands for pactl - we complete only these, and then the cmd commands in that completion
set -l ctlcommands stat info list exit {upload,play,remove}-sample {load,unload}-module \
    move-{sink-input,source-output} suspend-{sink,source} set-{card-profile,default-sink,sink-port,source-port,port-latency-offset} \
    set-{sink,source,sink-input,source-output}-{volume,mute} set-sink-formats subscribe

# HACK: This is the list of commands from pacmd - used so we can use complete -w there
if command -sq pacmd
    set commands (pacmd help 2>/dev/null | string match -r '^ +[-\w]+' | string trim)
else
    set commands $ctlcommands
end

function __fish_pa_complete_type
    # Print a completion candidate for the object type (like "card" or "sink"),
    # with a description.
    # Pa allows both a numerical index and a name
    pactl list short $argv | string replace -rf '(\d+)\s+(\S+)(\s+.*)?' '$2\t$1$3\n$1\t$2$3'
end

function __fish_pa_print_type
    # Print just the object, without description
    __fish_pa_complete_type $argv | string replace -r '\t.*' ''
end

function __fish_pa_list_ports
    # Yes, this is localized
    env LC_ALL=C pactl list cards 2>/dev/null | sed -n -e '/Ports:/,$p' | string match -r '^\t\t\w.*$' | string replace -r '\s+([-+\w:]+): (\w.+)' '$1\t$2'
end

function __fish_pa_list_profiles
    env LC_ALL=C pactl list cards 2>/dev/null | sed -n -e '/Profiles:/,/Active Profile/p' | string match -r '\t\t.*' | string replace -r '\s+([-+\w:]+): (\w.+)' '$1\t$2'
end

# This is needed to filter out loaded modules
function __fish_pa_complete_unloaded_modules
    if command -sq pulseaudio
        # We need to get just the module names
        set -l loaded (__fish_pa_print_type modules | string replace -r '^\w*\t([-\w]+).*' '$1')
        pulseaudio --dump-modules | while read -l name description
            # This is a potential source of slowness, but on my system it's instantaneous
            # with 73 modules
            if not contains -- $name $loaded
                printf "%s\t%s\n" $name $description
            end
        end
    end
end

complete -f -c pactl
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a "$ctlcommands"

complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a stat -d 'Show statistics about memory usage'
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a info -d 'Show info about the daemon'
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a list -d 'Show all loaded things of the specified type'
complete -f -c pactl -n "__fish_seen_subcommand_from list" -a "modules sinks sources sink-inputs source-outputs clients samples cards"
complete -f -c pactl -n "__fish_seen_subcommand_from list" -a short -d "Show shorter output"
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a exit -d 'Ask the daemon to exit'

complete -c pactl -n "not __fish_seen_subcommand_from $commands" -a upload-sample -d 'Upload a file to the sample cache'
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a play-sample -d 'Play a sample from the cache'
complete -f -c pactl -n "not __fish_seen_subcommand_from $commands" -a remove-sample -d 'Remove a sample from the cache'
complete -f -c pactl -n "__fish_seen_subcommand_from play-sample remove-sample" -a '(__fish_pa_complete_type samples)'

complete -f -c pactl -n "__fish_seen_subcommand_from unload-module" -a '(__fish_pa_complete_type modules)'
complete -f -c pactl -n "__fish_seen_subcommand_from load-module" -a '(__fish_pa_complete_unloaded_modules)'

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
        -a '0 false off' -d Resume
    complete -f -c pactl -n "__fish_seen_subcommand_from suspend-$t; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
        -a '1 true on' -d Suspend
    complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-port; and not __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
        -a "(__fish_pa_complete_type "$t"s)"
    complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-port; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
        -a "(__fish_pa_list_ports)"
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
        -a "1 true on" -d Muted
    complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-mute; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
        -a "0 false off" -d Unmuted
    complete -f -c pactl -n "__fish_seen_subcommand_from set-$t-mute; and __fish_seen_subcommand_from (__fish_pa_print_type "$t"s)" \
        -a toggle
end

complete -f -c pactl -n "__fish_seen_subcommand_from set-card-profile; and not __fish_seen_subcommand_from (__fish_pa_print_type cards)" \
    -a "(__fish_pa_complete_type cards)"
complete -f -c pactl -n "__fish_seen_subcommand_from set-card-profile; and __fish_seen_subcommand_from (__fish_pa_print_type cards)" \
    -a "(__fish_pa_list_profiles)"

complete -f -c pactl -n "__fish_seen_subcommand_from set-default-sink" -a "(__fish_pa_complete_type sinks)"

complete -f -c pactl -l version -d 'Print a short version and exit'
complete -f -c pactl -s h -l help -d 'Print help text and exit'
