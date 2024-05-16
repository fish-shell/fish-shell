# excluding help
set -l subcommands frequency-info frequency-set idle-info idle-set set info monitor

# disable file completion
complete -fc cpupower

# common switches
complete -c cpupower -s h -l help -d "Show supported commands and general usage"
complete -c cpupower -s c -l cpu -x -a all -d "Only show or set values for specific cores" # im not gonna write completion for the cpu bit mask stuff thingy

# complete subcommands
complete -c cpupower -n "not __fish_seen_subcommand_from $subcommands help" -a help -d "Show manpage for subcommands"
complete -c cpupower -n "not __fish_seen_subcommand_from $subcommands help" -a frequency-info -d "Retrieve cpufreq kernel information"
complete -c cpupower -n "not __fish_seen_subcommand_from $subcommands help" -a frequency-set -d "Modify cpufreq settings"
complete -c cpupower -n "not __fish_seen_subcommand_from $subcommands help" -a idle-info -d "Retrieve cpu idle kernel information"
complete -c cpupower -n "not __fish_seen_subcommand_from $subcommands help" -a idle-set -d "Set cpu idle state specific kernel options"
complete -c cpupower -n "not __fish_seen_subcommand_from $subcommands help" -a set -d "Set processor power related kernel or hardware config"
complete -c cpupower -n "not __fish_seen_subcommand_from $subcommands help" -a info -d "Show processor power related kernel or hardware config"
complete -c cpupower -n "not __fish_seen_subcommand_from $subcommands help" -a monitor -d "Report processor frequency and idle statistics"

# complete other subcommands after 'help', but only once
complete -c cpupower -n "__fish_seen_subcommand_from help && not __fish_seen_subcommand_from $subcommands" -x -a "frequency-info frequency-set idle-info idle-set set info monitor"

complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s e -l debug -d "Prints out debug information"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s f -l freq -d "Get frequency the CPU currently runs at, according to the cpufreq core"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s w -l hwfreq -d "Get frequency the CPU currently runs at, by reading it from hardware"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s l -l hwlimits -d "Determine the minimum and maximum CPU frequency allowed"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s d -l driver -d "Determines the used cpufreq kernel driver"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s p -l policy -d "Gets the currently used cpufreq policy"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s g -l governors -d "Determines available cpufreq governors"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s s -l stats -d "Shows cpufreq statistics if available"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s y -l latency -d "Determines the maximum latency on CPU frequency changes"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s o -l proc -d "Prints out information like the old /proc/cpufreq"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s m -l human -d "human-readable output for the -f, -w, -s and -y parameters"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s n -l no-rounding -d "Output frequencies and latencies without rounding off values"
# according to the manual, both of these have the short switch '-a', but -a seems to behave as the letter
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -l related-cpus -a "Determines which CPUs run at the same hardware frequency"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-info" -s a -l affected-cpus -a "Determines which CPUs need to have their frequency coordinated by software"

complete -c cpupower -n "__fish_seen_subcommand_from frequency-set" -s d -l min -x -d "new minimum CPU frequency the governor may select"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-set" -s u -l max -x -d "new maximum CPU frequency the governor may select"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-set" -s g -l governor -x -a "(string split ' ' < /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors)" -d "new cpufreq governor"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-set" -s f -l freq -x -d "specific frequency to be set. Requires userspace governor to be available and loaded"
complete -c cpupower -n "__fish_seen_subcommand_from frequency-set" -s r -l related -d "modify all hardware-related CPUs at the same time"

complete -c cpupower -n "__fish_seen_subcommand_from idle-info" -s f -l silent -d "Only print a summary of all available C-states in the system"

set -l idlestates (LC_ALL=C cpupower idle-info | string replace -f 'Available idle states: ' '')
complete -c cpupower -n "__fish_seen_subcommand_from idle-set" -s d -l disable -x -a "$idlestates" -d "Disable a specific processor sleep state"
complete -c cpupower -n "__fish_seen_subcommand_from idle-set" -s e -l enable -x -a "$idlestates" -d "Enable a specific processor sleep state"
complete -c cpupower -n "__fish_seen_subcommand_from idle-set" -s D -l disable-by-latency -x -d "Disable all idle states with a equal or higher latency than <LATENCY>"
complete -c cpupower -n "__fish_seen_subcommand_from idle-set" -s E -l enable-all -d "Enable all idle states if not enabled already"

complete -c cpupower -n "__fish_seen_subcommand_from set" -s b -l perf-bias -d "Set relative importance of performance vs energy savings"
complete -c cpupower -n "__fish_seen_subcommand_from info" -s b -l perf-bias -d "Get relative importance of performance vs energy savings"

set -l monitors (LC_ALL=C cpupower monitor -l 2>&1 | awk '$1 == "Monitor" {gsub(/"/, "", $2); print $2} $1 == "Available" {print $3}')
complete -c cpupower -n "__fish_seen_subcommand_from monitor" -s m -x -a "$monitors" -d "Only display this monitor"
complete -c cpupower -n "__fish_seen_subcommand_from monitor" -s v -d "Increase verbosity if the binary was compiled with the DEBUG option set"
complete -c cpupower -n "__fish_seen_subcommand_from monitor" -s c -d "Schedule the processs on every core before starting and ending measuring"
complete -c cpupower -n "__fish_seen_subcommand_from monitor" -s i -d "Measure interval"
complete -c cpupower -n "__fish_seen_subcommand_from monitor" -s l -d "List available monitors on your system"
