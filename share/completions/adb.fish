# Completions for Android adb command

function __fish_adb_no_subcommand -d 'Test if adb has yet to be given the subcommand'
    for i in (commandline -opc)
        if contains -- $i connect disconnect devices push pull sync shell emu logcat install uninstall jdwp forward bugreport backup restore version help wait-for-device start-server kill-server remount reboot get-state get-serialno get-devpath status-window root usb tcpip ppp sideload reconnect
            return 1
        end
    end
    return 0
end

function __fish_adb_get_devices -d 'Run adb devices and parse output'
    # This seems reasonably portable for all the platforms adb runs on
    set -l procs (ps -Ao comm= | string match 'adb')
    # Don't run adb devices unless the server is already started - it takes a while to init
    if set -q procs[1]
        adb devices -l | string replace -rf '(\S+) +.' '$1'\t | string replace -r \t'.*model:(\S+).*' \t'$1'
    end
end

function __fish_adb_run_command -d 'Runs adb with any -s parameters already given on the command line'
    set -l sopt
    set -l sopt_is_next
    set -l cmd (commandline -poc)
    set -e cmd[1]
    for i in $cmd
        if test $sopt_is_next
            set sopt -s $i
            break
        else
            switch $i
                case -s
                    set sopt_is_next 1
            end
        end
    end

    # If no -s option, see if there's a -d or -e instead
    if test -z "$sopt"
        if contains -- -d $cmd
            set sopt '-d'
        else if contains -- -e $cmd
            set sopt '-e'
        end
    end

    # adb returns CRLF (seemingly) so strip CRs
    adb $sopt shell $argv | string replace -a \r ''
end

function __fish_adb_list_packages
    __fish_adb_run_command pm list packages 2>/dev/null | string replace 'package:' ''
end


function __fish_adb_list_uninstallable_packages
    # -3 doesn't exactly mean show uninstallable, but it's the closest you can get to with pm list
    __fish_adb_run_command pm list packages -3 | string replace 'package:' ''
end

function __fish_adb_list_files
   set -l token (commandline -ct)

   # Have tab complete show initial / if nothing on current token
   if test -z "$token"
       set token "/"
   end

   # Return list of directories suffixed with '/'
   __fish_adb_run_command find -H "$token*" -maxdepth 0 -type d 2>/dev/null | awk '{print $1"/"}'
   # Return list of files
   __fish_adb_run_command find -H "$token*" -maxdepth 0 -type f 2>/dev/null
end


# Generic options, must come before command
complete -n '__fish_adb_no_subcommand' -c adb -s s -x -a "(__fish_adb_get_devices)" -d 'Device to communicate with'
complete -n '__fish_adb_no_subcommand' -c adb -s d -d 'Communicate with first USB device'
complete -n '__fish_adb_no_subcommand' -c adb -s e -d 'Communicate with emulator'

# Commands
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'connect' -d 'Connect to device'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'disconnect' -d 'Disconnect from device'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'devices' -d 'List all connected devices'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'push' -d 'Copy file to device'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'pull' -d 'Copy file from device'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'sync' -d 'Copy host->device only if changed'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'shell' -d 'Run remote shell [command]'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'emu' -d 'Run emulator console command'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'logcat' -d 'View device log'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'install' -d 'Install package'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'uninstall' -d 'Uninstall package'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'jdwp' -d 'List PIDs of processes hosting a JDWP transport'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'forward' -d 'Port forwarding'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'bugreport' -d 'Return bugreport information'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'backup' -d 'Perform device backup'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'restore' -d 'Restore device from backup'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'version' -d 'Show adb version'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'help' -d 'Show adb help'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'wait-for-device' -d 'Block until device is online'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'start-server' -d 'Ensure that there is a server running'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'kill-server' -d 'Kill the server if it is running'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'remount' -d 'Remounts the /system partition on the device read-write'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'reboot' -d 'Reboots the device, optionally into the bootloader or recovery program'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'get-state' -d 'Prints state of the device'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'get-serialno' -d 'Prints serial number of the device'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'get-devpath' -d 'Prints device path'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'status-window' -d 'Continuously print the device status'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'root' -d 'Restart the adbd daemon with root permissions'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'usb' -d 'Restart the adbd daemon listening on USB'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'tcpip' -d 'Restart the adbd daemon listening on TCP'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'ppp' -d 'Run PPP over USB'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'sideload' -d 'Sideloads the given package'
complete -f -n '__fish_adb_no_subcommand' -c adb -a 'reconnect' -d 'Kick current connection from host side and make it reconnect.'

