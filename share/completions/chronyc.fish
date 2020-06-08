# Completions for chronyc

# Global options
complete -f -c chronyc -s 4 -d "Use only IPv4 address for hostname resolution"
complete -f -c chronyc -s 6 -d "Use only IPv6 address for hostname resolution"
complete -f -c chronyc -s n -d "Disable resolving of IP address to hostname"
complete -f -c chronyc -s c -d "Print the report in CSV format"
complete -f -c chronyc -s d -d "Enable print of debugging message"
complete -f -c chronyc -s m -d "Allow multiple commands to be specified"
complete -f -c chronyc -s h -d "Specify the host to connect"
complete -f -c chronyc -s p -d "Specify the UDP port number used by the target to monitor connections"
complete -f -c chronyc -s f -d "Option for compatibility"
complete -f -c chronyc -s a -d "Option for compatibility"
complete -f -c chronyc -s v -d "Show version number"

# Commands
## System clock
complete -f -c chronyc -n __fish_use_subcommand -a tracking -d "Show system time info"
complete -f -c chronyc -n __fish_use_subcommand -a makestep -d "Correct the clock by stepping"
complete -f -c chronyc -n __fish_use_subcommand -a maxupdateskew -d "Modify max valid skew to update frequency"
complete -f -c chronyc -n __fish_use_subcommand -a waitsync -d "Wait until synced in specified limits"

## Time sources
complete -f -c chronyc -n __fish_use_subcommand -a sources -d "Show info about current sources"
complete -x -c chronyc -n "__fish_seen_subcommand_from sources" -s v -d "Be verbose"
complete -f -c chronyc -n __fish_use_subcommand -a sourcestats -d "Show statistics about collected measurements"
complete -x -c chronyc -n "__fish_seen_subcommand_from sourcestats" -s v -d "Be verbose"
complete -f -c chronyc -n __fish_use_subcommand -a reselect -d "Force reselecting sync source"
complete -f -c chronyc -n __fish_use_subcommand -a reselectdist -d "Modify reselection distance"

## NTP sources
complete -f -c chronyc -n __fish_use_subcommand -a activity -d "Check how many NTP sources are online/offline"
complete -f -c chronyc -n __fish_use_subcommand -a ntpdata -d "Show info about last valid measurement"
complete -f -c chronyc -n __fish_use_subcommand -a add
complete -f -c chronyc -n "__fish_seen_subcommand_from add" -a peer -d "Add new NTP peer"
complete -f -c chronyc -n "__fish_seen_subcommand_from add" -a server -d "Add new NTP server"
complete -f -c chronyc -n __fish_use_subcommand -a delete -d "Remove server/peer"
complete -f -c chronyc -n __fish_use_subcommand -a burst -d "Start rapid set of measurements"
complete -f -c chronyc -n __fish_use_subcommand -a maxdelay -d "Modify max valid sample delay"
complete -f -c chronyc -n __fish_use_subcommand -a maxdelaydevratio -d "Modify max valid delay/deviation ratio"
complete -f -c chronyc -n __fish_use_subcommand -a maxdelayratio -d "Modify max valid delay/min ratio"
complete -f -c chronyc -n __fish_use_subcommand -a maxpoll -d "Modify max polling interval"
complete -f -c chronyc -n __fish_use_subcommand -a minpoll -d "Modify min polling interval"
complete -f -c chronyc -n __fish_use_subcommand -a minstratum -d "Modify min stratum"
complete -f -c chronyc -n __fish_use_subcommand -a offline -d "Set sources in subnet to offline status"
complete -f -c chronyc -n __fish_use_subcommand -a online -d "Set sources in subnet to online status"
complete -f -c chronyc -n __fish_use_subcommand -a onoffline -d "Set all sources to online/offline status according to network config"
complete -f -c chronyc -n __fish_use_subcommand -a polltarget -d "Modify poll target"
complete -f -c chronyc -n __fish_use_subcommand -a refresh -d "Refresh IP address"

