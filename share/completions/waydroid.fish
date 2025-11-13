#all subcommands avaliable
set -l commands status log init upgrade session container app prop show-full-ui first-launch shell logcat

#help parameter can be used on any (sub)commands
complete -f waydroid -s h -l help -d "show help message and exit"

#disable file completions globally
complete -f waydroid

#main command parameter
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -s V -l version -d "show version number and exit"
complete -F waydroid -n "not __fish_seen_subcommand_from $commands" -s l -l log -r -d "path to log file"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -s v -l verbose -d "write even more to the logfiles"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -s q -l quiet -d "do not output any log messages"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -s w -l wait -d "wait for init before running"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -l details-to-stdout -d "print details to stdout"

#main command
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a status -d "quick check for the waydroid"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a log -d "follow the waydroid logfile"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a init -d "set up configs and install images"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a upgrade -d "upgrade images"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a session -d "session controller"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a container -d "container controller"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a app -d "applications controller"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a prop -d "android properties controller"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a show-full-ui -d "show android full screen in window"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a first-launch -d "initialize waydroid and start it"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a shell -d "run remote shell command"
complete -f waydroid -n "not __fish_seen_subcommand_from $commands" -a logcat -d "show android logcat"

#log
complete -f waydroid -n "__fish_seen_subcommand_from log" -s n -l lines -d "count of initial output lines"
complete -f waydroid -n "__fish_seen_subcommand_from log" -s c -l clear -d "clear the log"

#init
complete -F waydroid -n "__fish_seen_subcommand_from init" -s i -l images_path -r -d "custom path to waydroid images"
complete -f waydroid -n "__fish_seen_subcommand_from init" -s f -l force -d "re-initialize configs and images"
complete -f waydroid -n "__fish_seen_subcommand_from init" -s c -l system_channel -d "custom system channel"
complete -f waydroid -n "__fish_seen_subcommand_from init" -s v -l vendor_channel -d "custom vendor channel"
complete -f waydroid -n "__fish_seen_subcommand_from init" -s r -l rom_type -ra "lineage bliss" -d "rom type"
complete -f waydroid -n "__fish_seen_subcommand_from init" -s s -l system_type -ra "VANILLA FOSS GAPPS" -d "system type"

#upgrade
complete -f waydroid -n "__fish_seen_subcommand_from upgrade" -s o -l offline -d "just for updating configs"

#session
complete -f waydroid -n "__fish_seen_subcommand_from session; and not __fish_seen_subcommand_from start stop" -a start -d "start session"
complete -f waydroid -n "__fish_seen_subcommand_from session; and not __fish_seen_subcommand_from start stop" -a stop -d "stop session"

#container
complete -f waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a start -d "start container"
complete -f waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a stop -d "stop container"
complete -f waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a restart -d "restart container"
complete -f waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a freeze -d "freeze container"
complete -f waydroid -n "__fish_seen_subcommand_from container; and not __fish_seen_subcommand_from start stop restart freeze unfreeze" -a unfreeze -d "unfreeze container"

#app
complete -f waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a install -r -d "push a single package to the container and install it"
complete -f waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a remove -d "remove single app package from the container"
complete -f waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a launch -d "start single application"
complete -f waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a intent -d "start single application"
complete -f waydroid -n "__fish_seen_subcommand_from app; and not __fish_seen_subcommand_from install remove launch intent list" -a list -d "list installed applications"
#enable file completions on app install
complete -F waydroid -n "__fish_seen_subcommand_from app; and __fish_seen_subcommand_from install"

#prop
complete -f waydroid -n "__fish_seen_subcommand_from prop; and not __fish_seen_subcommand_from get set" -a get -d "get value of property from container"
complete -f waydroid -n "__fish_seen_subcommand_from prop; and not __fish_seen_subcommand_from get set" -a set -d "set value to property on container"

#prop get/set - property name completions
set -l props \
    persist.waydroid.cursor_force_shm \
    persist.waydroid.cursor_on_subsurface \
    persist.waydroid.fake_touch \
    persist.waydroid.fake_wifi \
    persist.waydroid.height \
    persist.waydroid.height_padding \
    persist.waydroid.invert_colors \
    persist.waydroid.multi_windows \
    persist.waydroid.no_background_subsurface \
    persist.waydroid.no_presentation \
    persist.waydroid.reverse_scrolling \
    persist.waydroid.suspend \
    persist.waydroid.uevent \
    persist.waydroid.use_subsurface \
    persist.waydroid.width \
    persist.waydroid.width_padding

set -l prop_condition "__fish_seen_subcommand_from prop; and __fish_seen_subcommand_from get set; and not __fish_seen_subcommand_from $props"
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.height_padding" -d 0-9999
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.width_padding" -d 0-9999
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.width" -d 0-9999
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.height" -d 0-9999
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.multi_windows" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.suspend" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.uevent" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.reverse_scrolling" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.no_presentation" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.invert_colors" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.cursor_on_subsurface" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.cursor_force_shm" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.no_background_subsurface" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.use_subsurface" -d bool
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.fake_touch" -d "list of package names"
complete -f waydroid -n "$prop_condition" -a "persist.waydroid.fake_wifi" -d "list of package names"

#prop set - boolean value completions
set -l bool_props \
    persist.waydroid.cursor_force_shm \
    persist.waydroid.cursor_on_subsurface \
    persist.waydroid.invert_colors \
    persist.waydroid.multi_windows \
    persist.waydroid.no_background_subsurface \
    persist.waydroid.no_presentation \
    persist.waydroid.reverse_scrolling \
    persist.waydroid.suspend \
    persist.waydroid.uevent \
    persist.waydroid.use_subsurface

set -l prop_condition "__fish_seen_subcommand_from prop; and __fish_seen_subcommand_from set; and __fish_seen_subcommand_from $bool_props; and not __fish_seen_subcommand_from true false"
complete -f waydroid -n "$prop_condition" -a "true false"

#shell
complete -f waydroid -n "__fish_seen_subcommand_from shell" -s u -l uid -r -d "the UID to run as"
complete -f waydroid -n "__fish_seen_subcommand_from shell" -s g -l gid -r -d "the GID to run as"
complete -f waydroid -n "__fish_seen_subcommand_from shell" -s s -l context -r -d "transition to the specified SELinux or AppArmor security context"
complete -f waydroid -n "__fish_seen_subcommand_from shell" -s L -l nolsm -d "Don't perform security domain transition related to mandatory access control"
complete -f waydroid -n "__fish_seen_subcommand_from shell" -s C -l allcaps -d "Don't drop capabilities"
complete -f waydroid -n "__fish_seen_subcommand_from shell" -s G -l nocgroup -d "Don't switch to the container cgroup"

#below subcommands don't have any parameter or subcommand avaliable
#status
#show-full-ui
#first-launch
#logcat
