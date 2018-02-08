# Fish completions for the OpenZFS zpool command
# Possible improvements:
# - whenever possible, propose designation of vdevs using their GUID
# - for eligible commands, with arguments of different types, only propose second type completions after the first have been selected; for instance, only propose pool members for offline command
# - this has been written mainly from manpages, which are known to be out-of-sync with the real feature set; some discrepancies have been addressed, but it is highly likely that others still lie

set OS ""
switch (uname)
    case Linux
        set OS "Linux"
    case Darwin
        set OS "macOS"
    case FreeBSD
        set OS "FreeBSD"
    case SunOS
        set OS "SunOS"
    # Others?
    case "*"
        set OS "unknown"
end

# Does the current invocation need a command?
function __fish_zpool_needs_command
    if commandline -c | grep -E -q " (\?|add|attach|clear|create|destroy|detach|events|get|history|import|iostat|labelclear|list|offline|online|reguid|reopen|remove|replace|scrub|set|split|status|upgrade) "
        return 1
    else
        return 0
    end
end

function __fish_zpool_using_command # zpool command whose completions are looked for
    set -l cmd (commandline -opc)
    if test (count $cmd) -eq 1 # The token can only be 'zpool', so skip
        return 2
    end
    if test $cmd[2] = $argv
        return 0
    else
        return 1
    end
end

function __fish_zpool_append -d "Internal completion function for appending string to the zpool commandline"
    set str (commandline -tc| sed -ne "s/\(.*,\)[^,]*/\1/p"|sed -e "s/--.*=//")
    printf "%s\n" "$str"$argv
end

function __fish_zpool_list_used_vdevs -a pool
    if test -z "$pool"
        zpool list -H -v | grep -E "^\s" | cut -f2 | grep -x -E -v "(spare|log|cache|mirror|raidz.?)"
    else
        zpool list -H -v $pool | grep -E "^\s" | cut -f2 | grep -x -E -v "(spare|log|cache|mirror|raidz.?)"
    end
end

function __fish_zpool_list_available_vdevs
    if test $OS = 'Linux'
        find /dev -type b | cut -d'/' -f3
    else if test $OS = 'FreeBSD'
        for i in (camcontrol devlist | cut -d "(" -f 2 | cut -d "," -f 1); ls /dev | grep "^$i"; end
    else if test $OS = 'SunOS'
        ls /dev/dsk
    end
end

function __fish_zpool_complete_vdevs
    # As this function is to be called for completions only when necessary, we don't need to verify that it is relevant for the specified command; this is to be decided by calling, or not, the current function for command completions
    # We can display the physical devices, as they are relevant whereas we are in a vdev definition or not
    __fish_zpool_list_available_vdevs
    # First, reverse token list to analyze it from the end
    set -l reversedTokens ""
    for i in (commandline -co)
        set reversedTokens "$i $reversedTokens"
    end
    set -l tokens 0
    for i in (echo "$reversedTokens" | sed "s/ /\n/g")
        switch $i
            case spare log cache # At least 1 item expected
                if test $tokens -ge 1
                    __fish_zpool_list_vdev_types
                end
                return
            case mirror raidz raidz1 # At least 2 items expected
                if test $tokens -ge 2
                    __fish_zpool_list_vdev_types
                end
                return
            case raidz2 # At least 3 items expected
                if test $tokens -ge 3
                    __fish_zpool_list_vdev_types
                end
                return
            case raidz3 # At least 4 items expected
                if test $tokens -ge 4
                    __fish_zpool_list_vdev_types
                end
                return
            # Here, we accept any possible zpool command; this way, the developper will not have to augment or reduce the list when adding the current function to or removing it from the completions for the said command
            case \? add attach clear create destroy detach events get history import iostat labelclear list offline online reguid reopen remove replace scrub set split status upgrade
                __fish_zpool_list_vdev_types
                return
            case "" # Au cas où
                echo "" > /dev/null
            case "*"
                if echo "$i" | grep -q -E '^((-.*)|(.*(=|,).*))$' # The token is an option or an option argument; as no option uses a vdev as its argument, we can abandon commandline parsing
                    __fish_zpool_list_vdev_types
                    return
                end
        end
        set tokens (math $tokens+1)
    end
