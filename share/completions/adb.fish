# Completions for Android adb command

function __fish_adb_parse_global_prefix
    set -l mode $argv[1]
    set -l tokens (commandline -pxc)
    set -e tokens[1]

    while set -q tokens[1]
        set -l token $tokens[1]
        set -e tokens[1]
        switch $token
            case -s -t -H -P -L --one-device
                if set -q tokens[1]
                    test "$mode" = forwarded-options
                    and printf '%s\n' $token $tokens[1]
                    set -e tokens[1]
                end
            case -d -e
                test "$mode" = forwarded-options
                and printf '%s\n' $token
            case -a --exit-on-write-error
                # Global option without an argument that does not affect shell queries.
            case '-*'
                if string match -qr '^-([HP].+|s[0-9]|t[0-9]+$)' -- $token
                    test "$mode" = forwarded-options
                    and printf '%s\n' $token
                else
                    test "$mode" = command
                    and echo $token
                    return 0
                end
            case '*'
                test "$mode" = command
                and echo $token
                return 0
        end
    end

    return 1
end

function __fish_adb_using_command
    set -l command (__fish_adb_parse_global_prefix command)
    test -n "$command"; and contains -- $command $argv
end

function __fish_adb_needs_command -d 'Test if adb has yet to be given the subcommand'
    __fish_adb_parse_global_prefix command >/dev/null
    and return 1
    return 0
end

function __fish_adb_seen_shell_command
    __fish_adb_using_command shell exec-out
    and __fish_seen_subcommand_from $argv
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
    set -l adb_opts (__fish_adb_parse_global_prefix forwarded-options)
    # adb returns CRLF (seemingly) so strip CRs
    adb $adb_opts shell $argv | string replace -a \r ''
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
complete -n __fish_adb_needs_command -c adb -o a -d 'Listen on all network interfaces'
complete -n __fish_adb_needs_command -c adb -o d -d 'Use first USB device'
complete -n __fish_adb_needs_command -c adb -o e -d 'Use first TCP/IP device'
complete -n __fish_adb_needs_command -c adb -o s -x -a "(__fish_adb_get_devices)" -d 'Use device with given serial'
complete -n __fish_adb_needs_command -c adb -o t -x -d 'Use device with given transport id'
complete -n __fish_adb_needs_command -c adb -o H -x -d 'Name of adb server host'
complete -n __fish_adb_needs_command -c adb -o P -x -d 'Port of adb server'
complete -n __fish_adb_needs_command -c adb -o L -r -d 'Listen on given socket for adb server'
complete -n __fish_adb_needs_command -c adb -l one-device -x -a "(__fish_adb_get_devices)" -d 'Server only connects to one USB device'
complete -n __fish_adb_needs_command -c adb -l exit-on-write-error -d 'Exit if stdout is closed'

# Commands
complete -f -n __fish_adb_needs_command -c adb -a connect -d 'Connect to device'
complete -f -n __fish_adb_needs_command -c adb -a disconnect -d 'Disconnect from device'
complete -f -n __fish_adb_needs_command -c adb -a devices -d 'List all connected devices'
complete -f -n __fish_adb_needs_command -c adb -a push -d 'Copy file to device'
complete -f -n __fish_adb_needs_command -c adb -a pull -d 'Copy file from device'
complete -f -n __fish_adb_needs_command -c adb -a sync -d 'Copy host->device only if changed'
complete -f -n __fish_adb_needs_command -c adb -a shell -d 'Run remote shell [command]'
complete -f -n __fish_adb_needs_command -c adb -a emu -d 'Run emulator console command'
complete -f -n __fish_adb_needs_command -c adb -a logcat -d 'View device log'
complete -f -n __fish_adb_needs_command -c adb -a install -d 'Install package'
complete -f -n __fish_adb_needs_command -c adb -a uninstall -d 'Uninstall package'
complete -f -n __fish_adb_needs_command -c adb -a jdwp -d 'List PIDs of processes hosting a JDWP transport'
complete -f -n __fish_adb_needs_command -c adb -a forward -d 'Port forwarding'
complete -f -n __fish_adb_needs_command -c adb -a bugreport -d 'Return bugreport information'
complete -f -n __fish_adb_needs_command -c adb -a backup -d 'Perform device backup'
complete -f -n __fish_adb_needs_command -c adb -a restore -d 'Restore device from backup'
complete -f -n __fish_adb_needs_command -c adb -a version -d 'Show adb version'
complete -f -n __fish_adb_needs_command -c adb -a help -d 'Show adb help'
complete -f -n __fish_adb_needs_command -c adb -a wait-for-device -d 'Block until device is online'
complete -f -n __fish_adb_needs_command -c adb -a start-server -d 'Ensure that there is a server running'
complete -f -n __fish_adb_needs_command -c adb -a kill-server -d 'Kill the server if it is running'
complete -f -n __fish_adb_needs_command -c adb -a remount -d 'Remounts the /system partition on the device read-write'
complete -f -n __fish_adb_needs_command -c adb -a reboot -d 'Reboots the device, optionally into the bootloader or recovery program'
complete -f -n __fish_adb_needs_command -c adb -a get-state -d 'Prints state of the device'
complete -f -n __fish_adb_needs_command -c adb -a get-serialno -d 'Prints serial number of the device'
complete -f -n __fish_adb_needs_command -c adb -a get-devpath -d 'Prints device path'
complete -f -n __fish_adb_needs_command -c adb -a root -d 'Restart the adbd daemon with root permissions'
complete -f -n __fish_adb_needs_command -c adb -a unroot -d 'Restart the adbd daemon without root permissions'
complete -f -n __fish_adb_needs_command -c adb -a usb -d 'Restart the adbd daemon listening on USB'
complete -f -n __fish_adb_needs_command -c adb -a tcpip -d 'Restart the adbd daemon listening on TCP'
complete -f -n __fish_adb_needs_command -c adb -a sideload -d 'Sideloads the given package'
complete -f -n __fish_adb_needs_command -c adb -a reconnect -d 'Kick current connection from host side and make it reconnect.'
complete -f -n __fish_adb_needs_command -c adb -a exec-out -d 'Execute a command on the device and send its stdout back'

