# Completions for Android adb command

function __fish_adb_no_subcommand -d 'Test if adb has yet to be given the subcommand'
    for i in (commandline -xpc)
        if contains -- $i connect disconnect devices push pull sync shell emu logcat install uninstall jdwp forward bugreport backup restore version help start-server kill-server remount reboot get-state get-serialno get-devpath status-window root usb tcpip ppp sideload reconnect unroot exec-out
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
        adb devices -l | string replace -rf '(\S+).*product:(\S+).*model:(\S+).*' '$1'\t'$2 $3'
    end
end

function __fish_adb_get_tcpip_devices -d 'Get list of devices connected via TCP/IP'
    __fish_adb_get_devices | string match -r '^\d+\.\d+\.\d+\.\d+:\d+\t.*'
end

function __fish_adb_run_command -d 'Runs adb with any -s parameters already given on the command line'
    set -l sopt
    set -l sopt_is_next
    set -l cmd (commandline -pxc)
    set -e cmd[1]
    for i in $cmd
        if test -n "$sopt_is_next"
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
            set sopt -d
        else if contains -- -e $cmd
            set sopt -e
        end
    end

    # adb returns CRLF (seemingly) so strip CRs
    adb $sopt shell $argv | string replace -a \r ''
end

function __fish_adb_list_packages
    # That "2\>" is to pass the redirection *to adb*.
    # It sends stderr from commands it executes to its stdout as well.
    # Why it does that, I don't know - crossing the streams is a bad idea (c.f. Ghostbusters)
    __fish_adb_run_command pm list packages 2\>/dev/null | string replace 'package:' ''
end

function __fish_adb_list_uninstallable_packages
    # -3 doesn't exactly mean show uninstallable, but it's the closest you can get to with pm list
    __fish_adb_run_command pm list packages -3 | string replace 'package:' ''
end

function __fish_adb_list_local_files
    set -l token (commandline -ct)*

    # Unquoted $token to expand the array
    # Return list of directories suffixed with '/'
    find $token -maxdepth 0 -type d 2>/dev/null | string replace -r '$' /
    # Return list of files
    find $token -maxdepth 0 -type f -o -type l 2>/dev/null
end

function __fish_adb_list_files
    set -l token (commandline -ct)

    # Have tab complete show initial / if nothing on current token
    if test -z "$token"
        set token /
    end

    # Return list of directories suffixed with '/'
    __fish_adb_run_command find -H "$token*" -maxdepth 0 -type d 2\>/dev/null | string replace -r '$' /
    # Return list of files
    __fish_adb_run_command find -H "$token*" -maxdepth 0 -type f 2\>/dev/null
end

function __fish_adb_list_bin
    # list all binary without group
    __fish_adb_run_command ls -1 /system/bin/ 2\>/dev/null
    __fish_adb_run_command ls -1 /system/xbin/ 2\>/dev/null

end

function __fish_adb_list_properties
    __fish_adb_run_command getprop | string match -rg '\[(.*)\]:'
end

# Generic options, must come before command
complete -n __fish_adb_no_subcommand -c adb -o a -d 'Listen on all network interfaces'
complete -n __fish_adb_no_subcommand -c adb -o d -d 'Use first USB device'
complete -n __fish_adb_no_subcommand -c adb -o e -d 'Use first TCP/IP device'
complete -n __fish_adb_no_subcommand -c adb -o s -x -a "(__fish_adb_get_devices)" -d 'Use device with given serial'
complete -n __fish_adb_no_subcommand -c adb -o t -d 'Use device with given transport id'
complete -n __fish_adb_no_subcommand -c adb -o H -d 'Name of adb server host'
complete -n __fish_adb_no_subcommand -c adb -o P -d 'Port of adb server'
complete -n __fish_adb_no_subcommand -c adb -o L -d 'Listen on given socket for adb server'