end

function __fish_zpool_list_get_fields
    echo -e "name\t"(_ "Pool full name")
    echo -e "property\t"(_ "Property")
    echo -e "value\t"(_ "Property value")
    echo -e "source\t"(_ "Property value origin")
end

function __fish_zpool_list_vdev_types
    echo -e "mirror\t"(_ "Mirror of at least two devices")
    echo -e "raidz\t"(_ "ZFS RAID-5 variant, single parity")
    echo -e "raidz1\t"(_ "ZFS RAID-5 variant, single parity")
    echo -e "raidz2\t"(_ "ZFS RAID-5 variant, double parity")
    echo -e "raidz3\t"(_ "ZFS RAID-5 variant, triple parity")
    echo -e "spare\t"(_ "Pseudo vdev for pool hot spares")
    echo -e "log\t"(_ "SLOG device")
    echo -e "cache\t"(_ "L2ARC device")
end

function __fish_zpool_list_ro_properties
    echo -e "alloc\t"(_ "Physically allocated space")
    if contains -- $OS SunOS Linux
        echo -e "available\t"(_ "Available space")
        echo -e "avail\t"(_ "Available space")
    end
    if test $OS = 'SunOS'
        echo -e "bootsize\t"(_ "System boot partition size")
    end
    echo -e "capacity\t"(_ "Usage percentage of pool")
    echo -e "dedupratio\t"(_ "Deduplication ratio") 
    echo -e "expandsize\t"(_ "Amount of uninitialized space within the pool")
    echo -e "fragmentation\t"(_ "Fragmentation percentage of pool")
    echo -e "free\t"(_ "Free pool space")
    echo -e "freeing\t"(_ "Remaining pool space to be freed")
    echo -e "guid\t"(_ "Pool GUID")
    echo -e "health\t"(_ "Current pool health")
    echo -e "size\t"(_ "Total pool space")
    echo -e "used\t"(_ "Used pool space")
    # Unsupported features
    for i in (zpool list -o all | head -n1 | tr -s "[:blank:]" | tr ' ' '\n' | tr "[:upper:]" "[:lower:]" | grep unsupported); echo $i; end
end

function __fish_zpool_list_device_properties
    echo -e "ashift\t"(_ "Pool sector size exponent")" (COUNT)"
end

function __fish_zpool_list_writeonce_properties
    echo -e "altroot\t"(_ "Alternate root directory")" (PATH)"
end

function __fish_zpool_list_importtime_properties
    echo -e "altroot\t"(_ "Alternate root directory")" (PATH)"
    echo -e "readonly\t"(_ "Import pool in read-only mode")" (on, off)"
    echo -e "rdonly\t"(_ "Import pool in read-only mode")" (on, off)"
end

function __fish_zpool_list_rw_properties
    echo -e "autoexpand\t"(_ "Automatic pool expansion on LUN growing")" (on, off)"
    echo -e "expand\t"(_ "Automatic pool expansion on LUN growing")" (on, off)"
    echo -e "autoreplace\t"(_ "Automatic use of replacement device")" (on, off)"
    echo -e "replace\t"(_ "Automatic use of replacement device")" (on, off)"
    echo -e "bootfs\t"(_ "Default bootable dataset")" (POOL/DATASET)"
    echo -e "cachefile\t"(_ "Pool configuration cache")" (PATH, none, '')"
    echo -e "comment\t"(_ "Comment about the pool")" (COMMENT)"
    echo -e "dedupditto\t"(_ "Threshold for writting a ditto copy of deduplicated blocks")" (COUNT)"
    echo -e "delegation\t"(_ "Allow rights delegation on the pool")" (on, off)"
    echo -e "failmode\t"(_ "Behavior in case of catastrophic pool failure")" (wait, continue, panic)"
    echo -e "listsnaps\t"(_ "Display snapshots even if 'zfs list' does not use '-t'")" (on, off)"
    echo -e "version\t"(_ "On-disk version of pool")" (VERSION)"
    # Feature properties
    for featureLong in (zpool list -o all | tr -s "[:blank:]" | tr ' ' '\n' | tr "[:upper:]" "[:lower:]" | grep '^feature@')
        set -l featureShort (echo $featureLong | sed -e 's/feature@\(.*\)/\1/g')
        set featureLong (echo -e "$featureLong\t")
        printf (_ "%sEnable the %s feature\n") $featureLong $featureShort
    end