# install options
complete -n '__fish_adb_using_command install' -c adb -s l -d 'Forward-lock the app'
complete -n '__fish_adb_using_command install' -c adb -s r -d 'Reinstall the app keeping its data'
complete -n '__fish_adb_using_command install' -c adb -s s -d 'Install on SD card instead of internal storage'
complete -n '__fish_adb_using_command install' -c adb -l algo -d 'Algorithm name'
complete -n '__fish_adb_using_command install' -c adb -l key -d 'Hex-encoded key'
complete -n '__fish_adb_using_command install' -c adb -l iv -d 'Hex-encoded iv'
complete -n '__fish_adb_using_command install' -c adb -ka '(__fish_complete_suffix .apk)'

# uninstall
complete -n '__fish_adb_using_command uninstall' -c adb -s k -d 'Keep the data and cache directories'
complete -n '__fish_adb_using_command uninstall' -c adb -f -a "(__fish_adb_list_uninstallable_packages)"

# devices
complete -n '__fish_adb_using_command devices' -c adb -f
complete -n '__fish_adb_using_command devices' -c adb -s l -d 'Also list device qualifiers'

# disconnect
complete -n '__fish_adb_using_command disconnect' -c adb -x -a "(__fish_adb_get_tcpip_devices)" -d 'Device to disconnect'

# backup
complete -n '__fish_adb_using_command backup' -c adb -s f -r -d 'File to write backup data to'
complete -n '__fish_adb_using_command backup' -c adb -o apk -d 'Enable backup of the .apks themselves'
complete -n '__fish_adb_using_command backup' -c adb -o noapk -d 'Disable backup of the .apks themselves (default)'
complete -n '__fish_adb_using_command backup' -c adb -o obb -d 'Enable backup of any installed apk expansion'
complete -n '__fish_adb_using_command backup' -c adb -o noobb -d 'Disable backup of any installed apk expansion (default)'
complete -n '__fish_adb_using_command backup' -c adb -o shared -d 'Enable backup of the device\'s shared storage / SD card contents'
complete -n '__fish_adb_using_command backup' -c adb -o noshared -d 'Disable backup of the device\'s shared storage / SD card contents (default)'
complete -n '__fish_adb_using_command backup' -c adb -o all -d 'Back up all installed applications'
complete -n '__fish_adb_using_command backup' -c adb -o system -d 'Include system applications in -all (default)'
complete -n '__fish_adb_using_command backup' -c adb -o nosystem -d 'Exclude system applications in -all'
complete -n '__fish_adb_using_command backup' -c adb -f -a "(__fish_adb_list_packages)" -d 'Package(s) to backup'

# reboot
complete -n '__fish_adb_using_command reboot' -c adb -x -a 'bootloader recovery fastboot'

# forward
complete -n '__fish_adb_using_command forward' -c adb -l list -d 'List all forward socket connections'
complete -n '__fish_adb_using_command forward' -c adb -l no-rebind -d 'Fails the forward if local is already forwarded'
complete -n '__fish_adb_using_command forward' -c adb -l remove -d 'Remove a specific forward socket connection'
complete -n '__fish_adb_using_command forward' -c adb -l remove-all -d 'Remove all forward socket connections'