# Commands
complete -f -n __fish_adb_no_subcommand -c adb -a connect -d 'Connect to device'
complete -f -n __fish_adb_no_subcommand -c adb -a disconnect -d 'Disconnect from device'
complete -f -n __fish_adb_no_subcommand -c adb -a devices -d 'List all connected devices'
complete -f -n __fish_adb_no_subcommand -c adb -a push -d 'Copy file to device'
complete -f -n __fish_adb_no_subcommand -c adb -a pull -d 'Copy file from device'
complete -f -n __fish_adb_no_subcommand -c adb -a sync -d 'Copy host->device only if changed'
complete -f -n __fish_adb_no_subcommand -c adb -a shell -d 'Run remote shell [command]'
complete -f -n __fish_adb_no_subcommand -c adb -a emu -d 'Run emulator console command'
complete -f -n __fish_adb_no_subcommand -c adb -a logcat -d 'View device log'
complete -f -n __fish_adb_no_subcommand -c adb -a install -d 'Install package'
complete -f -n __fish_adb_no_subcommand -c adb -a uninstall -d 'Uninstall package'
complete -f -n __fish_adb_no_subcommand -c adb -a jdwp -d 'List PIDs of processes hosting a JDWP transport'
complete -f -n __fish_adb_no_subcommand -c adb -a forward -d 'Port forwarding'
complete -f -n __fish_adb_no_subcommand -c adb -a bugreport -d 'Return bugreport information'
complete -f -n __fish_adb_no_subcommand -c adb -a backup -d 'Perform device backup'
complete -f -n __fish_adb_no_subcommand -c adb -a restore -d 'Restore device from backup'
complete -f -n __fish_adb_no_subcommand -c adb -a version -d 'Show adb version'
complete -f -n __fish_adb_no_subcommand -c adb -a help -d 'Show adb help'
complete -f -n __fish_adb_no_subcommand -c adb -a wait-for-device -d 'Block until device is online'
complete -f -n __fish_adb_no_subcommand -c adb -a start-server -d 'Ensure that there is a server running'
complete -f -n __fish_adb_no_subcommand -c adb -a kill-server -d 'Kill the server if it is running'
complete -f -n __fish_adb_no_subcommand -c adb -a remount -d 'Remounts the /system partition on the device read-write'
complete -f -n __fish_adb_no_subcommand -c adb -a reboot -d 'Reboots the device, optionally into the bootloader or recovery program'
complete -f -n __fish_adb_no_subcommand -c adb -a get-state -d 'Prints state of the device'
complete -f -n __fish_adb_no_subcommand -c adb -a get-serialno -d 'Prints serial number of the device'
complete -f -n __fish_adb_no_subcommand -c adb -a get-devpath -d 'Prints device path'
complete -f -n __fish_adb_no_subcommand -c adb -a status-window -d 'Continuously print the device status'
complete -f -n __fish_adb_no_subcommand -c adb -a root -d 'Restart the adbd daemon with root permissions'
complete -f -n __fish_adb_no_subcommand -c adb -a unroot -d 'Restart the adbd daemon without root permissions'
complete -f -n __fish_adb_no_subcommand -c adb -a usb -d 'Restart the adbd daemon listening on USB'
complete -f -n __fish_adb_no_subcommand -c adb -a tcpip -d 'Restart the adbd daemon listening on TCP'
complete -f -n __fish_adb_no_subcommand -c adb -a ppp -d 'Run PPP over USB'
complete -f -n __fish_adb_no_subcommand -c adb -a sideload -d 'Sideloads the given package'
complete -f -n __fish_adb_no_subcommand -c adb -a reconnect -d 'Kick current connection from host side and make it reconnect.'
complete -f -n __fish_adb_no_subcommand -c adb -a exec-out -d 'Execute a command on the device and send its stdout back'

# install options
complete -n '__fish_seen_subcommand_from install' -c adb -s l -d 'Forward-lock the app'
complete -n '__fish_seen_subcommand_from install' -c adb -s r -d 'Reinstall the app keeping its data'
complete -n '__fish_seen_subcommand_from install' -c adb -s s -d 'Install on SD card instead of internal storage'
complete -n '__fish_seen_subcommand_from install' -c adb -l algo -d 'Algorithm name'
complete -n '__fish_seen_subcommand_from install' -c adb -l key -d 'Hex-encoded key'
complete -n '__fish_seen_subcommand_from install' -c adb -l iv -d 'Hex-encoded iv'
complete -n '__fish_seen_subcommand_from install' -c adb -ka '(__fish_complete_suffix .apk)'

# uninstall
complete -n '__fish_seen_subcommand_from uninstall' -c adb -s k -d 'Keep the data and cache directories'
complete -n '__fish_seen_subcommand_from uninstall' -c adb -f -a "(__fish_adb_list_uninstallable_packages)"

# devices
complete -n '__fish_seen_subcommand_from devices' -c adb -f
complete -n '__fish_seen_subcommand_from devices' -c adb -s l -d 'Also list device qualifiers'

# disconnect
complete -n '__fish_seen_subcommand_from disconnect' -c adb -x -a "(__fish_adb_get_tcpip_devices)" -d 'Device to disconnect'

# backup
complete -n '__fish_seen_subcommand_from backup' -c adb -s f -d 'File to write backup data to'
complete -n '__fish_seen_subcommand_from backup' -c adb -o apk -d 'Enable backup of the .apks themselves'
complete -n '__fish_seen_subcommand_from backup' -c adb -o noapk -d 'Disable backup of the .apks themselves (default)'
complete -n '__fish_seen_subcommand_from backup' -c adb -o obb -d 'Enable backup of any installed apk expansion'
complete -n '__fish_seen_subcommand_from backup' -c adb -o noobb -d 'Disable backup of any installed apk expansion (default)'
complete -n '__fish_seen_subcommand_from backup' -c adb -o shared -d 'Enable backup of the device\'s shared storage / SD card contents'
complete -n '__fish_seen_subcommand_from backup' -c adb -o noshared -d 'Disable backup of the device\'s shared storage / SD card contents (default)'
complete -n '__fish_seen_subcommand_from backup' -c adb -o all -d 'Back up all installed applications'
complete -n '__fish_seen_subcommand_from backup' -c adb -o system -d 'Include system applications in -all (default)'
complete -n '__fish_seen_subcommand_from backup' -c adb -o nosystem -d 'Exclude system applications in -all'
complete -n '__fish_seen_subcommand_from backup' -c adb -f -a "(__fish_adb_list_packages)" -d 'Package(s) to backup'