end

complete -c zpool -f -n '__fish_zpool_needs_command' -s '?' -d 'Display a help message'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'add' -d 'Add new virtual devices to pool'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'attach' -d 'Attach virtual device to a pool device'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'clear' -d 'Clear devices errors in pool'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'create' -d 'Create a new storage pool'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'destroy' -d 'Destroy a storage pool'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'detach' -d 'Detach virtual device from a mirroring pool'
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_needs_command' -a 'events' -d 'Display pool event log'
end
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'export' -d 'Export a pool'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'get' -d 'Get one or several pool properties'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'history' -d 'Display pool command history'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'import' -d 'List importable pools, or import some'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'iostat' -d 'Display pool I/O stats'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'labelclear' -d 'Remove ZFS label information from the specified device'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'list' -d 'List pools with health status and space usage'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'offline' -d 'Take the specified devices offline'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'online' -d 'Bring the specified devices back online'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'reguid' -d 'Reset pool GUID'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'remove' -d 'Remove virtual devices from pool'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'reopen' -d 'Reopen pool devices'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'replace' -d 'Replace a pool virtual device'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'scrub' -d 'Start or stop scrubbing'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'set' -d 'Set a pool property'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'split' -d 'Create a pool by splitting an existing mirror one'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'status' -d 'Display detailed pool health status'
complete -c zpool -f -n '__fish_zpool_needs_command' -a 'upgrade' -d 'List upgradeable pools, or upgrade one'

# add completions
complete -c zpool -f -n '__fish_zpool_using_command add' -s f -d 'Force use of virtual device'
complete -c zpool -f -n '__fish_zpool_using_command add' -s n -d 'Dry run: only display resulting configuration'
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_using_command add' -s g -d 'Display virtual device GUID instead of device name'
    complete -c zpool -f -n '__fish_zpool_using_command add' -s L -d 'Resolve device path symbolic links'
    complete -c zpool -f -n '__fish_zpool_using_command add' -s P -d 'Display device full path'
    complete -c zpool -x -n '__fish_zpool_using_command add' -s o -d 'Pool property' -a '(__fish_zpool_list_device_properties)'
end
complete -c zpool -x -n '__fish_zpool_using_command add' -d 'Pool to add virtual devices to' -a '(__fish_complete_zfs_pools)'
complete -c zpool -x -n '__fish_zpool_using_command add' -d 'Virtual device to add' -a '(__fish_zpool_complete_vdevs)'

# attach completions
complete -c zpool -f -n '__fish_zpool_using_command attach' -s f -d 'Force use of virtual device'
if test $OS = 'Linux'
    complete -c zpool -x -n '__fish_zpool_using_command attach' -s o -d 'Pool property' -a '(__fish_zpool_list_device_properties)'
end
complete -c zpool -x -n '__fish_zpool_using_command attach' -d 'Pool to attach virtual device to' -a '(__fish_complete_zfs_pools)'
complete -c zpool -x -n '__fish_zpool_using_command attach' -d 'Virtual device to operate on' -a '(__fish_zpool_list_available_vdevs)'

