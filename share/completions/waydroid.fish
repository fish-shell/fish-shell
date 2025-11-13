#all subcommands avaliable
set -l waydroid_commands status log init upgrade session container app prop show-full-ui first-launch shell logcat

#help parameter can be used on any (sub)commands
complete -f -c waydroid -s h -l help -d "show help message and exit"

#disable fine completions globally
complete -f -c waydroid

#main command parameter
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -s V -l version -d "show program's version number and exit"
complete -F -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -s l -l log -r -d "path to log file"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -s v -l verbose -d "write even more to the logfiles (this may reduce performance)"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -s q -l quiet -d "do not output any log messages"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -s w -l wait -d "wait for init before running"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -l details-to-stdout -d "print details (e.g. build output) to stdout, instead of writing to the log"

#main command
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a status -d "quick check for the waydroid"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a log -d "follow the waydroid logfile"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a init -d "set up waydroid specific configs and install images"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a upgrade -d "upgrade images"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a session -d "session controller"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a container -d "container controller"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a app -d "applications controller"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a prop -d "android properties controller"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a show-full-ui -d "show android full screen in window"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a first-launch -d "initialize waydroid and start it"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a shell -d "run remote shell command"
complete -f -c waydroid -n "not __fish_seen_subcommand_from $waydroid_commands" -a logcat -d "show android logcat"

#log
complete -f -c waydroid -n "__fish_seen_subcommand_from log" -s n -l lines -d "count of initial output lines"
complete -f -c waydroid -n "__fish_seen_subcommand_from log" -s c -l clear -d "clear the log"

#init
complete -F -c waydroid -n "__fish_seen_subcommand_from init" -s i -l images_path -r -d "custom path to waydroid images"
complete -f -c waydroid -n "__fish_seen_subcommand_from init" -s f -l force -d "re-initialize configs and images"
complete -f -c waydroid -n "__fish_seen_subcommand_from init" -s c -l system_channel -d "custom system channel"
complete -f -c waydroid -n "__fish_seen_subcommand_from init" -s v -l vendor_channel -d "custom vendor channel"
complete -f -c waydroid -n "__fish_seen_subcommand_from init" -s r -l rom_type -ra "lineage bliss" -d "rom type"
complete -f -c waydroid -n "__fish_seen_subcommand_from init" -s s -l system_type -ra "VANILLA FOSS GAPPS" -d "system type"

#upgrade
complete -f -c waydroid -n "__fish_seen_subcommand_from upgrade" -s o -l offline -d "just for updating configs"

#session
complete -f -c waydroid -n "__fish_seen_subcommand_from session; and not __fish_seen_subcommand_from start stop" -a start -d "start session"
complete -f -c waydroid -n "__fish_seen_subcommand_from session; and not __fish_seen_subcommand_from start stop" -a stop -d "stop session"

#container
complete -f -c waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a start -d "start container"
complete -f -c waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a stop -d "stop container"
complete -f -c waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a restart -d "restart container"
complete -f -c waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a freeze -d "freeze container"
complete -f -c waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a unfreeze -d "unfreeze container"

#app
complete -f -c waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a install -r -d "push a single package to the container and install it"
complete -f -c waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a remove -d "remove single app package from the container"
complete -f -c waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a launch -d "start single application"
complete -f -c waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a intent -d "start single application"
complete -f -c waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a list -d "list installed applications"
#enable file completions on app install
complete -F -c waydroid -n "__fish_seen_subcommand_from app; and __fish_seen_subcommand_from install"

#prop
complete -f -c waydroid -n "__fish_seen_subcommand_from prop; and not __fish_seen_subcommand_from get set" -a get -d "get value of property from container"
complete -f -c waydroid -n "__fish_seen_subcommand_from prop; and not __fish_seen_subcommand_from get set" -a set -d "set value to property on container"

#prop get/set - property name completions
set -l waydroid_prop persist.waydroid.height_padding persist.waydroid.width_padding persist.waydroid.width persist.waydroid.height persist.waydroid.multi_windows persist.waydroid.suspend persist.waydroid.uevent persist.waydroid.reverse_scrolling persist.waydroid.no_presentation persist.waydroid.invert_colors persist.waydroid.cursor_on_subsurface persist.waydroid.cursor_force_shm persist.waydroid.no_background_subsurface persist.waydroid.use_subsurface persist.waydroid.fake_touch persist.waydroid.fake_wifi
set -l prop_condition "__fish_seen_subcommand_from prop; and __fish_seen_subcommand_from get set; and not __fish_seen_subcommand_from $waydroid_prop"
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.height_padding" -d 0-9999
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.width_padding" -d 0-9999
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.width" -d 0-9999
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.height" -d 0-9999
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.multi_windows" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.suspend" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.uevent" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.reverse_scrolling" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.no_presentation" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.invert_colors" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.cursor_on_subsurface" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.cursor_force_shm" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.no_background_subsurface" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.use_subsurface" -d bool
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.fake_touch" -d "list of package names"
complete -f -c waydroid -n "$prop_condition" -a "persist.waydroid.fake_wifi" -d "list of package names"

#prop set - boolean value completions
set -l waydroid_prop_bool persist.waydroid.multi_windows persist.waydroid.suspend persist.waydroid.uevent persist.waydroid.reverse_scrolling persist.waydroid.no_presentation persist.waydroid.invert_colors persist.waydroid.cursor_on_subsurface persist.waydroid.cursor_force_shm persist.waydroid.no_background_subsurface persist.waydroid.use_subsurface
set -l prop_condition "__fish_seen_subcommand_from prop; and __fish_seen_subcommand_from set; and __fish_seen_subcommand_from $waydroid_prop_bool; and not __fish_seen_subcommand_from true false"
complete -f -c waydroid -n "$prop_condition" -a "true false"

#shell
complete -f -c waydroid -n "__fish_seen_subcommand_from shell" -s u -l uid -r -d "the UID to run as"
complete -f -c waydroid -n "__fish_seen_subcommand_from shell" -s g -l gid -r -d "the GID to run as"
complete -f -c waydroid -n "__fish_seen_subcommand_from shell" -s s -l context -r -d "transition to the specified SELinux or AppArmor security context"
complete -f -c waydroid -n "__fish_seen_subcommand_from shell" -s L -l nolsm -d "Don't perform security domain transition related to mandatory access control"
complete -f -c waydroid -n "__fish_seen_subcommand_from shell" -s C -l allcaps -d "Don't drop capabilities"
complete -f -c waydroid -n "__fish_seen_subcommand_from shell" -s G -l nocgroup -d "Don't switch to the container cgroup"

#below subcommands don't have any parameter or subcommand avaliable
#status
#show-full-ui
#first-launch
#logcat