# reboot
complete -n '__fish_seen_subcommand_from reboot' -c adb -x -a 'bootloader recovery fastboot'

# forward
complete -n '__fish_seen_subcommand_from forward' -c adb -l list -d 'List all forward socket connections'
complete -n '__fish_seen_subcommand_from forward' -c adb -l no-rebind -d 'Fails the forward if local is already forwarded'
complete -n '__fish_seen_subcommand_from forward' -c adb -l remove -d 'Remove a specific forward socket connection'
complete -n '__fish_seen_subcommand_from forward' -c adb -l remove-all -d 'Remove all forward socket connections'

# sideload
complete -n '__fish_seen_subcommand_from sideload' -c adb -k -xa '(__fish_complete_suffix .zip)'

# reconnect
complete -n '__fish_seen_subcommand_from reconnect' -c adb -x -a device -d 'Kick current connection from device side and make it reconnect.'

# commands that accept listing device files
complete -n '__fish_seen_subcommand_from shell' -c adb -f -a "(__fish_adb_list_files)" -d 'File on device'
complete -n '__fish_seen_subcommand_from pull' -c adb -F -a "(__fish_adb_list_files)" -d 'File on device'
complete -n '__fish_seen_subcommand_from push' -c adb -ka "(__fish_adb_list_files)" -d 'File on device'
complete -n '__fish_seen_subcommand_from push' -c adb -ka "(__fish_adb_list_local_files)"

# logcat
complete -n '__fish_seen_subcommand_from logcat' -c adb -f
# general options
complete -n '__fish_seen_subcommand_from logcat' -c adb -s L -l last -d 'Dump logs from prior to last reboot from pstore'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s b -l buffer -d ' Request alternate ring buffer(s)' -xa '(__fish_append , main system radio events crash default all)'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s c -l clear -d 'Clear (flush) the entire log and exit'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s d -d 'Dump the log and then exit (don\'t block)'
complete -n '__fish_seen_subcommand_from logcat' -c adb -l pid -d 'Only print the logs for the given PID'
complete -n '__fish_seen_subcommand_from logcat' -c adb -l wrap -d 'Sleep for 2 hours or when buffer about to wrap whichever comes first'
# formatting
complete -n '__fish_seen_subcommand_from logcat' -c adb -s v -l format -d 'Sets log print format verb and adverbs' -xa ' brief help long process raw tag thread threadtime time color descriptive epoch monotonic printable uid usec UTC year zone'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s D -l dividers -d 'Print dividers between each log buffer'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s B -l binary -d 'Output the log in binary'
# outfile files
complete -n '__fish_seen_subcommand_from logcat' -c adb -s f -l file -d 'Log to file instead of stdout'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s r -l rotate-kbytes -d 'Rotate log every kbytes, requires -f'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s n -l rotate-count -d 'Sets number of rotated logs to keep, default 4'
complete -n '__fish_seen_subcommand_from logcat' -c adb -l id -d 'If the given signature for logging changes, clear the associated files'
# logd control
complete -n '__fish_seen_subcommand_from logcat' -c adb -s g -l buffer-size -d 'Get the size of the ring buffers within logd'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s G -l buffer-size -d 'Set size of a ring buffer in logd'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s S -l statistics -d 'Output statistics'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s p -l prune -d 'Print prune rules'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s P -l prune -d 'Set prune rules'
# filtering
complete -n '__fish_seen_subcommand_from logcat' -c adb -s s -d 'Set default filter to silent'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s e -l regex -d 'Only print lines where the log message matches'
complete -n '__fish_seen_subcommand_from logcat' -c adb -s m -l max-count -d 'Quit after print <count> lines'
complete -n '__fish_seen_subcommand_from logcat' -c adb -l print -d 'Print all message even if they do not matches, requires --regex and --max-count'
complete -n '__fish_seen_subcommand_from logcat' -c adb -l uid -d 'Only display log messages from UIDs present in the comma separate list <uids>'

# commands that accept listing device binaries
complete -n '__fish_seen_subcommand_from exec-out' -c adb -f -a "(__fish_adb_list_bin)" -d "Command on device"
complete -n '__fish_seen_subcommand_from shell' -c adb -f -a "(__fish_adb_list_bin)" -d "Command on device"

# setprop and getprop in shell
complete -n '__fish_seen_subcommand_from setprop' -c adb -f -a "(__fish_adb_list_properties)" -d 'Property to set'
complete -n '__fish_seen_subcommand_from getprop' -c adb -f -a "(__fish_adb_list_properties)" -d 'Property to get'
