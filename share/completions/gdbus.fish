# Completions for gdbus (a part of GLib)

# Commands
complete -f -c gdbus -n __fish_use_subcommand -a introspect -d "Introspect a remote object"
complete -f -c gdbus -n __fish_use_subcommand -a monitor -d "Monitor a remote object"
complete -f -c gdbus -n __fish_use_subcommand -a call -d "Invoke a method on a remote object"
complete -f -c gdbus -n __fish_use_subcommand -a emit -d "Emit a signal"
complete -f -c gdbus -n __fish_use_subcommand -a wait -d "Wait for a bus name to appear"
complete -f -c gdbus -n __fish_use_subcommand -a help -d "Prints help"

# Common options
complete -f -c gdbus -n "__fish_seen_subcommand_from introspect monitor call emit wait" -s y -l system -d "Connect to the system bus"
complete -f -c gdbus -n "__fish_seen_subcommand_from introspect monitor call emit wait" -s e -l session -d "Connect to the session bus"
complete -x -c gdbus -n "__fish_seen_subcommand_from introspect monitor call emit wait" -s a -l address -d "Connect to given D-Bus address"
complete -x -c gdbus -n "__fish_seen_subcommand_from introspect monitor call emit" -s d -l dest -d "Destination name"
complete -x -c gdbus -n "__fish_seen_subcommand_from introspect monitor call emit" -s o -l object-path -d "Object path"
complete -f -c gdbus -n "__fish_seen_subcommand_from introspect monitor call emit wait" -s h -l help -d "Prints help"

# Options of introspect command
complete -f -c gdbus -n "__fish_seen_subcommand_from introspect" -s x -l xml -d "Print XML"
complete -f -c gdbus -n "__fish_seen_subcommand_from introspect" -s r -l recurse -d "Introspect children"
complete -f -c gdbus -n "__fish_seen_subcommand_from introspect" -s p -l only-properties -d "Only print properties"

# Options of call command
complete -x -c gdbus -n "__fish_seen_subcommand_from call" -s m -l method -d "Method and interface name"
complete -x -c gdbus -n "__fish_seen_subcommand_from call" -s t -l timeout -d "Timeout in seconds"

# Options of emit command
complete -x -c gdbus -n "__fish_seen_subcommand_from emit" -s s -l signal -d "Signal and interface name"

# Options of wait command
complete -x -c gdbus -n "__fish_seen_subcommand_from wait" -s a -l activate -d "Service to activate"
complete -x -c gdbus -n "__fish_seen_subcommand_from wait" -s t -l timeout -d "Timeout to wait"