# clear completions
if test $OS = 'FreeBSD'
    complete -c zpool -f -n '__fish_zpool_using_command clear' -s F -d 'Initiate recovery mode'
    complete -c zpool -f -n '__fish_zpool_using_command clear; and __fish_contains_opt -s F' -s n -d 'Dry run: only determine if the recovery is possible, without attempting it'
end
complete -c zpool -x -n '__fish_zpool_using_command clear' -d 'Pool to clear errors on' -a '(__fish_complete_zfs_pools)'
complete -c zpool -f -n '__fish_zpool_using_command clear' -d 'Virtual device to operate on' -a '(__fish_zpool_list_used_vdevs)'

# create completions
if test $OS = 'SunOS'
    complete -c zpool -f -n '__fish_zpool_using_command create' -s B -d 'Create whole disk pool with EFI System partition to support booting system with UEFI firmware'
else
    complete -c zpool -f -n '__fish_zpool_using_command create' -s f -d 'Force use of virtual device'
end
complete -c zpool -f -n '__fish_zpool_using_command create' -s n -d 'Dry run, only display resulting configuration'
complete -c zpool -f -n '__fish_zpool_using_command create' -s d -d 'Do not enable any feature on the new pool'
complete -c zpool -x -n '__fish_zpool_using_command create' -s o -d 'Pool property' -a '(__fish_zpool_list_writeonce_properties; __fish_zpool_list_rw_properties)'
complete -c zpool -x -n '__fish_zpool_using_command create' -s O -d 'Root filesystem property' -a '(__fish_complete_zfs_ro_properties; __fish_complete_zfs_rw_properties; __fish_complete_zfs_write_once_properties)'
complete -c zpool -r -n '__fish_zpool_using_command create' -s R -d 'Equivalent to "-o cachefile=none,altroot=ROOT"'
complete -c zpool -x -n '__fish_zpool_using_command create' -s m -d 'Root filesystem mountpoint' -a 'legacy none'
if test $OS = 'Linux'
    complete -c zpool -x -n '__fish_zpool_using_command create' -s t -d 'Set a different in-core pool name'
end
complete -c zpool -x -n '__fish_zpool_using_command create' -d 'Virtual device to add' -a '(__fish_zpool_complete_vdevs)'

# destroy completions
complete -c zpool -f -n '__fish_zpool_using_command destroy' -s f -d 'Force unmounting of all contained datasets'
complete -c zpool -x -n '__fish_zpool_using_command destroy' -d 'Pool to destroy' -a '(__fish_complete_zfs_pools)'

# detach completions
complete -c zpool -x -n '__fish_zpool_using_command clear' -d 'Pool to detach device from' -a '(__fish_complete_zfs_pools)'
complete -c zpool -x -n '__fish_zpool_using_command clear' -d 'Physical device to detach' -a '(__fish_zpool_list_used_vdevs)'

# events completions
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_using_command events' -s v -d 'Print verbose event information'
    complete -c zpool -f -n '__fish_zpool_using_command events' -s H -d 'Print output in a machine-parsable format'
    complete -c zpool -f -n '__fish_zpool_using_command events' -s f -d 'Output appended data as the log grows'
    complete -c zpool -f -n '__fish_zpool_using_command events' -s c -d 'Clear all previous events'
    complete -c zpool -f -n '__fish_zpool_using_command events' -d 'Pool to read events from' -a '(__fish_complete_zfs_pools)'
end

# export completions
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_using_command export' -s a -d 'Export all pools'
end
complete -c zpool -f -n '__fish_zpool_using_command export' -s f -d 'Force unmounting of all contained datasets'
complete -c zpool -x -n '__fish_zpool_using_command export' -d 'Pool to export' -a '(__fish_complete_zfs_pools)'

# get completions
complete -c zpool -f -n '__fish_zpool_using_command get' -s p -d 'Print parsable (exact) values for numbers'
complete -c zpool -f -n '__fish_zpool_using_command get' -s H -d 'Print output in a machine-parsable format'
if contains -- $OS FreeBSD SunOS
    complete -c zpool -x -n '__fish_zpool_using_command get' -s o -d 'Fields to display' -a '(__fish_zpool_append (__fish_zpool_list_get_fields))'