# sideload
complete -n '__fish_adb_using_command sideload' -c adb -k -xa '(__fish_complete_suffix .zip)'

# reconnect
complete -n '__fish_adb_using_command reconnect' -c adb -x -a device -d 'Kick current connection from device side and make it reconnect.'

# commands that accept listing device files
complete -n '__fish_adb_using_command exec-out' -c adb -f -a "(__fish_adb_list_files)" -d 'File on device'
complete -n '__fish_adb_using_command shell' -c adb -f -a "(__fish_adb_list_files)" -d 'File on device'
complete -n '__fish_adb_using_command pull' -c adb -F -a "(__fish_adb_list_files)" -d 'File on device'
complete -n '__fish_adb_using_command push' -c adb -ka "(__fish_adb_list_files)" -d 'File on device'
complete -n '__fish_adb_using_command push' -c adb -ka "(__fish_adb_list_local_files)"

# logcat
complete -n '__fish_adb_using_command logcat' -c adb -f
# general options
complete -n '__fish_adb_using_command logcat' -c adb -s L -l last -d 'Dump logs from prior to last reboot from pstore'
complete -n '__fish_adb_using_command logcat' -c adb -s b -l buffer -d ' Request alternate ring buffer(s)' -xa '(__fish_append , main system radio events crash default all)'
complete -n '__fish_adb_using_command logcat' -c adb -s c -l clear -d 'Clear (flush) the entire log and exit'
complete -n '__fish_adb_using_command logcat' -c adb -s d -d 'Dump the log and then exit (don\'t block)'
complete -n '__fish_adb_using_command logcat' -c adb -l pid -d 'Only print the logs for the given PID'
complete -n '__fish_adb_using_command logcat' -c adb -l wrap -d 'Sleep for 2 hours or when buffer about to wrap whichever comes first'
# formatting
complete -n '__fish_adb_using_command logcat' -c adb -s v -l format -d 'Sets log print format verb and adverbs' -xa ' brief help long process raw tag thread threadtime time color descriptive epoch monotonic printable uid usec UTC year zone'
complete -n '__fish_adb_using_command logcat' -c adb -s D -l dividers -d 'Print dividers between each log buffer'
complete -n '__fish_adb_using_command logcat' -c adb -s B -l binary -d 'Output the log in binary'
# outfile files
complete -n '__fish_adb_using_command logcat' -c adb -s f -l file -r -d 'Log to file instead of stdout'
complete -n '__fish_adb_using_command logcat' -c adb -s r -l rotate-kbytes -d 'Rotate log every kbytes, requires -f'
complete -n '__fish_adb_using_command logcat' -c adb -s n -l rotate-count -d 'Sets number of rotated logs to keep, default 4'
complete -n '__fish_adb_using_command logcat' -c adb -l id -d 'If the given signature for logging changes, clear the associated files'
# logd control
complete -n '__fish_adb_using_command logcat' -c adb -s g -l buffer-size -d 'Get the size of the ring buffers within logd'
complete -n '__fish_adb_using_command logcat' -c adb -s G -l buffer-size -d 'Set size of a ring buffer in logd'
complete -n '__fish_adb_using_command logcat' -c adb -s S -l statistics -d 'Output statistics'
complete -n '__fish_adb_using_command logcat' -c adb -s p -l prune -d 'Print prune rules'
complete -n '__fish_adb_using_command logcat' -c adb -s P -l prune -d 'Set prune rules'
# filtering
complete -n '__fish_adb_using_command logcat' -c adb -s s -d 'Set default filter to silent'
complete -n '__fish_adb_using_command logcat' -c adb -s e -l regex -d 'Only print lines where the log message matches'
complete -n '__fish_adb_using_command logcat' -c adb -s m -l max-count -d 'Quit after print <count> lines'
complete -n '__fish_adb_using_command logcat' -c adb -l print -d 'Print all message even if they do not matches, requires --regex and --max-count'
complete -n '__fish_adb_using_command logcat' -c adb -l uid -d 'Only display log messages from UIDs present in the comma separate list <uids>'

# commands that accept listing device binaries
complete -n '__fish_adb_using_command exec-out' -c adb -f -a "(__fish_adb_list_bin)" -d "Command on device"
complete -n '__fish_adb_using_command shell' -c adb -f -a "(__fish_adb_list_bin)" -d "Command on device"

# setprop and getprop in shell
complete -n '__fish_adb_seen_shell_command setprop' -c adb -f -a "(__fish_adb_list_properties)" -d 'Property to set'
complete -n '__fish_adb_seen_shell_command getprop' -c adb -f -a "(__fish_adb_list_properties)" -d 'Property to get'