## Manual time input
complete -f -c chronyc -n __fish_use_subcommand -a manual
complete -f -c chronyc -n "__fish_seen_subcommand_from manual" -a on -d "Enable settime command"
complete -f -c chronyc -n "__fish_seen_subcommand_from manual" -a off -d "Disable settime command"
complete -f -c chronyc -n "__fish_seen_subcommand_from manual" -a delete -d "Delete previous settime entry"
complete -f -c chronyc -n "__fish_seen_subcommand_from manual" -a list -d "Show previous settime entries"
complete -f -c chronyc -n "__fish_seen_subcommand_from manual" -a reset -d "Reset settime command"
complete -f -c chronyc -n __fish_use_subcommand -a settime -d "Set daemon time"

## NTP access
complete -f -c chronyc -n __fish_use_subcommand -a accheck -d "Check whether address is allowed"
complete -f -c chronyc -n __fish_use_subcommand -a clients -d "Report on clients that have accessed the server"
complete -f -c chronyc -n __fish_use_subcommand -a serverstats -d "Show statistics of the server"
complete -f -c chronyc -n __fish_use_subcommand -a allow -d "Allow access to subnet as a default"
complete -f -c chronyc -n "__fish_seen_subcommand_from allow" -a all -d "Allow access to subnet and all children"
complete -f -c chronyc -n __fish_use_subcommand -a deny -d "Deny access to subnet as a default"
complete -f -c chronyc -n "__fish_seen_subcommand_from deny" -a all -d "Deny access to subnet and all children"
complete -f -c chronyc -n __fish_use_subcommand -a local -d "Serve time even when not synced"
complete -f -c chronyc -n "__fish_seen_subcommand_from local" -a off -d "Don't serve time when not synced"
complete -f -c chronyc -n __fish_use_subcommand -a smoothing -d "Show current time smoothing state"
complete -f -c chronyc -n __fish_use_subcommand -a smoothtime
complete -f -c chronyc -n "__fish_seen_subcommand_from smoothtime" -a activate -d "Activate time smoothing"
complete -f -c chronyc -n "__fish_seen_subcommand_from smoothtime" -a reset -d "Reset time smoothing"

## Monitoring access
complete -f -c chronyc -n __fish_use_subcommand -a cmdaccheck -d "Check whether address is allowed"
complete -f -c chronyc -n __fish_use_subcommand -a cmdallow -d "Allow access to subnet as a default"
complete -f -c chronyc -n "__fish_seen_subcommand_from cmdallow" -a all -d "Allow access to subnet and all children"
complete -f -c chronyc -n __fish_use_subcommand -a cmddeny -d "Deny access to subnet as a default"
complete -f -c chronyc -n "__fish_seen_subcommand_from cmddeny" -a all -d "Deny access to subnet and all children"

## RTC
complete -f -c chronyc -n __fish_use_subcommand -a rtcdata -d "Print current RTC performance parameters"
complete -f -c chronyc -n __fish_use_subcommand -a trimrtc -d "Correct RTC relative to system clock"
complete -f -c chronyc -n __fish_use_subcommand -a writertc -d "Save RTC performance parameters to file"

## Other daemon commands
complete -f -c chronyc -n __fish_use_subcommand -a cyclelogs -d "Close and reopen log files"
complete -f -c chronyc -n __fish_use_subcommand -a dump -d "Dump all measurements to save files"
complete -f -c chronyc -n __fish_use_subcommand -a rekey -d "Reread keys from key file"
complete -f -c chronyc -n __fish_use_subcommand -a shutdown -d "Stop daemon"

## Client commands
complete -f -c chronyc -n __fish_use_subcommand -a dns -d "Configure how hostname and IP address are resolved"
complete -f -c chronyc -n __fish_use_subcommand -a timeout -d "Set initial response timeout"
complete -f -c chronyc -n __fish_use_subcommand -a retries -d "Set max number of retries"
complete -f -c chronyc -n __fish_use_subcommand -a keygen -d "Generate key for key file"
complete -f -c chronyc -n __fish_use_subcommand -a "exit quit" -d "Leave the program"
complete -f -c chronyc -n __fish_use_subcommand -a help -d "Show help message"