end
complete -c zpool -x -n '__fish_zpool_using_command get' -d 'Properties to get' -a '(__fish_zpool_append (__fish_zpool_list_importtime_properties; __fish_zpool_list_rw_properties; __fish_zpool_list_writeonce_properties; __fish_zpool_list_ro_properties; __fish_zpool_list_device_properties; echo -e "all\t"(_ "All properties")))'
complete -c zpool -x -n '__fish_zpool_using_command get' -d 'Pool to get properties of' -a '(__fish_complete_zfs_pools)'

# history completions
complete -c zpool -f -n '__fish_zpool_using_command history' -s i -d 'Also display internal events'
complete -c zpool -f -n '__fish_zpool_using_command history' -s l -d 'Display log records using long format'
complete -c zpool -f -n '__fish_zpool_using_command history' -d 'Pool to get command history of' -a '(__fish_complete_zfs_pools)'

# import completions
complete -c zpool -r -n '__fish_zpool_using_command import; and __fish_not_contain_opt -s d' -s c -d 'Read configuration from specified cache file'
complete -c zpool -r -n '__fish_zpool_using_command import; and __fish_not_contain_opt -s c' -s d -d 'Search for devices or files in specified directory'
complete -c zpool -f -n '__fish_zpool_using_command import' -s D -d 'List or import destroyed pools only (requires -f for importation)'
complete -c zpool -x -n '__fish_zpool_using_command import' -s o -d 'Mount properties for contained datasets' -a '(__fish_zpool_append (__fish_complete_zfs_mountpoint_properties))'
complete -c zpool -x -n '__fish_zpool_using_command import' -s o -d 'Properties of the imported pool' -a '(__fish_complete_zfs_mountpoint_properties; __fish_zpool_list_importtime_properties; __fish_zpool_list_rw_properties)'
complete -c zpool -f -n '__fish_zpool_using_command import' -s f -d 'Force import'
complete -c zpool -f -n '__fish_zpool_using_command import' -s F -d 'Recovery mode'
complete -c zpool -f -n '__fish_zpool_using_command import' -s a -d 'Search for and import all pools found'
complete -c zpool -f -n '__fish_zpool_using_command import' -s m -d 'Ignore missing log device (risk of loss of last changes)'
complete -c zpool -r -n '__fish_zpool_using_command import' -s R -d 'Equivalent to "-o cachefile=none,altroot=ROOT"'
complete -c zpool -f -n '__fish_zpool_using_command import' -s N -d 'Do not mount contained filesystems'
complete -c zpool -f -n '__fish_zpool_using_command import; and __fish_contains_opt -s F' -s n -d 'Dry run: only determine if the recovery is possible, without attempting it'
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_using_command import; and __fish_contains_opt -s F' -s X -d 'Roll back to a previous TXG (hazardous)'
    complete -c zpool -r -n '__fish_zpool_using_command import' -s T -d 'TXG to roll back to (implies -FX)'
    complete -c zpool -f -n '__fish_zpool_using_command import' -s t -d 'Specify, as the last argument, a temporary pool name'
end
complete -c zpool -f -n '__fish_zpool_using_command import; and __fish_not_contain_opt -s a' -d 'Pool to import' -a '(__fish_complete_zfs_pools)'

# iostat completions
complete -c zpool -x -n '__fish_zpool_using_command iostat' -s T -d 'Display a timestamp using specified format'
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_using_command iostat' -s g -d 'Display virtual device GUID instead of device name'
    complete -c zpool -f -n '__fish_zpool_using_command iostat' -s L -d 'Resolve device path symbolic links'
    complete -c zpool -f -n '__fish_zpool_using_command iostat' -s P -d 'Display device full path'
    complete -c zpool -f -n '__fish_zpool_using_command iostat' -s y -d 'Omit statistics since boot'
