# Completions for mpc, used to control MPD from the command line

set -l subcommands consume crossfade queued mixrampdb mixrampdelay next \
    pause play prev random repeat replaygain single seek seekthrough stop \
    toggle add insert clear crop del mv searchplay shuffle load lsplaylists \
    playlist rm save listall ls search search find findadd list stats mount \
    mount unmount outputs disable enable toggleoutput channels sendmessage \
    waitmessage subscribe idle idleloop version volume update rescan current

# disable file completions
complete -fc mpc

complete -c mpc -s f -l format -d "Configure the format used to display songs"
complete -c mpc -l wait -d "Wait for operation to finish (e. g.  database update)"
complete -c mpc -l range -d "Operate on a range"
complete -c mpc -s q -l quiet -l no-status -d "Prevents the current song status from being printed"
complete -c mpc -l verbose -d "Verbose output"
complete -c mpc -l host -xd "The MPD server to connect to"
complete -c mpc -s p -l port -xd "The TCP port of the MPD server to connect to"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a consume -d "Toggle consume mode if state is not specified"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a crossfade -d "Get and set current amount of crossfading"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a queued -d "Show currently queued (next) song"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a mixrampdb -d "Get/set volume level at which songs with MixRamp tags will be overlapped"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a mixrampdelay -d "Get/set extra delay added computed from MixRamp tags"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a next -d "Start playing next song on queue"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a pause -d "Pause playing"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a play -d "Start playing song-number specified"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a prev -d "Start playing previous song"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a random -d "Toggle random mode if state (on or off) is not specified"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a repeat -d "Toggle repeat mode if state (on or off) is not specified"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a replaygain -d "Set replay gain mode"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a single -d "Toggle single mode if state (on or off) is not specified"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a seek -d "Seek by hour, minute or seconds"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a seekthrough -d "Seek relative to current position"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a stop -d "Stop playing"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a toggle -d "Toggle between play and pause"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a add -d "Add a song from music database to queue"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a insert -d "Add a song from music database to queue after current song"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a clear -d "Empty queue"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a crop -d "Remove all songs except for currently playing song"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a del -d "Remove a queue number from queue"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a mv -l move -d "Change position of song in queue"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a searchplay -d "Search queue for a matching song and play it"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a shuffle -d "Shuffle all songs on queue"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a load -d "Load a file as queue"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a lsplaylists -d "List available playlists"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a playlist -d "List all songs in playlist"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a rm -d "Delete a specific playlist"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a save -d "Save playlist as file"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a listall -d "List file from database"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a ls -d "List all files/folders in directory"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a search -d "Search for substrings in song tags"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a search -d " Search with a filter expression"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a find -d "Exact search with a filter expression"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a findadd -d "Find and add results to current queue"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a list -d "Return a list of all tags of given tag type"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a stats -d "Display statistics about MPD"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a mount -d "List all mounts"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a mount -d "Create a new mount"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a unmount -d "Remove a mount"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a outputs -d "List all available outputs"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a disable -d "Disable output(s)"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a enable -d "Enable output(s)"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a toggleoutput -d "Change status for given output(s)"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a channels -d "List channels that other clients have subscribed to"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a sendmessage -d "Send a message to specified channel"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a waitmessage -d "Wait for at least one message on specified channel"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a subscribe -d "Subscribe to specified channel and continuously receive messages"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a idle -d "Wait until an event occurs"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a idleloop -d "Keep waiting and printing events as they occur"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a version -d "Report version of MPD"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a volume -d "Set volume"

complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a update -d "Scan for updated files in music directory"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a rescan -d "Like update, but also rescans unmodified files"
complete -c mpc -n "not __fish_seen_subcommand_from $subcommands" -a current -d "Show currently playing song"

# Using '(mpc search filename (commandline -ct))' _might_ be faster for larger libraries
complete -c mpc -n "__fish_seen_subcommand_from add insert" -a "(mpc listall)"

complete -c mpc -n "__fish_seen_subcommand_from playlist load" -a "(mpc lsplaylists)"
complete -c mpc -n "__fish_seen_subcommand_from consume random repeat single" -a "on off"
complete -c mpc -n "__fish_seen_subcommand_from replaygain" -a "off track album"

# TODO: sticker subcommand