# install options
complete -n '__fish_seen_subcommand_from install' -c adb -s l -d 'Forward-lock the app'
complete -n '__fish_seen_subcommand_from install' -c adb -s r -d 'Reinstall the app keeping its data'
complete -n '__fish_seen_subcommand_from install' -c adb -s s -d 'Install on SD card instead of internal storage'
complete -n '__fish_seen_subcommand_from install' -c adb -l 'algo' -d 'Algorithm name'
complete -n '__fish_seen_subcommand_from install' -c adb -l 'key' -d 'Hex-encoded key'
complete -n '__fish_seen_subcommand_from install' -c adb -l 'iv' -d 'Hex-encoded iv'

# uninstall
complete -n '__fish_seen_subcommand_from uninstall' -c adb -s k -d 'Keep the data and cache directories'
complete -n '__fish_seen_subcommand_from uninstall' -c adb -f -a "(__fish_adb_list_uninstallable_packages)"

# devices
complete -n '__fish_seen_subcommand_from devices' -c adb -s l -d 'Also list device qualifiers'

# disconnect
complete -n '__fish_seen_subcommand_from disconnect' -c adb -x -a "(__fish_adb_get_devices)" -d 'Device to disconnect'

# backup
complete -n '__fish_seen_subcommand_from backup' -c adb -s f -d 'File to write backup data to'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'apk' -d 'Enable backup of the .apks themselves'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'noapk' -d 'Disable backup of the .apks themselves (default)'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'obb' -d 'Enable backup of any installed apk expansion'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'noobb' -d 'Disable backup of any installed apk expansion (default)'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'shared' -d 'Enable backup of the device\'s shared storage / SD card contents'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'noshared' -d 'Disable backup of the device\'s shared storage / SD card contents (default)'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'all' -d 'Back up all installed applications'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'system' -d 'Include system applications in -all (default)'
complete -n '__fish_seen_subcommand_from backup' -c adb -o 'nosystem' -d 'Exclude system applications in -all'
complete -n '__fish_seen_subcommand_from backup' -c adb -f -a "(__fish_adb_list_packages)" -d 'Package(s) to backup'

# reboot
complete -n '__fish_seen_subcommand_from reboot' -c adb -x -a 'bootloader recovery'

# forward
complete -n '__fish_seen_subcommand_from forward' -c adb -l 'list' -d 'List all forward socket connections'
complete -n '__fish_seen_subcommand_from forward' -c adb -l 'no-rebind' -d 'Fails the forward if local is already forwarded'
complete -n '__fish_seen_subcommand_from forward' -c adb -l 'remove' -d 'Remove a specific forward socket connection'
complete -n '__fish_seen_subcommand_from forward' -c adb -l 'remove-all' -d 'Remove all forward socket connections'

# sideload
complete -n '__fish_seen_subcommand_from sideload' -c adb -xa '(__fish_complete_suffix .zip)'

# reconnect
complete -n '__fish_seen_subcommand_from reconnect' -c adb -x -a 'device' -d 'Kick current connection from device side and make it reconnect.'

# commands that accept listing device files
complete -n '__fish_seen_subcommand_from shell' -c adb -f -a "(__fish_adb_list_files)" -d 'File on device'
complete -n '__fish_seen_subcommand_from pull' -c adb -f -a "(__fish_adb_list_files)" -d 'File on device'