end
complete -c zpool -f -n '__fish_zpool_using_command iostat' -s v -d 'Print verbose statistics'
complete -c zpool -f -n '__fish_zpool_using_command iostat' -d 'Pool to retrieve I/O stats from' -a '(__fish_complete_zfs_pools)'

# labelclear completions
complete -c zpool -f -n '__fish_zpool_using_command labelclear' -s f -d 'Treat exported or foreign devices as inactive'
complete -c zpool -x -n '__fish_zpool_using_command labelclear' -d 'Device to clear ZFS label information from' -a '(__fish_zpool_list_available_vdevs)'

# list completions
complete -c zpool -f -n '__fish_zpool_using_command list' -s H -d 'Print output in a machine-parsable format'
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_using_command list' -s g -d 'Display virtual device GUID instead of device name'
    complete -c zpool -f -n '__fish_zpool_using_command list' -s L -d 'Resolve device path symbolic links'
end
complete -c zpool -f -n '__fish_zpool_using_command list' -s P -d 'Display device full path'
complete -c zpool -x -n '__fish_zpool_using_command list' -s T -d 'Display a timestamp using specified format'
complete -c zpool -f -n '__fish_zpool_using_command list' -s v -d 'Print verbose statistics'
complete -c zpool -x -n '__fish_zpool_using_command list' -s o -d 'Property to list' -a '(__fish_zpool_append (__fish_zpool_list_importtime_properties; __fish_zpool_list_rw_properties; __fish_zpool_list_writeonce_properties; __fish_zpool_list_ro_properties; __fish_zpool_list_device_properties))'
complete -c zpool -f -n '__fish_zpool_using_command list' -d 'Pool to list properties of' -a '(__fish_complete_zfs_pools)'

# offline completions
complete -c zpool -f -n '__fish_zpool_using_command offline' -s t -d 'Temporarily offline'
complete -c zpool -x -n '__fish_zpool_using_command offline' -d 'Pool to offline device from' -a '(__fish_complete_zfs_pools)'
complete -c zpool -x -n '__fish_zpool_using_command offline' -d 'Physical device to offline' -a '(__fish_zpool_list_used_vdevs)'

# online completions
complete -c zpool -f -n '__fish_zpool_using_command online' -s e -d 'Expand the device to use all available space'
complete -c zpool -x -n '__fish_zpool_using_command online' -d 'Pool to bring device back online on' -a '(__fish_complete_zfs_pools)'
complete -c zpool -x -n '__fish_zpool_using_command online' -d 'Physical device to bring back online' -a '(__fish_zpool_list_used_vdevs)'

# reguid completions
complete -c zpool -x -n '__fish_zpool_using_command reguid' -d 'Pool which GUID is to be changed' -a '(__fish_complete_zfs_pools)'

# remove completions
complete -c zpool -x -n '__fish_zpool_using_command remove' -d 'Pool to remove device from' -a '(__fish_complete_zfs_pools)'
complete -c zpool -x -n '__fish_zpool_using_command remove' -d 'Physical device to remove' -a '(__fish_zpool_list_used_vdevs)'

# reopen completions
complete -c zpool -x -n '__fish_zpool_using_command reopen' -d 'Pool which devices are to be reopened' -a '(__fish_complete_zfs_pools)'

# replace completions
complete -c zpool -f -n '__fish_zpool_using_command replace' -s f -d 'Force use of virtual device'
if test $OS = 'Linux'
    complete -c zpool -x -n '__fish_zpool_using_command replace' -s o -d 'Pool property' -a '(__fish_zpool_list_device_properties)'
end
complete -c zpool -x -n '__fish_zpool_using_command replace' -d 'Pool to replace device' -a '(__fish_complete_zfs_pools)'
complete -c zpool -x -n '__fish_zpool_using_command replace' -d 'Pool device to be replaced' -a '(__fish_zpool_list_used_vdevs)'
complete -c zpool -f -n '__fish_zpool_using_command replace' -d 'Device to use for replacement' -a '(__fish_zpool_list_available_vdevs)'

# scrub completions
complete -c zpool -f -n '__fish_zpool_using_command scrub' -s s -d 'Stop scrubbing'
if test $OS = 'SunOS'
    complete -c zpool -f -n '__fish_zpool_using_command scrub' -s p -d 'Pause scrubbing'
end
complete -c zpool -x -n '__fish_zpool_using_command scrub' -d 'Pool to start/stop scrubbing' -a '(__fish_complete_zfs_pools)'

# set completions
complete -c zpool -x -n '__fish_zpool_using_command set' -d 'Property to set' -a '(__fish_zpool_list_rw_properties)'
complete -c zpool -x -n '__fish_zpool_using_command set' -d 'Pool which property is to be set' -a '(__fish_complete_zfs_pools)'

# split completions
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_using_command split' -s g -d 'Display virtual device GUID instead of device name'
    complete -c zpool -f -n '__fish_zpool_using_command split' -s L -d 'Resolve device path symbolic links'
    complete -c zpool -f -n '__fish_zpool_using_command split' -s P -d 'Display device full path'
end
complete -c zpool -f -n '__fish_zpool_using_command split' -s n -d 'Dry run: only display resulting configuration'
complete -c zpool -r -n '__fish_zpool_using_command split' -s R -d 'Set altroot for newpool and automatically import it'
complete -c zpool -x -n '__fish_zpool_using_command split' -s o -d 'Pool property' -a '(__fish_zpool_list_writeonce_properties; __fish_zpool_list_rw_properties)'
if test $OS = 'FreeBSD'
    complete -c zpool -x -n '__fish_zpool_using_command split; and __fish_contains_opt -s R' -s o -d 'Mount properties for contained datasets' -a '(__fish_zpool_append (__fish_complete_zfs_mountpoint_properties))'
end
complete -c zpool -x -n '__fish_zpool_using_command split' -d 'Pool to split' -a '(__fish_complete_zfs_pools)'

# status completions
if test $OS = 'Linux'
    complete -c zpool -f -n '__fish_zpool_using_command status' -s g -d 'Display virtual device GUID instead of device name'
    complete -c zpool -f -n '__fish_zpool_using_command status' -s L -d 'Resolve device path symbolic links'
    complete -c zpool -f -n '__fish_zpool_using_command status' -s P -d 'Display device full path'
    complete -c zpool -f -n '__fish_zpool_using_command status' -s D -d 'Display deduplication histogram'
else if test $OS = 'SunOS'
    complete -c zpool -f -n '__fish_zpool_using_command status' -s D -d 'Display deduplication histogram'
end
complete -c zpool -f -n '__fish_zpool_using_command status' -s v -d 'Verbose mode'
complete -c zpool -f -n '__fish_zpool_using_command status' -s x -d 'Only display status for unhealthy or unavailable pools'
complete -c zpool -x -n '__fish_zpool_using_command status' -s T -d 'Display a timestamp using specified format'
complete -c zpool -f -n '__fish_zpool_using_command status' -d 'Pool which status is to be displayed' -a '(__fish_complete_zfs_pools)'

# upgrade completions
complete -c zpool -f -n '__fish_zpool_using_command upgrade' -s a -d 'Upgrade all eligible pools, enabling all supported features'
complete -c zpool -f -n '__fish_zpool_using_command upgrade' -s v -d 'Display upgradeable ZFS versions'
complete -c zpool -x -n '__fish_zpool_using_command upgrade' -s V -d 'Upgrade to the specified legacy version'
complete -c zpool -f -n '__fish_zpool_using_command upgrade; and __fish_not_contain_opt -s a' -d 'Pool which status is to be displayed' -a '(__fish_complete_zfs_pools)'
