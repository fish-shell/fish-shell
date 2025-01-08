# virsh is the main interface for managing virtlib guest domains.
# See: https://libvirt.org/virshcmdref.html

set -l cmds attach-device attach-disk attach-interface autostart blkdeviotune blkiotune \
    blockcommit blockcopy blockjob blockpull blockresize change-media console cpu-baseline \
    cpu-compare cpu-stats create define desc destroy detach-device detach-disk detach-interface \
    domdisplay domfsfreeze domfsthaw domfsinfo domfstrim domhostname domid domif-setlink domiftune \
    domjobabort domjobinfo domname domrename dompmsuspend dompmwakeup domuuid domxml-from-native \
    domxml-to-native dump dumpxml edit event inject-nmi iothreadinfo iothreadpin iothreadadd \
    iothreaddel send-key send-process-signal lxc-enter-namespace managedsave managedsave-remove \
    managedsave-edit managedsave-dumpxml managedsave-define memtune perf metadata migrate \
    migrate-setmaxdowntime migrate-getmaxdowntime migrate-compcache migrate-setspeed \
    migrate-getspeed migrate-postcopy numatune qemu-attach qemu-monitor-command \
    qemu-monitor-event qemu-agent-command reboot reset restore resume save save-image-define \
    save-image-dumpxml save-image-edit schedinfo screenshot set-lifecycle-action set-user-password \
    setmaxmem setmem setvcpus shutdown start suspend ttyconsole undefine update-device vcpucount \
    vcpuinfo vcpupin emulatorpin vncdisplay guestvcpus setvcpu domblkthreshold domblkerror \
    domblkinfo domblklist domblkstat domcontrol domif-getlink domifaddr domiflist domifstat \
    dominfo dommemstat domstate domstats domtime list allocpages capabilities cpu-models \
    domcapabilities freecell freepages hostname maxvcpus node-memory-tune nodecpumap nodecpustats \
    nodeinfo nodememstats nodesuspend sysinfo uri version iface-begin iface-bridge iface-commit \
    iface-define iface-destroy iface-dumpxml iface-edit iface-list iface-mac iface-name \
    iface-rollback iface-start iface-unbridge iface-undefine nwfilter-define nwfilter-dumpxml \
    nwfilter-edit nwfilter-list nwfilter-undefine net-autostart net-create net-define net-destroy \
    net-dhcp-leases net-dumpxml net-edit net-event net-info net-list net-name net-start \
    net-undefine net-update net-uuid nodedev-create nodedev-destroy nodedev-detach nodedev-dumpxml \
    nodedev-list nodedev-reattach nodedev-reset nodedev-event secret-define secret-dumpxml \
    secret-event secret-get-value secret-list secret-set-value secret-undefine snapshot-create \
    snapshot-create-as snapshot-current snapshot-delete snapshot-dumpxml snapshot-edit snapshot-info \
    snapshot-list snapshot-parent snapshot-revert find-storage-pool-sources-as \
    find-storage-pool-sources pool-autostart pool-build pool-create-as pool-create pool-define-as \
    pool-define pool-delete pool-destroy pool-dumpxml pool-edit pool-info pool-list pool-name \
    pool-refresh pool-start pool-undefine pool-uuid pool-event vol-clone vol-create-as vol-create \
    vol-create-from vol-delete vol-download vol-dumpxml vol-info vol-key vol-list \
    vol-name vol-path vol-pool vol-resize vol-upload vol-wipe

function __fish_virsh_get_domains --argument-names state --description "Prints the list of virtlib domains with the given state (running, shutoff, paused or transient)."
    set -l filter
    switch "$state"
        case running paused shutoff
            set filter --state-$state
        case inactive transient
            set filter --$state
        case '*'
            set filter --all
    end

    set -l desc (test -n "$state"; and echo (string sub -s1 -l1 -- $state | string upper)(string sub -s2 -- $state) domain; or echo Domain)
    set -l domains (virsh list --name $filter | string match -v -r '^$')
    set -q domains[1]
    and printf "%s\t$desc\n" $domains
end

function __fish_virsh_get_networks
    # Example of `virsh net-list --all` output:
    #
    #  Name                 State      Autostart     Persistent
    # ----------------------------------------------------------
    #  default              active     yes           yes
    #  mynet                active     yes           yes
    #

    set -l header 'Name State Autostart Persistent'
    set -l networks (virsh net-list --all | string replace -ar '[ \t]+' ' ' | string trim | string match -rv -- ---)

    if not string match -q -- $header $networks[1]
        return 1
    end

    set -l networks (string match -v -- $header $networks | string match -rv '^$')
    for network in $networks
        set -l network (string split ' ' $network)
        set -l network_name $network[1]
        set -l network_state $network[2]
        set -l network_autostart $network[3]
        set -l network_persistent $network[4]

        set -l network_qualities $network_state (test $network_autostart = 'yes'; and echo 'autostart') (test $network_persistent = 'yes'; and echo 'persistent')
        set -l show true
        if set -q argv[1]
            for filter in $argv
                if not string match -q -- $filter $network_qualities
                    set show false
                    continue
                end
            end
        end

        if not test $show = true
            continue
        end

        printf '%s\tNetwork (%s)\n' $network_name (string join ' ' $network_qualities)
    end
end

# virsh
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -x
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l connect -s -x -d "Specify hypervisor connection URI"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l debug -s d -x -a "0\t 1\t 2\t 3\t 4\t" -d "Specify debug level (0-4)"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l help -s h -d "Show help and exit"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l keepalive-interval -s k -x -a "0\tDisable\ keepalive" -d "Set keepalive interval (secs)"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l keepalive-count -s K -x -d "Set number of possible missed keepalive messages"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l log -s l -r -d "Output logging to file"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l quiet -s q -d "Quiet mode"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l readonly -s r -d "Connect readonly"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l timing -s t -d "Show timing information"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -s v -d "Show short version and exit"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -s V -d "Show short version and exit"
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -l version -a "short\tShort\ version long\tLong\ version" -d "Show version"

# virsh attach-device
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a attach-device -d "Attach device from an XML file"
complete -c virsh -n "__fish_seen_subcommand_from attach-device" -l persistent -d "Make live change persistent"
complete -c virsh -n "__fish_seen_subcommand_from attach-device" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from attach-device" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from attach-device" -l current -d "Affect current domain"

# virsh attach-disk
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a attach-disk -d "Attach disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l targetbus -d "Target bus of disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l driver -d "Driver of disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l subdriver -d "Subdriver of disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l iothread -d "IOThread to be used by supported device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l cache -d "Cache mode of disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l io -d "IO policy of disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l type -d "Target device type"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l mode -d "Mode of device reading and writing"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l sourcetype -d "Type of source" -x -a "block file"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l serial -d "Serial of disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l wwn -d "Wwn of disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l rawio -d "Needs rawio capability"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l address -d "Address of disk device"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l multifunction -d "Use multifunction pci under specified address"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l print-xml -d "Print XML document rather than attach the disk"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l persistent -d "Make live change persistent"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from attach-disk" -l current -d "Affect current domain"

# virsh attach-interface
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a attach-interface -d "Attach network interface"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l target -d "Target network name"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l mac -d "MAC address"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l script -d "Script used to bridge network interface"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l model -d "Model type"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l inbound -d "Control domain's incoming traffics"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l outbound -d "Control domain's outgoing traffics"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l persistent -d "Make live change persistent"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l current -d "Affect current domain"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l print-xml -d "Print XML document rather than attach the interface"
complete -c virsh -n "__fish_seen_subcommand_from attach-interface" -l managed -d "Libvirt will automatically detach/attach the device from/to host"

# virsh autostart
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a autostart -d "Autostart a domain"
complete -c virsh -n "__fish_seen_subcommand_from autostart" -a '(__fish_virsh_get_domains)' -x
complete -c virsh -n "__fish_seen_subcommand_from autostart" -l disable -d "Disable autostarting"

# virsh blkdeviotune
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a blkdeviotune -d "Set or query a block device I/O tuning parameters"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l total-bytes-sec -d "Total throughput limit, as scaled integer (default bytes)"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l read-bytes-sec -d "Read throughput limit, as scaled integer (default bytes)"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l write-bytes-sec -d "Write throughput limit, as scaled integer (default bytes)"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l total-iops-sec -d "Total I/O operations limit per second"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l read-iops-sec -d "Read I/O operations limit per second"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l write-iops-sec -d "Write I/O operations limit per second"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l total-bytes-sec-max -d "Total max, as scaled integer (default bytes)"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l read-bytes-sec-max -d "Read max, as scaled integer (default bytes)"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l write-bytes-sec-max -d "Write max, as scaled integer (default bytes)"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l total-iops-sec-max -d "Total I/O operations max"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l read-iops-sec-max -d "Read I/O operations max"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l write-iops-sec-max -d "Write I/O operations max"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l size-iops-sec -d "I/O size in bytes"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l group-name -d "Group name to share I/O quota between multiple drives"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l total-bytes-sec-max-length -d "Duration in seconds to allow total max bytes"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l read-bytes-sec-max-length -d "Duration in seconds to allow read max bytes"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l write-bytes-sec-max-length -d "Duration in seconds to allow write max bytes"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l total-iops-sec-max-length -d "Duration in seconds to allow total I/O operations max"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l read-iops-sec-max-length -d "Duration in seconds to allow read I/O operations max"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l write-iops-sec-max-length -d "Duration in seconds to allow write I/O operations max"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from blkdeviotune" -l current -d "Affect current domain"

# virsh blkiotune
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a blkiotune -d "Get or set blkio parameters"
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l weight -d "IO Weight"
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l device-weights -d "Per-device IO Weights, in the form of /path/to/device,weight,..."
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l device-read-iops-sec -d "Per-device read I/O limit per second, in the form of /path/to/device,read_iops_sec,..."
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l device-write-iops-sec -d "Per-device write I/O limit per second, in the form of /path/to/device,write_iops_sec,..."
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l device-read-bytes-sec -d "Per-device bytes read per second, in the form of /path/to/device,read_bytes_sec,..."
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l device-write-bytes-sec -d "Per-device bytes wrote per second, in the form of /path/to/device,write_bytes_sec,..."
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from blkiotune" -l current -d "Affect current domain"

# virsh blockcommit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a blockcommit -d "Start a block commit operation"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l bandwidth -d "Bandwidth limit in MiB/s"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l base -d "Path of base file to commit into (default bottom of chain)"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l shallow -d "Use backing file of top as base"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l top -d "Path of top file to commit from (default top of chain)"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l active -d "Trigger two-stage active commit of top file"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l delete -d "Delete files that were successfully committed"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l wait -d "Wait for job to complete (with --active, wait for job to sync)"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l verbose -d "With --wait, display the progress"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l timeout -d "Implies --wait, abort if copy exceeds timeout (in seconds)"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l pivot -d "Implies --active --wait, pivot when commit is synced"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l keep-overlay -d "Implies --active --wait, quit when commit is synced"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l async -d "With --wait, don't wait for cancel to finish"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l keep-relative -d "Keep the backing chain relatively referenced"
complete -c virsh -n "__fish_seen_subcommand_from blockcommit" -l bytes -d "The bandwidth limit is in bytes/s rather than MiB/s"

# virsh blockcopy
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a blockcopy -d "Start a block copy operation"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l dest -d "Path of the copy to create"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l bandwidth -d "Bandwidth limit in MiB/s"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l shallow -d "Make the copy share a backing chain"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l reuse-external -d "Reuse existing destination"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l blockdev -d "Copy destination is block device instead of regular file"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l wait -d "Wait for job to reach mirroring phase"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l verbose -d "With --wait, display the progress"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l timeout -d "Implies --wait, abort if copy exceeds timeout (in seconds)"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l pivot -d "Implies --wait, pivot when mirroring starts"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l finish -d "Implies --wait, quit when mirroring starts"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l async -d "With --wait, don't wait for cancel to finish"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l xml -d "Filename containing XML description of the copy destination"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l format -d "Format of the destination file"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l granularity -d "Power-of-two granularity to use during the copy"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l buf-size -d "Maximum amount of in-flight data during the copy"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l bytes -d "The bandwidth limit is in bytes/s rather than MiB/s"
complete -c virsh -n "__fish_seen_subcommand_from blockcopy" -l transient-job -d "The copy job is not persisted if VM is turned off"

# virsh blockjob
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a blockjob -d "Manage active block operations"
complete -c virsh -n "__fish_seen_subcommand_from blockjob" -l abort -d "Abort the active job on the specified disk"
complete -c virsh -n "__fish_seen_subcommand_from blockjob" -l async -d "Implies --abort; request but don't wait for job end"
complete -c virsh -n "__fish_seen_subcommand_from blockjob" -l pivot -d "Implies --abort; conclude and pivot a copy or commit job"
complete -c virsh -n "__fish_seen_subcommand_from blockjob" -l info -d "Get active job information for the specified disk"
complete -c virsh -n "__fish_seen_subcommand_from blockjob" -l bytes -d "Get/set bandwidth in bytes rather than MiB/s"
complete -c virsh -n "__fish_seen_subcommand_from blockjob" -l raw -d "Implies --info; output details rather than human summary"
complete -c virsh -n "__fish_seen_subcommand_from blockjob" -l bandwidth -d "Set the bandwidth limit in MiB/s"

# virsh blockpull
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a blockpull -d "Populate a disk from its backing image"
complete -c virsh -n "__fish_seen_subcommand_from blockpull" -l bandwidth -d "Bandwidth limit in MiB/s"
complete -c virsh -n "__fish_seen_subcommand_from blockpull" -l base -d "Path of backing file in chain for a partial pull"
complete -c virsh -n "__fish_seen_subcommand_from blockpull" -l wait -d "Wait for job to finish"
complete -c virsh -n "__fish_seen_subcommand_from blockpull" -l verbose -d "With --wait, display the progress"
complete -c virsh -n "__fish_seen_subcommand_from blockpull" -l timeout -d "With --wait, abort if pull exceeds timeout (in seconds)"
complete -c virsh -n "__fish_seen_subcommand_from blockpull" -l async -d "With --wait, don't wait for cancel to finish"
complete -c virsh -n "__fish_seen_subcommand_from blockpull" -l keep-relative -d "Keep the backing chain relatively referenced"
complete -c virsh -n "__fish_seen_subcommand_from blockpull" -l bytes -d "The bandwidth limit is in bytes/s rather than MiB/s"

# virsh blockresize
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a blockresize -d "Resize block device of domain"

# virsh change-media
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a change-media -d "Change media of CD or floppy drive"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l source -d "Source of the media"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l eject -d "Eject the media"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l insert -d "Insert the media"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l update -d "Update the media"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l current -d "Alter live or persistent configuration of domain depending on hypervisor driver"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l live -d "Alter live configuration of running domain"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l config -d "Alter persistent configuration, effect observed on next boot"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l force -d "Force media changing"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l print-xml -d "Print XML document rather than change media"
complete -c virsh -n "__fish_seen_subcommand_from change-media" -l block -d "Source media is a block device"

# virsh console
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a console -d "Connect to the guest console"
complete -c virsh -n "__fish_seen_subcommand_from console" -a '(__fish_virsh_get_domains running)' -x
complete -c virsh -n "__fish_seen_subcommand_from console" -l devname -d "Character device name"
complete -c virsh -n "__fish_seen_subcommand_from console" -l force -d "Force console connection (disconnect already connected sessions)"
complete -c virsh -n "__fish_seen_subcommand_from console" -l safe -d "Only connect if safe console handling is supported"

# virsh cpu-baseline
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a cpu-baseline -d "Compute baseline CPU"
complete -c virsh -n "__fish_seen_subcommand_from cpu-baseline" -l features -d "Show features that are part of the CPU model type"
complete -c virsh -n "__fish_seen_subcommand_from cpu-baseline" -l migratable -d "Do not include features that block migration"

# virsh cpu-compare
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a cpu-compare -d "Compare host CPU with a CPU described by an XML file"
complete -c virsh -n "__fish_seen_subcommand_from cpu-compare" -l error -d "Report error if CPUs are incompatible"

# virsh cpu-stats
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a cpu-stats -d "Show domain cpu statistics"
complete -c virsh -n "__fish_seen_subcommand_from cpu-stats" -l total -d "Show total statistics only"
complete -c virsh -n "__fish_seen_subcommand_from cpu-stats" -l start -d "Show statistics from this CPU"
complete -c virsh -n "__fish_seen_subcommand_from cpu-stats" -l count -d "Number of shown CPUs at most"

# virsh create
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a create -d "Create a domain from an XML file"
complete -c virsh -n "__fish_seen_subcommand_from create" -l console -d "Attach to console after creation"
complete -c virsh -n "__fish_seen_subcommand_from create" -l paused -d "Leave the guest paused after creation"
complete -c virsh -n "__fish_seen_subcommand_from create" -l autodestroy -d "Automatically destroy the guest when virsh disconnects"
complete -c virsh -n "__fish_seen_subcommand_from create" -l pass-fds -d "Pass file descriptors N,M,... to the guest"
complete -c virsh -n "__fish_seen_subcommand_from create" -l validate -d "Validate the XML against the schema"

# virsh define
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a define -d "Define (but don't start) a domain"
complete -c virsh -n "__fish_seen_subcommand_from define" -l validate -d "Validate the XML against the schema"

# virsh desc
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a desc -d "Show or set domain's description or title"
complete -c virsh -n "__fish_seen_subcommand_from desc" -l live -d "Modify/get running state"
complete -c virsh -n "__fish_seen_subcommand_from desc" -l config -d "Modify/get persistent configuration"
complete -c virsh -n "__fish_seen_subcommand_from desc" -l current -d "Modify/get current state configuration"
complete -c virsh -n "__fish_seen_subcommand_from desc" -l title -d "Modify/get the title instead of description"
complete -c virsh -n "__fish_seen_subcommand_from desc" -l edit -d "Open an editor to modify the description"

# virsh destroy
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a destroy -d "Destroy (stop) a domain"
complete -c virsh -n "__fish_seen_subcommand_from destroy" -l graceful -d "Terminate gracefully"

# virsh detach-device
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a detach-device -d "Detach device from an XML file"
complete -c virsh -n "__fish_seen_subcommand_from detach-device" -l persistent -d "Make live change persistent"
complete -c virsh -n "__fish_seen_subcommand_from detach-device" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from detach-device" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from detach-device" -l current -d "Affect current domain"

# virsh detach-disk
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a detach-disk -d "Detach disk device"
complete -c virsh -n "__fish_seen_subcommand_from detach-disk" -l persistent -d "Make live change persistent"
complete -c virsh -n "__fish_seen_subcommand_from detach-disk" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from detach-disk" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from detach-disk" -l current -d "Affect current domain"

# virsh detach-interface
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a detach-interface -d "Detach network interface"
complete -c virsh -n "__fish_seen_subcommand_from detach-interface" -l mac -d "MAC address"
complete -c virsh -n "__fish_seen_subcommand_from detach-interface" -l persistent -d "Make live change persistent"
complete -c virsh -n "__fish_seen_subcommand_from detach-interface" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from detach-interface" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from detach-interface" -l current -d "Affect current domain"

# virsh domdisplay
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domdisplay -d "Domain display connection URI"
complete -c virsh -n "__fish_seen_subcommand_from domdisplay" -l include-password -d "Includes the password into the connection URI if available"
complete -c virsh -n "__fish_seen_subcommand_from domdisplay" -l type -d "Select particular graphical display (e.g. "vnc", "spice", "rdp")"
complete -c virsh -n "__fish_seen_subcommand_from domdisplay" -l all -d "Show all possible graphical displays"

# virsh domfsfreeze
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domfsfreeze -d "Freeze domain's mounted filesystems"

# virsh domfsthaw
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domfsthaw -d "Thaw domain's mounted filesystems"

# virsh domfsinfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domfsinfo -d "Get information of domain's mounted filesystems"

# virsh domfstrim
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domfstrim -d "Invoke fstrim on domain's mounted filesystems"
complete -c virsh -n "__fish_seen_subcommand_from domfstrim" -l minimum -d "Just a hint to ignore contiguous free ranges smaller than this (Bytes)"
complete -c virsh -n "__fish_seen_subcommand_from domfstrim" -l mountpoint -d "Which mount point to trim"

# virsh domhostname
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domhostname -d "Print the domain's hostname"

# virsh domid
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domid -d "Convert a domain name or UUID to domain id"

# virsh domif-setlink
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domif-setlink -d "Set link state of a virtual interface"
complete -c virsh -n "__fish_seen_subcommand_from domif-setlink" -l config -d "Affect next boot"

# virsh domiftune
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domiftune -d "Get/set parameters of a virtual interface"
complete -c virsh -n "__fish_seen_subcommand_from domiftune" -l inbound -d "Control domain's incoming traffics"
complete -c virsh -n "__fish_seen_subcommand_from domiftune" -l outbound -d "Control domain's outgoing traffics"
complete -c virsh -n "__fish_seen_subcommand_from domiftune" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from domiftune" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from domiftune" -l current -d "Affect current domain"

# virsh domjobabort
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domjobabort -d "Abort active domain job"

# virsh domjobinfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domjobinfo -d "Domain job information"
complete -c virsh -n "__fish_seen_subcommand_from domjobinfo" -l completed -d "Return statistics of a recently completed job"

# virsh domname
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domname -d "Convert a domain id or UUID to domain name"

# virsh domrename
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domrename -d "Rename a domain"
complete -c virsh -n "__fish_seen_subcommand_from domrename" -a '(__fish_virsh_get_domains)' -x

# virsh dompmsuspend
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a dompmsuspend -d "Suspend a domain gracefully using power management functions"
complete -c virsh -n "__fish_seen_subcommand_from dompmsuspend" -l duration -d "Duration in seconds"
complete -c virsh -n "__fish_seen_subcommand_from dompmsuspend" -a '(__fish_virsh_get_domains running)' -x

# virsh dompmwakeup
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a dompmwakeup -d "Wakeup a domain from pmsuspended state"
complete -c virsh -n "__fish_seen_subcommand_from dompmwakeup" -a '(__fish_virsh_get_domains paused)' -x

# virsh domuuid
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domuuid -d "Convert a domain name or id to domain UUID"
complete -c virsh -n "__fish_seen_subcommand_from domuuid" -a '(__fish_virsh_get_domains)' -x

# virsh domxml-from-native
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domxml-from-native -d "Convert native config to domain XML"

# virsh domxml-to-native
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domxml-to-native -d "Convert domain XML to native config"
complete -c virsh -n "__fish_seen_subcommand_from domxml-to-native" -l domain -d "Domain name, id or UUID"
complete -c virsh -n "__fish_seen_subcommand_from domxml-to-native" -l xml -d "Xml data file to export from"

# virsh dump
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a dump -d "Dump the core of a domain to a file for analysis"
complete -c virsh -n "__fish_seen_subcommand_from dump" -l live -d "Perform a live core dump if supported"
complete -c virsh -n "__fish_seen_subcommand_from dump" -l crash -d "Crash the domain after core dump"
complete -c virsh -n "__fish_seen_subcommand_from dump" -l bypass-cache -d "Avoid file system cache when dumping"
complete -c virsh -n "__fish_seen_subcommand_from dump" -l reset -d "Reset the domain after core dump"
complete -c virsh -n "__fish_seen_subcommand_from dump" -l verbose -d "Display the progress of dump"
complete -c virsh -n "__fish_seen_subcommand_from dump" -l memory-only -d "Dump domain's memory only"
complete -c virsh -n "__fish_seen_subcommand_from dump" -l format -d "Specify the format of memory-only dump"

# virsh dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a dumpxml -d "Domain information in XML"
complete -c virsh -n "__fish_seen_subcommand_from dumpxml" -a '(__fish_virsh_get_domains)' -x
complete -c virsh -n "__fish_seen_subcommand_from dumpxml" -l inactive -d "Show inactive defined XML"
complete -c virsh -n "__fish_seen_subcommand_from dumpxml" -l security-info -d "Include security sensitive information in XML dump"
complete -c virsh -n "__fish_seen_subcommand_from dumpxml" -l update-cpu -d "Update guest CPU according to host CPU"
complete -c virsh -n "__fish_seen_subcommand_from dumpxml" -l migratable -d "Provide XML suitable for migrations"

# virsh edit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a edit -d "Edit XML configuration for a domain"
complete -c virsh -n "__fish_seen_subcommand_from edit" -a '(__fish_virsh_get_domains)' -x
complete -c virsh -n "__fish_seen_subcommand_from edit" -l skip-validate -d "Skip validation of the XML against the schema"

# virsh event
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a event -d "Domain Events"
complete -c virsh -n "__fish_seen_subcommand_from event" -l domain -d "Filter by domain name, id, or UUID"
complete -c virsh -n "__fish_seen_subcommand_from event" -l event -d "Which event type to wait for"
complete -c virsh -n "__fish_seen_subcommand_from event" -l all -d "Wait for all events instead of just one type"
complete -c virsh -n "__fish_seen_subcommand_from event" -l loop -d "Loop until timeout or interrupt, rather than one-shot"
complete -c virsh -n "__fish_seen_subcommand_from event" -l timeout -d "Timeout seconds"
complete -c virsh -n "__fish_seen_subcommand_from event" -l list -d "List valid event types"
complete -c virsh -n "__fish_seen_subcommand_from event" -l timestamp -d "Show timestamp for each printed event"

# virsh inject-nmi
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a inject-nmi -d "Inject NMI to the guest"

# virsh iothreadinfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iothreadinfo -d "View domain IOThreads"
complete -c virsh -n "__fish_seen_subcommand_from iothreadinfo" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from iothreadinfo" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from iothreadinfo" -l current -d "Affect current domain"

# virsh iothreadpin
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iothreadpin -d "Control domain IOThread affinity"
complete -c virsh -n "__fish_seen_subcommand_from iothreadpin" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from iothreadpin" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from iothreadpin" -l current -d "Affect current domain"

# virsh iothreadadd
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iothreadadd -d "Add an IOThread to the guest domain"
complete -c virsh -n "__fish_seen_subcommand_from iothreadadd" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from iothreadadd" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from iothreadadd" -l current -d "Affect current domain"

# virsh iothreaddel
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iothreaddel -d "Delete an IOThread from the guest domain"
complete -c virsh -n "__fish_seen_subcommand_from iothreaddel" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from iothreaddel" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from iothreaddel" -l current -d "Affect current domain"

# virsh send-key
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a send-key -d "Send keycodes to the guest"
complete -c virsh -n "__fish_seen_subcommand_from send-key" -l codeset -d "The codeset of keycodes, default:linux"
complete -c virsh -n "__fish_seen_subcommand_from send-key" -l holdtime -d "The time (in milliseconds) how long the keys will be held"

# virsh send-process-signal
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a send-process-signal -d "Send signals to processes"

# virsh lxc-enter-namespace
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a lxc-enter-namespace -d "LXC Guest Enter Namespace"
complete -c virsh -n "__fish_seen_subcommand_from lxc-enter-namespace" -l noseclabel -d "Do not change process security label"

# virsh managedsave
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a managedsave -d "Managed save of a domain state"
complete -c virsh -n "__fish_seen_subcommand_from managedsave" -l bypass-cache -d "Avoid file system cache when saving"
complete -c virsh -n "__fish_seen_subcommand_from managedsave" -l running -d "Set domain to be running on next start"
complete -c virsh -n "__fish_seen_subcommand_from managedsave" -l paused -d "Set domain to be paused on next start"
complete -c virsh -n "__fish_seen_subcommand_from managedsave" -l verbose -d "Display the progress of save"

# virsh managedsave-remove
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a managedsave-remove -d "Remove managed save of a domain"

# virsh managedsave-edit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a managedsave-edit -d "Edit XML for a domain's managed save state file"
complete -c virsh -n "__fish_seen_subcommand_from managedsave-edit" -l running -d "Set domain to be running on start"
complete -c virsh -n "__fish_seen_subcommand_from managedsave-edit" -l paused -d "Set domain to be paused on start"

# virsh managedsave-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a managedsave-dumpxml -d "Domain information of managed save state file in XML"
complete -c virsh -n "__fish_seen_subcommand_from managedsave-dumpxml" -l security-info -d "Include security sensitive information in XML dump"

# virsh managedsave-define
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a managedsave-define -d "Redefine the XML for a domain's managed save state file"
complete -c virsh -n "__fish_seen_subcommand_from managedsave-define" -l running -d "Set domain to be running on start"
complete -c virsh -n "__fish_seen_subcommand_from managedsave-define" -l paused -d "Set domain to be paused on start"

# virsh memtune
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a memtune -d "Get or set memory parameters"
complete -c virsh -n "__fish_seen_subcommand_from memtune" -l hard-limit -d "Max memory, as scaled integer (default KiB)"
complete -c virsh -n "__fish_seen_subcommand_from memtune" -l soft-limit -d "Memory during contention, as scaled integer (default KiB)"
complete -c virsh -n "__fish_seen_subcommand_from memtune" -l swap-hard-limit -d "Max memory plus swap, as scaled integer (default KiB)"
complete -c virsh -n "__fish_seen_subcommand_from memtune" -l min-guarantee -d "Min guaranteed memory, as scaled integer (default KiB)"
complete -c virsh -n "__fish_seen_subcommand_from memtune" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from memtune" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from memtune" -l current -d "Affect current domain"

# virsh perf
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a perf -d "Get or set perf event"
complete -c virsh -n "__fish_seen_subcommand_from perf" -l enable -d "Perf events which will be enabled"
complete -c virsh -n "__fish_seen_subcommand_from perf" -l disable -d "Perf events which will be disabled"
complete -c virsh -n "__fish_seen_subcommand_from perf" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from perf" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from perf" -l current -d "Affect current domain"

# virsh metadata
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a metadata -d "Show or set domain's custom XML metadata"
complete -c virsh -n "__fish_seen_subcommand_from metadata" -l live -d "Modify/get running state"
complete -c virsh -n "__fish_seen_subcommand_from metadata" -l config -d "Modify/get persistent configuration"
complete -c virsh -n "__fish_seen_subcommand_from metadata" -l current -d "Modify/get current state configuration"
complete -c virsh -n "__fish_seen_subcommand_from metadata" -l edit -d "Use an editor to change the metadata"
complete -c virsh -n "__fish_seen_subcommand_from metadata" -l key -d "Key to be used as a namespace identifier"
complete -c virsh -n "__fish_seen_subcommand_from metadata" -l set -d "New metadata to set"
complete -c virsh -n "__fish_seen_subcommand_from metadata" -l remove -d "Remove the metadata corresponding to an uri"

# virsh migrate
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a migrate -d "Migrate domain to another host"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l live -d "Live migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l offline -d "Offline migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l p2p -d "Peer-2-peer migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l direct -d "Direct migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l tunnelled -d "Tunnelled migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l persistent -d "Persist VM on destination"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l undefinesource -d "Undefine VM on source"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l suspend -d "Do not restart the domain on the destination host"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l copy-storage-all -d "Migration with non-shared storage with full disk copy"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l copy-storage-inc -d "Migration with non-shared storage with incremental copy"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l change-protection -d "Prevent any configuration changes to domain until migration ends"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l unsafe -d "Force migration even if it may be unsafe"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l verbose -d "Display the progress of migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l compressed -d "Compress repeated pages during live migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l auto-converge -d "Force convergence during live migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l rdma-pin-all -d "Pin all memory before starting RDMA live migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l abort-on-error -d "Abort on soft errors during migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l postcopy -d "Enable post-copy migration; switch to it using migrate-postcopy command"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l postcopy-after-precopy -d "Automatically switch to post-copy migration after one pass of pre-copy"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l migrateuri -d "Migration URI, usually can be omitted"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l graphicsuri -d "Graphics URI to be used for seamless graphics migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l listen-address -d "Listen address that destination should bind to for incoming migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l dname -d "Rename to new name during migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l timeout -d "Run action specified by --timeout-* if live migration exceeds timeout"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l timeout-suspend -d "Suspend the guest after timeout"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l timeout-postcopy -d "Switch to post-copy after timeout"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l xml -d "Filename containing updated XML for the target"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l migrate-disks -d "Comma separated list of disks to be migrated"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l disks-port -d "Port to use by target server for incoming disks migration"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l comp-methods -d "Comma separated list of compression methods to be used"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l comp-mt-level -d "Compress level for multithread compression"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l comp-mt-threads -d "Number of compression threads for multithread compression"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l comp-mt-dthreads -d "Number of decompression threads for multithread compression"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l comp-xbzrle-cache -d "Page cache size for xbzrle compression"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l auto-converge-initial -d "Initial CPU throttling rate for auto-convergence"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l auto-converge-increment -d "CPU throttling rate increment for auto-convergence"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l persistent-xml -d "Filename containing updated persistent XML for the target"
complete -c virsh -n "__fish_seen_subcommand_from migrate" -l tls -d "Use TLS for migration"

# virsh migrate-setmaxdowntime
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a migrate-setmaxdowntime -d "Set maximum tolerable downtime"

# virsh migrate-getmaxdowntime
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a migrate-getmaxdowntime -d "Get maximum tolerable downtime"

# virsh migrate-compcache
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a migrate-compcache -d "Get/set compression cache size"
complete -c virsh -n "__fish_seen_subcommand_from migrate-compcache" -l size -d "Requested size of the cache (in bytes) used for compression"

# virsh migrate-setspeed
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a migrate-setspeed -d "Set the maximum migration bandwidth"

# virsh migrate-getspeed
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a migrate-getspeed -d "Get the maximum migration bandwidth"

# virsh migrate-postcopy
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a migrate-postcopy -d "Switch running migration from pre-copy to post-copy"

# virsh numatune
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a numatune -d "Get or set numa parameters"
complete -c virsh -n "__fish_seen_subcommand_from numatune" -l mode -d "NUMA mode, one of strict, preferred and interleave"
complete -c virsh -n "__fish_seen_subcommand_from numatune" -l nodeset -d "NUMA node selections to set"
complete -c virsh -n "__fish_seen_subcommand_from numatune" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from numatune" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from numatune" -l current -d "Affect current domain"

# virsh qemu-attach
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a qemu-attach -d "QEMU Attach"

# virsh qemu-monitor-command
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a qemu-monitor-command -d "QEMU Monitor Command"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-command" -l hmp -d "Command is in human monitor protocol"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-command" -l pretty -d "Pretty-print any qemu monitor protocol output"

# virsh qemu-monitor-event
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a qemu-monitor-event -d "QEMU Monitor Events"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-event" -l domain -d "Filter by domain name, id or UUID"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-event" -l event -d "Filter by event name"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-event" -l pretty -d "Pretty-print any JSON output"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-event" -l loop -d "Loop until timeout or interrupt, rather than one-shot"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-event" -l timeout -d "Timeout seconds"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-event" -l regex -d "Treat event as a regex rather than literal filter"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-event" -l no-case -d "Treat event case-insensitively"
complete -c virsh -n "__fish_seen_subcommand_from qemu-monitor-event" -l timestamp -d "Show timestamp for each printed event"

# virsh qemu-agent-command
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a qemu-agent-command -d "QEMU Guest Agent Command"
complete -c virsh -n "__fish_seen_subcommand_from qemu-agent-command" -l timeout -d "Timeout seconds. must be positive"
complete -c virsh -n "__fish_seen_subcommand_from qemu-agent-command" -l async -d "Execute command without waiting for timeout"
complete -c virsh -n "__fish_seen_subcommand_from qemu-agent-command" -l block -d "Execute command without timeout"
complete -c virsh -n "__fish_seen_subcommand_from qemu-agent-command" -l pretty -d "Pretty-print the output"

# virsh reboot
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a reboot -d "Reboot a domain"
complete -c virsh -n "__fish_seen_subcommand_from reboot" -l mode -d "Shutdown mode" -x -a "acpi agent initctl signal paravirt"
complete -c virsh -n "__fish_seen_subcommand_from reboot" -a '(__fish_virsh_get_domains running)' -x

# virsh reset
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a reset -d "Reset a domain"

# virsh restore
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a restore -d "Restore a domain from a saved state in a file"
complete -c virsh -n "__fish_seen_subcommand_from restore" -l bypass-cache -d "Avoid file system cache when restoring"
complete -c virsh -n "__fish_seen_subcommand_from restore" -l xml -d "Filename containing updated XML for the target"
complete -c virsh -n "__fish_seen_subcommand_from restore" -l running -d "Restore domain into running state"
complete -c virsh -n "__fish_seen_subcommand_from restore" -l paused -d "Restore domain into paused state"

# virsh resume
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a resume -d "Resume a domain"
complete -c virsh -n "__fish_seen_subcommand_from resume" -a '(__fish_virsh_get_domains paused)' -x

# virsh save
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a save -d "Save a domain state to a file"
complete -c virsh -n "__fish_seen_subcommand_from save" -a '(__fish_virsh_get_domains running)' -x
complete -c virsh -n "__fish_seen_subcommand_from save" -l bypass-cache -d "Avoid file system cache when saving"
complete -c virsh -n "__fish_seen_subcommand_from save" -l xml -d "Filename containing updated XML for the target"
complete -c virsh -n "__fish_seen_subcommand_from save" -l running -d "Set domain to be running on restore"
complete -c virsh -n "__fish_seen_subcommand_from save" -l paused -d "Set domain to be paused on restore"
complete -c virsh -n "__fish_seen_subcommand_from save" -l verbose -d "Display the progress of save"

# virsh save-image-define
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a save-image-define -d "Redefine the XML for a domain's saved state file"
complete -c virsh -n "__fish_seen_subcommand_from save-image-define" -l running -d "Set domain to be running on restore"
complete -c virsh -n "__fish_seen_subcommand_from save-image-define" -l paused -d "Set domain to be paused on restore"

# virsh save-image-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a save-image-dumpxml -d "Saved state domain information in XML"
complete -c virsh -n "__fish_seen_subcommand_from save-image-dumpxml" -l security-info -d "Include security sensitive information in XML dump"

# virsh save-image-edit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a save-image-edit -d "Edit XML for a domain's saved state file"
complete -c virsh -n "__fish_seen_subcommand_from save-image-edit" -l running -d "Set domain to be running on restore"
complete -c virsh -n "__fish_seen_subcommand_from save-image-edit" -l paused -d "Set domain to be paused on restore"

# virsh schedinfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a schedinfo -d "Show/set scheduler parameters"
complete -c virsh -n "__fish_seen_subcommand_from shechinfo" -a '(__fish_virsh_get_domains)' -x
complete -c virsh -n "__fish_seen_subcommand_from schedinfo" -l weight -d "Weight for XEN_CREDIT"
complete -c virsh -n "__fish_seen_subcommand_from schedinfo" -l cap -d "Cap for XEN_CREDIT"
complete -c virsh -n "__fish_seen_subcommand_from schedinfo" -l current -d "Get/set current scheduler info"
complete -c virsh -n "__fish_seen_subcommand_from schedinfo" -l config -d "Get/set value to be used on next boot"
complete -c virsh -n "__fish_seen_subcommand_from schedinfo" -l live -d "Get/set value from running domain"

# virsh screenshot
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a screenshot -d "Take a screenshot of a current domain console and store it into a file"
complete -c virsh -n "__fish_seen_subcommand_from screenshot" -l file -d "Where to store the screenshot"
complete -c virsh -n "__fish_seen_subcommand_from screenshot" -l screen -d "ID of a screen to take screenshot of"

# virsh set-lifecycle-action
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a set-lifecycle-action -d "Change lifecycle actions"
complete -c virsh -n "__fish_seen_subcommand_from set-lifecycle-action" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from set-lifecycle-action" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from set-lifecycle-action" -l current -d "Affect current domain"

# virsh set-user-password
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a set-user-password -d "Set the user password inside the domain"
complete -c virsh -n "__fish_seen_subcommand_from set-user-password" -l encrypted -d "The password is already encrypted"

# virsh setmaxmem
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a setmaxmem -d "Change maximum memory limit"
complete -c virsh -n "__fish_seen_subcommand_from setmaxmem" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from setmaxmem" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from setmaxmem" -l current -d "Affect current domain"

# virsh setmem
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a setmem -d "Change memory allocation"
complete -c virsh -n "__fish_seen_subcommand_from setmem" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from setmem" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from setmem" -l current -d "Affect current domain"

# virsh setvcpus
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a setvcpus -d "Change number of virtual CPUs"
complete -c virsh -n "__fish_seen_subcommand_from setvcpus" -l maximum -d "Set maximum limit on next boot"
complete -c virsh -n "__fish_seen_subcommand_from setvcpus" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from setvcpus" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from setvcpus" -l current -d "Affect current domain"
complete -c virsh -n "__fish_seen_subcommand_from setvcpus" -l guest -d "Modify cpu state in the guest"
complete -c virsh -n "__fish_seen_subcommand_from setvcpus" -l hotpluggable -d "Make added vcpus hot(un)pluggable"

# virsh shutdown
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a shutdown -d "Gracefully shutdown a domain"
complete -c virsh -n "__fish_seen_subcommand_from shutdown" -a '(__fish_virsh_get_domains running)' -x
complete -c virsh -n "__fish_seen_subcommand_from shutdown" -l mode -d "Shutdown mode" -x -a "acpi agent initctl signal paravirt"

# virsh start
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a start -d "Start a (previously defined) inactive domain"
complete -c virsh -n "__fish_seen_subcommand_from start" -a '(__fish_virsh_get_domains inactive)' -x
complete -c virsh -n "__fish_seen_subcommand_from start" -l console -d "Attach to console after creation"
complete -c virsh -n "__fish_seen_subcommand_from start" -l paused -d "Leave the guest paused after creation"
complete -c virsh -n "__fish_seen_subcommand_from start" -l autodestroy -d "Automatically destroy the guest when virsh disconnects"
complete -c virsh -n "__fish_seen_subcommand_from start" -l bypass-cache -d "Avoid file system cache when loading"
complete -c virsh -n "__fish_seen_subcommand_from start" -l force-boot -d "Force fresh boot by discarding any managed save"
complete -c virsh -n "__fish_seen_subcommand_from start" -l pass-fds -d "Pass file descriptors N,M,... to the guest"

# virsh suspend
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a suspend -d "Suspend a domain"
complete -c virsh -n "__fish_seen_subcommand_from suspend" -a '(__fish_virsh_get_domains running)' -x

# virsh ttyconsole
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a ttyconsole -d "TTY console"

# virsh undefine
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a undefine -d "Undefine a domain"
complete -c virsh -n "__fish_seen_subcommand_from undefine" -l managed-save -d "Remove domain managed state file"
complete -c virsh -n "__fish_seen_subcommand_from undefine" -l storage -d "Remove associated storage volumes"
complete -c virsh -n "__fish_seen_subcommand_from undefine" -l remove-all-storage -d "Remove all associated storage volumes"
complete -c virsh -n "__fish_seen_subcommand_from undefine" -l delete-snapshots -d "Delete snapshots associated with volume(s)"
complete -c virsh -n "__fish_seen_subcommand_from undefine" -l wipe-storage -d "Wipe data on the removed volumes"
complete -c virsh -n "__fish_seen_subcommand_from undefine" -l snapshots-metadata -d "Remove all domain snapshot metadata, if inactive"
complete -c virsh -n "__fish_seen_subcommand_from undefine" -l nvram -d "Remove nvram file, if inactive"
complete -c virsh -n "__fish_seen_subcommand_from undefine" -l keep-nvram -d "Keep nvram file, if inactive"

# virsh update-device
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a update-device -d "Update device from an XML file"
complete -c virsh -n "__fish_seen_subcommand_from update-device" -l persistent -d "Make live change persistent"
complete -c virsh -n "__fish_seen_subcommand_from update-device" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from update-device" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from update-device" -l current -d "Affect current domain"
complete -c virsh -n "__fish_seen_subcommand_from update-device" -l force -d "Force device update"

# virsh vcpucount
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vcpucount -d "Domain vcpu counts"
complete -c virsh -n "__fish_seen_subcommand_from vcpucount" -l maximum -d "Get maximum count of vcpus"
complete -c virsh -n "__fish_seen_subcommand_from vcpucount" -l active -d "Get number of currently active vcpus"
complete -c virsh -n "__fish_seen_subcommand_from vcpucount" -l live -d "Get value from running domain"
complete -c virsh -n "__fish_seen_subcommand_from vcpucount" -l config -d "Get value to be used on next boot"
complete -c virsh -n "__fish_seen_subcommand_from vcpucount" -l current -d "Get value according to current domain state"
complete -c virsh -n "__fish_seen_subcommand_from vcpucount" -l guest -d "Retrieve vcpu count from the guest instead of the hypervisor"

# virsh vcpuinfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vcpuinfo -d "Detailed domain vcpu information"
complete -c virsh -n "__fish_seen_subcommand_from vcpuinfo" -l pretty -d "Return human readable output"

# virsh vcpupin
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vcpupin -d "Control or query domain vcpu affinity"
complete -c virsh -n "__fish_seen_subcommand_from vcpupin" -l vcpu -d "Vcpu number"
complete -c virsh -n "__fish_seen_subcommand_from vcpupin" -l cpulist -d "Host cpu number(s) to set, or omit option to query"
complete -c virsh -n "__fish_seen_subcommand_from vcpupin" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from vcpupin" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from vcpupin" -l current -d "Affect current domain"

# virsh emulatorpin
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a emulatorpin -d "Control or query domain emulator affinity"
complete -c virsh -n "__fish_seen_subcommand_from emulatorpin" -l cpulist -d "Host cpu number(s) to set, or omit option to query"
complete -c virsh -n "__fish_seen_subcommand_from emulatorpin" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from emulatorpin" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from emulatorpin" -l current -d "Affect current domain"

# virsh vncdisplay
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vncdisplay -d "Vnc display"

# virsh guestvcpus
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a guestvcpus -d "Query or modify state of vcpu in the guest (via agent)"
complete -c virsh -n "__fish_seen_subcommand_from guestvcpus" -l cpulist -d "List of cpus to enable or disable"
complete -c virsh -n "__fish_seen_subcommand_from guestvcpus" -l enable -d "Enable cpus specified by cpulist"
complete -c virsh -n "__fish_seen_subcommand_from guestvcpus" -l disable -d "Disable cpus specified by cpulist"

# virsh setvcpu
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a setvcpu -d "Attach/detach vcpu or groups of threads"
complete -c virsh -n "__fish_seen_subcommand_from setvcpu" -l enable -d "Enable cpus specified by cpumap"
complete -c virsh -n "__fish_seen_subcommand_from setvcpu" -l disable -d "Disable cpus specified by cpumap"
complete -c virsh -n "__fish_seen_subcommand_from setvcpu" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from setvcpu" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from setvcpu" -l current -d "Affect current domain"

# virsh domblkthreshold
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domblkthreshold -d "Set the threshold for block-threshold event"

# virsh domblkerror
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domblkerror -d "Show errors on block devices"

# virsh domblkinfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domblkinfo -d "Domain block device size information"
complete -c virsh -n "__fish_seen_subcommand_from domblkinfo" -l human -d "Human readable output"

# virsh domblklist
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domblklist -d "List all domain blocks"
complete -c virsh -n "__fish_seen_subcommand_from domblklist" -l inactive -d "Get inactive rather than running configuration"
complete -c virsh -n "__fish_seen_subcommand_from domblklist" -l details -d "Additionally display the type and device value"

# virsh domblkstat
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domblkstat -d "Get device block stats for a domain"
complete -c virsh -n "__fish_seen_subcommand_from domblkstat" -l device -d "Block device"
complete -c virsh -n "__fish_seen_subcommand_from domblkstat" -l human -d "Print a more human readable output"

# virsh domcontrol
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domcontrol -d "Domain control interface state"

# virsh domif-getlink
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domif-getlink -d "Get link state of a virtual interface"
complete -c virsh -n "__fish_seen_subcommand_from domif-getlink" -l config -d "Get persistent interface state"

# virsh domifaddr
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domifaddr -d "Get network interfaces' addresses for a running domain"
complete -c virsh -n "__fish_seen_subcommand_from domifaddr" -l interface -d "Network interface name"
complete -c virsh -n "__fish_seen_subcommand_from domifaddr" -l full -d "Always display names and MACs of interfaces"
complete -c virsh -n "__fish_seen_subcommand_from domifaddr" -l source -d "Address source: 'lease' or 'agent'"

# virsh domiflist
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domiflist -d "List all domain virtual interfaces"
complete -c virsh -n "__fish_seen_subcommand_from domiflist" -l inactive -d "Get inactive rather than running configuration"

# virsh domifstat
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domifstat -d "Get network interface stats for a domain"

# virsh dominfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a dominfo -d "Domain information"

# virsh dommemstat
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a dommemstat -d "Get memory statistics for a domain"
complete -c virsh -n "__fish_seen_subcommand_from dommemstat" -l period -d "Period in seconds to set collection"
complete -c virsh -n "__fish_seen_subcommand_from dommemstat" -l config -d "Affect next boot"
complete -c virsh -n "__fish_seen_subcommand_from dommemstat" -l live -d "Affect running domain"
complete -c virsh -n "__fish_seen_subcommand_from dommemstat" -l current -d "Affect current domain"

# virsh domstate
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domstate -d "Domain state"
complete -c virsh -n "__fish_seen_subcommand_from domstate" -l reason -d "Also print reason for the state"

# virsh domstats
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domstats -d "Get statistics about one or multiple domains"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l state -d "Report domain state"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l cpu-total -d "Report domain physical cpu usage"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l balloon -d "Report domain balloon statistics"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l vcpu -d "Report domain virtual cpu information"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l interface -d "Report domain network interface information"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l block -d "Report domain block device statistics"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l perf -d "Report domain perf event statistics"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l list-active -d "List only active domains"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l list-inactive -d "List only inactive domains"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l list-persistent -d "List only persistent domains"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l list-transient -d "List only transient domains"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l list-running -d "List only running domains"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l list-paused -d "List only paused domains"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l list-shutoff -d "List only shutoff domains"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l list-other -d "List only domains in other states"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l raw -d "Do not pretty-print the fields"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l enforce -d "Enforce requested stats parameters"
complete -c virsh -n "__fish_seen_subcommand_from domstats" -l backing -d "Add backing chain information to block stats"

# virsh domtime
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domtime -d "Domain time"
complete -c virsh -n "__fish_seen_subcommand_from domtime" -l now -d "Set to the time of the host running virsh"
complete -c virsh -n "__fish_seen_subcommand_from domtime" -l pretty -d "Print domain's time in human readable form"
complete -c virsh -n "__fish_seen_subcommand_from domtime" -l sync -d "Instead of setting given time, synchronize from domain's RTC"
complete -c virsh -n "__fish_seen_subcommand_from domtime" -l time -d "Time to set"

# virsh list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a list -d "List domains"
complete -c virsh -n "__fish_seen_subcommand_from list" -l inactive -d "List inactive domains"
complete -c virsh -n "__fish_seen_subcommand_from list" -l all -d "List inactive & active domains"
complete -c virsh -n "__fish_seen_subcommand_from list" -l transient -d "List transient domains"
complete -c virsh -n "__fish_seen_subcommand_from list" -l persistent -d "List persistent domains"
complete -c virsh -n "__fish_seen_subcommand_from list" -l with-snapshot -d "List domains with existing snapshot"
complete -c virsh -n "__fish_seen_subcommand_from list" -l without-snapshot -d "List domains without a snapshot"
complete -c virsh -n "__fish_seen_subcommand_from list" -l state-running -d "List domains in running state"
complete -c virsh -n "__fish_seen_subcommand_from list" -l state-paused -d "List domains in paused state"
complete -c virsh -n "__fish_seen_subcommand_from list" -l state-shutoff -d "List domains in shutoff state"
complete -c virsh -n "__fish_seen_subcommand_from list" -l state-other -d "List domains in other states"
complete -c virsh -n "__fish_seen_subcommand_from list" -l autostart -d "List domains with autostart enabled"
complete -c virsh -n "__fish_seen_subcommand_from list" -l no-autostart -d "List domains with autostart disabled"
complete -c virsh -n "__fish_seen_subcommand_from list" -l with-managed-save -d "List domains with managed save state"
complete -c virsh -n "__fish_seen_subcommand_from list" -l without-managed-save -d "List domains without managed save"
complete -c virsh -n "__fish_seen_subcommand_from list" -l uuid -d "List UUID's only"
complete -c virsh -n "__fish_seen_subcommand_from list" -l name -d "List domain names only"
complete -c virsh -n "__fish_seen_subcommand_from list" -l table -d "List table (default)"
complete -c virsh -n "__fish_seen_subcommand_from list" -l managed-save -d "Mark inactive domains with managed save state"
complete -c virsh -n "__fish_seen_subcommand_from list" -l title -d "Show domain title"

# virsh allocpages
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a allocpages -d "Manipulate pages pool size"
complete -c virsh -n "__fish_seen_subcommand_from allocpages" -l cellno -d "NUMA cell number"
complete -c virsh -n "__fish_seen_subcommand_from allocpages" -l add -d "Instead of setting new pool size add pages to it"
complete -c virsh -n "__fish_seen_subcommand_from allocpages" -l all -d "Set on all NUMA cells"

# virsh capabilities
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a capabilities -d Capabilities

# virsh cpu-models
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a cpu-models -d "CPU models"

# virsh domcapabilities
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a domcapabilities -d "Domain capabilities"
complete -c virsh -n "__fish_seen_subcommand_from domcapabilities" -l virttype -d "Virtualization type (/domain/@type)"
complete -c virsh -n "__fish_seen_subcommand_from domcapabilities" -l emulatorbin -d "Path to emulator binary (/domain/devices/emulator)"
complete -c virsh -n "__fish_seen_subcommand_from domcapabilities" -l arch -d "Domain architecture (/domain/os/type/@arch)"
complete -c virsh -n "__fish_seen_subcommand_from domcapabilities" -l machine -d "Machine type (/domain/os/type/@machine)"

# virsh freecell
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a freecell -d "NUMA free memory"
complete -c virsh -n "__fish_seen_subcommand_from freecell" -l cellno -d "NUMA cell number"
complete -c virsh -n "__fish_seen_subcommand_from freecell" -l all -d "Show free memory for all NUMA cells"

# virsh freepages
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a freepages -d "NUMA free pages"
complete -c virsh -n "__fish_seen_subcommand_from freepages" -l cellno -d "NUMA cell number"
complete -c virsh -n "__fish_seen_subcommand_from freepages" -l pagesize -d "Page size (in kibibytes)"
complete -c virsh -n "__fish_seen_subcommand_from freepages" -l all -d "Show free pages for all NUMA cells"

# virsh hostname
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a hostname -d "Print the hypervisor hostname"

# virsh maxvcpus
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a maxvcpus -d "Connection vcpu maximum"
complete -c virsh -n "__fish_seen_subcommand_from maxvcpus" -l type -d "Domain type"

# virsh node-memory-tune
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a node-memory-tune -d "Get or set node memory parameters"
complete -c virsh -n "__fish_seen_subcommand_from node-memory-tune" -l shm-pages-to-scan -d "Number of pages to scan before the shared memory service goes to sleep"
complete -c virsh -n "__fish_seen_subcommand_from node-memory-tune" -l shm-sleep-millisecs -d "Number of millisecs the shared memory service should sleep before next scan"
complete -c virsh -n "__fish_seen_subcommand_from node-memory-tune" -l shm-merge-across-nodes -d "Specifies if pages from different numa nodes can be merged"

# virsh nodecpumap
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodecpumap -d "Node cpu map"
complete -c virsh -n "__fish_seen_subcommand_from nodecpumap" -l pretty -d "Return human readable output"

# virsh nodecpustats
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodecpustats -d "Prints cpu stats of the node"
complete -c virsh -n "__fish_seen_subcommand_from nodecpustats" -l cpu -d "Prints specified cpu statistics only"
complete -c virsh -n "__fish_seen_subcommand_from nodecpustats" -l percent -d "Prints by percentage during 1 second"

# virsh nodeinfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodeinfo -d "Node information"

# virsh nodememstats
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodememstats -d "Prints memory stats of the node"
complete -c virsh -n "__fish_seen_subcommand_from nodememstats" -l cell -d "Prints specified cell statistics only"

# virsh nodesuspend
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodesuspend -d "Suspend the host node for a given time duration"

# virsh sysinfo
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a sysinfo -d "Print the hypervisor sysinfo"

# virsh uri
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a uri -d "Print the hypervisor canonical URI"

# virsh version
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a version -d "Show version"
complete -c virsh -n "__fish_seen_subcommand_from version" -l daemon -d "Report daemon version too"

# virsh iface-begin
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-begin -d "Create a snapshot of current interfaces settings"

# virsh iface-bridge
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-bridge -d "Create a bridge device and attach an existing network device to it"
complete -c virsh -n "__fish_seen_subcommand_from iface-bridge" -l no-stp -d "Do not enable STP for this bridge"
complete -c virsh -n "__fish_seen_subcommand_from iface-bridge" -l delay -d "Number of seconds to squelch traffic on newly connected ports"
complete -c virsh -n "__fish_seen_subcommand_from iface-bridge" -l no-start -d "Don't start the bridge immediately"

# virsh iface-commit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-commit -d "Commit changes made since iface-begin and free restore point"

# virsh iface-define
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-define -d "Define or modify an inactive persistent physical host interface"

# virsh iface-destroy
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-destroy -d "Destroy a physical host interface"

# virsh iface-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-dumpxml -d "Interface information in XML"
complete -c virsh -n "__fish_seen_subcommand_from iface-dumpxml" -l inactive -d "Show inactive defined XML"

# virsh iface-edit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-edit -d "Edit XML configuration for a physical host interface"

# virsh iface-list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-list -d "List physical host interfaces"
complete -c virsh -n "__fish_seen_subcommand_from iface-list" -l inactive -d "List inactive interfaces"
complete -c virsh -n "__fish_seen_subcommand_from iface-list" -l all -d "List inactive & active interfaces"

# virsh iface-mac
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-mac -d "Convert an interface name to interface MAC address"

# virsh iface-name
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-name -d "Convert an interface MAC address to interface name"

# virsh iface-rollback
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-rollback -d "Rollback to previous saved configuration created via iface-begin"

# virsh iface-start
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-start -d "Start a physical host interface"

# virsh iface-unbridge
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-unbridge -d "Undefine a bridge device after detaching its slave device"
complete -c virsh -n "__fish_seen_subcommand_from iface-unbridge" -l no-start -d "Don't start the un-slaved interface immediately"

# virsh iface-undefine
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a iface-undefine -d "Undefine a physical host interface"

# virsh nwfilter-define
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nwfilter-define -d "Define or update a network filter"

# virsh nwfilter-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nwfilter-dumpxml -d "Network filter information in XML"

# virsh nwfilter-edit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nwfilter-edit -d "Edit XML configuration for a network filter"

# virsh nwfilter-list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nwfilter-list -d "List network filters"

# virsh nwfilter-undefine
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nwfilter-undefine -d "Undefine a network filter"

# virsh net-autostart
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-autostart -d "Autostart a network"
complete -c virsh -n "__fish_seen_subcommand_from net-autostart" -x -a '(__fish_virsh_get_networks)'
complete -c virsh -n "__fish_seen_subcommand_from net-autostart" -l disable -d "Disable autostarting"

# virsh net-create
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-create -d "Create a network from an XML file"

# virsh net-define
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-define -d "Define or modify an inactive persistent virtual network"

# virsh net-destroy
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-destroy -d "Destroy (stop) a network"
complete -c virsh -n "__fish_seen_subcommand_from net-destroy" -x -a '(__fish_virsh_get_networks active)'

# virsh net-dhcp-leases
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-dhcp-leases -d "Print lease info for a given network"
complete -c virsh -n "__fish_seen_subcommand_from net-dhcp-leases" -x -a '(__fish_virsh_get_networks active)'
complete -c virsh -n "__fish_seen_subcommand_from net-dhcp-leases" -l mac -d "MAC address"

# virsh net-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-dumpxml -d "Network information in XML"
complete -c virsh -n "__fish_seen_subcommand_from net-dumpxml" -x -a '(__fish_virsh_get_networks)'
complete -c virsh -n "__fish_seen_subcommand_from net-dumpxml" -l inactive -d "Show inactive defined XML"

# virsh net-edit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-edit -d "Edit XML configuration for a network"
complete -c virsh -n "__fish_seen_subcommand_from net-edit" -x -a '(__fish_virsh_get_networks)'

# virsh net-event
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-event -d "Network Events"
complete -c virsh -n "__fish_seen_subcommand_from net-event" -l network -d "Filter by network name or UUID" -a '(__fish_virsh_get_networks)'
complete -c virsh -n "__fish_seen_subcommand_from net-event" -l event -d "Which event type to wait for"
complete -c virsh -n "__fish_seen_subcommand_from net-event" -l loop -d "Loop until timeout or interrupt, rather than one-shot"
complete -c virsh -n "__fish_seen_subcommand_from net-event" -l timeout -d "Timeout seconds"
complete -c virsh -n "__fish_seen_subcommand_from net-event" -l list -d "List valid event types"
complete -c virsh -n "__fish_seen_subcommand_from net-event" -l timestamp -d "Show timestamp for each printed event"

# virsh net-info
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-info -d "Network information"
complete -c virsh -n "__fish_seen_subcommand_from net-info" -x -a '(__fish_virsh_get_networks)'

# virsh net-list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-list -d "List networks"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l inactive -d "List inactive networks"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l all -d "List inactive & active networks"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l persistent -d "List persistent networks"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l transient -d "List transient networks"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l autostart -d "List networks with autostart enabled"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l no-autostart -d "List networks with autostart disabled"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l uuid -d "List UUID's only"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l name -d "List network names only"
complete -c virsh -n "__fish_seen_subcommand_from net-list" -l table -d "List table (default)"

# virsh net-name
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-name -d "Convert a network UUID to network name"

# virsh net-start
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-start -d "Start a (previously defined) inactive network"
complete -c virsh -n "__fish_seen_subcommand_from net-start" -x -a '(__fish_virsh_get_networks inactive)'

# virsh net-undefine
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-undefine -d "Undefine a persistent network"
complete -c virsh -n "__fish_seen_subcommand_from net-undefine" -x -a '(__fish_virsh_get_networks)'

# virsh net-update
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-update -d "Update parts of an existing network's configuration"
complete -c virsh -n "__fish_seen_subcommand_from net-update" -x -a '(__fish_virsh_get_networks)'
complete -c virsh -n "__fish_seen_subcommand_from net-update" -l parent-index -d "Which parent object to search through"
complete -c virsh -n "__fish_seen_subcommand_from net-update" -l config -d "Affect next network startup"
complete -c virsh -n "__fish_seen_subcommand_from net-update" -l live -d "Affect running network"
complete -c virsh -n "__fish_seen_subcommand_from net-update" -l current -d "Affect current state of network"

# virsh net-uuid
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a net-uuid -d "Convert a network name to network UUID"
complete -c virsh -n "__fish_seen_subcommand_from net-uuid" -x -a '(__fish_virsh_get_networks)'

# virsh nodedev-create
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodedev-create -d "Create a device defined by an XML file on the node"

# virsh nodedev-destroy
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodedev-destroy -d "Destroy (stop) a device on the node"

# virsh nodedev-detach
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodedev-detach -d "Detach node device from its device driver"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-detach" -l driver -d "Pci device assignment backend driver (e.g. 'vfio' or 'kvm')"

# virsh nodedev-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodedev-dumpxml -d "Node device details in XML"

# virsh nodedev-list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodedev-list -d "Enumerate devices on this host"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-list" -l tree -d "List devices in a tree"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-list" -l cap -d "Capability names, separated by comma"

# virsh nodedev-reattach
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodedev-reattach -d "Reattach node device to its device driver"

# virsh nodedev-reset
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodedev-reset -d "Reset node device"

# virsh nodedev-event
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a nodedev-event -d "Node Device Events"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-event" -l device -d "Filter by node device name"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-event" -l event -d "Which event type to wait for"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-event" -l loop -d "Loop until timeout or interrupt, rather than one-shot"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-event" -l timeout -d "Timeout seconds"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-event" -l list -d "List valid event types"
complete -c virsh -n "__fish_seen_subcommand_from nodedev-event" -l timestamp -d "Show timestamp for each printed event"

# virsh secret-define
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a secret-define -d "Define or modify a secret"

# virsh secret-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a secret-dumpxml -d "Secret attributes in XML"

# virsh secret-event
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a secret-event -d "Secret Events"
complete -c virsh -n "__fish_seen_subcommand_from secret-event" -l secret -d "Filter by secret name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from secret-event" -l event -d "Which event type to wait for"
complete -c virsh -n "__fish_seen_subcommand_from secret-event" -l loop -d "Loop until timeout or interrupt, rather than one-shot"
complete -c virsh -n "__fish_seen_subcommand_from secret-event" -l timeout -d "Timeout seconds"
complete -c virsh -n "__fish_seen_subcommand_from secret-event" -l list -d "List valid event types"
complete -c virsh -n "__fish_seen_subcommand_from secret-event" -l timestamp -d "Show timestamp for each printed event"

# virsh secret-get-value
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a secret-get-value -d "Output a secret value"

# virsh secret-list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a secret-list -d "List secrets"
complete -c virsh -n "__fish_seen_subcommand_from secret-list" -l ephemeral -d "List ephemeral secrets"
complete -c virsh -n "__fish_seen_subcommand_from secret-list" -l no-ephemeral -d "List non-ephemeral secrets"
complete -c virsh -n "__fish_seen_subcommand_from secret-list" -l private -d "List private secrets"
complete -c virsh -n "__fish_seen_subcommand_from secret-list" -l no-private -d "List non-private secrets"

# virsh secret-set-value
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a secret-set-value -d "Set a secret value"

# virsh secret-undefine
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a secret-undefine -d "Undefine a secret"

# virsh snapshot-create
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-create -d "Create a snapshot from XML"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l xmlfile -d "Domain snapshot XML"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l redefine -d "Redefine metadata for existing snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l current -d "With redefine, set current snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l no-metadata -d "Take snapshot but create no metadata"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l halt -d "Halt domain after snapshot is created"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l disk-only -d "Capture disk state but not vm state"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l reuse-external -d "Reuse any existing external files"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l quiesce -d "Quiesce guest's file systems"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l atomic -d "Require atomic operation"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create" -l live -d "Take a live snapshot"

# virsh snapshot-create-as
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-create-as -d "Create a snapshot from a set of args"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l name -d "Name of snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l description -d "Description of snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l print-xml -d "Print XML document rather than create"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l no-metadata -d "Take snapshot but create no metadata"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l halt -d "Halt domain after snapshot is created"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l disk-only -d "Capture disk state but not vm state"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l reuse-external -d "Reuse any existing external files"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l quiesce -d "Quiesce guest's file systems"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l atomic -d "Require atomic operation"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l live -d "Take a live snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-create-as" -l memspec -d "Memory attributes: [file=]name[,snapshot=type]"

# virsh snapshot-current
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-current -d "Get or set the current snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-current" -l name -d "List the name, rather than the full xml"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-current" -l security-info -d "Include security sensitive information in XML dump"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-current" -l snapshotname -d "Name of existing snapshot to make current"

# virsh snapshot-delete
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-delete -d "Delete a domain snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-delete" -l snapshotname -d "Snapshot name"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-delete" -l current -d "Delete current snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-delete" -l children -d "Delete snapshot and all children"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-delete" -l children-only -d "Delete children but not snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-delete" -l metadata -d "Delete only libvirt metadata, leaving snapshot contents behind"

# virsh snapshot-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-dumpxml -d "Dump XML for a domain snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-dumpxml" -l security-info -d "Include security sensitive information in XML dump"

# virsh snapshot-edit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-edit -d "Edit XML for a snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-edit" -l snapshotname -d "Snapshot name"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-edit" -l current -d "Also set edited snapshot as current"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-edit" -l rename -d "Allow renaming an existing snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-edit" -l clone -d "Allow cloning to new name"

# virsh snapshot-info
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-info -d "Snapshot information"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-info" -l snapshotname -d "Snapshot name"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-info" -l current -d "Info on current snapshot"

# virsh snapshot-list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-list -d "List snapshots for a domain"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l parent -d "Add a column showing parent snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l roots -d "List only snapshots without parents"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l leaves -d "List only snapshots without children"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l no-leaves -d "List only snapshots that are not leaves (with children)"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l metadata -d "List only snapshots that have metadata that would prevent undefine"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l no-metadata -d "List only snapshots that have no metadata managed by libvirt"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l inactive -d "Filter by snapshots taken while inactive"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l active -d "Filter by snapshots taken while active (system checkpoints)"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l disk-only -d "Filter by disk-only snapshots"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l internal -d "Filter by internal snapshots"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l external -d "Filter by external snapshots"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l tree -d "List snapshots in a tree"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l from -d "Limit list to children of given snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l current -d "Limit list to children of current snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l descendants -d "With --from, list all descendants"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-list" -l name -d "List snapshot names only"

# virsh snapshot-parent
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-parent -d "Get the name of the parent of a snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-parent" -l snapshotname -d "Find parent of snapshot name"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-parent" -l current -d "Find parent of current snapshot"

# virsh snapshot-revert
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a snapshot-revert -d "Revert a domain to a snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-revert" -l snapshotname -d "Snapshot name"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-revert" -l current -d "Revert to current snapshot"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-revert" -l running -d "After reverting, change state to running"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-revert" -l paused -d "After reverting, change state to paused"
complete -c virsh -n "__fish_seen_subcommand_from snapshot-revert" -l force -d "Try harder on risky reverts"

# virsh find-storage-pool-sources-as
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a find-storage-pool-sources-as -d "Find potential storage pool sources"
complete -c virsh -n "__fish_seen_subcommand_from find-storage-pool-sources-as" -l host -d "Optional host to query"
complete -c virsh -n "__fish_seen_subcommand_from find-storage-pool-sources-as" -l port -d "Optional port to query"
complete -c virsh -n "__fish_seen_subcommand_from find-storage-pool-sources-as" -l initiator -d "Optional initiator IQN to use for query"

# virsh find-storage-pool-sources
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a find-storage-pool-sources -d "Discover potential storage pool sources"
complete -c virsh -n "__fish_seen_subcommand_from find-storage-pool-sources" -l srcSpec -d "Optional file of source xml to query for pools"

# virsh pool-autostart
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-autostart -d "Autostart a pool"
complete -c virsh -n "__fish_seen_subcommand_from pool-autostart" -l disable -d "Disable autostarting"

# virsh pool-build
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-build -d "Build a pool"
complete -c virsh -n "__fish_seen_subcommand_from pool-build" -l no-overwrite -d "Do not overwrite any existing data"
complete -c virsh -n "__fish_seen_subcommand_from pool-build" -l overwrite -d "Overwrite any existing data"

# virsh pool-create-as
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-create-as -d "Create a pool from a set of args"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l print-xml -d "Print XML document, but don't define/create"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l source-host -d "Source-host for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l source-path -d "Source path for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l source-dev -d "Source device for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l source-name -d "Source name for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l target -d "Target for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l source-format -d "Format for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l auth-type -d "Auth type to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l auth-username -d "Auth username to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l secret-usage -d "Auth secret usage to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l secret-uuid -d "Auth secret UUID to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l adapter-name -d "Adapter name to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l adapter-wwnn -d "Adapter wwnn to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l adapter-wwpn -d "Adapter wwpn to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l adapter-parent -d "Adapter parent to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l build -d "Build the pool as normal"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l no-overwrite -d "Do not overwrite any existing data"
complete -c virsh -n "__fish_seen_subcommand_from pool-create-as" -l overwrite -d "Overwrite any existing data"

# virsh pool-create
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-create -d "Create a pool from an XML file"
complete -c virsh -n "__fish_seen_subcommand_from pool-create" -l build -d "Build the pool as normal"
complete -c virsh -n "__fish_seen_subcommand_from pool-create" -l no-overwrite -d "Do not overwrite any existing data"
complete -c virsh -n "__fish_seen_subcommand_from pool-create" -l overwrite -d "Overwrite any existing data"

# virsh pool-define-as
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-define-as -d "Define a pool from a set of args"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l print-xml -d "Print XML document, but don't define/create"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l source-host -d "Source-host for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l source-path -d "Source path for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l source-dev -d "Source device for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l source-name -d "Source name for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l target -d "Target for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l source-format -d "Format for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l auth-type -d "Auth type to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l auth-username -d "Auth username to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l secret-usage -d "Auth secret usage to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l secret-uuid -d "Auth secret UUID to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l adapter-name -d "Adapter name to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l adapter-wwnn -d "Adapter wwnn to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l adapter-wwpn -d "Adapter wwpn to be used for underlying storage"
complete -c virsh -n "__fish_seen_subcommand_from pool-define-as" -l adapter-parent -d "Adapter parent to be used for underlying storage"

# virsh pool-define
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-define -d "Define or modify an inactive persistent storage pool"

# virsh pool-delete
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-delete -d "Delete a pool"

# virsh pool-destroy
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-destroy -d "Destroy (stop) a pool"

# virsh pool-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-dumpxml -d "Pool information in XML"
complete -c virsh -n "__fish_seen_subcommand_from pool-dumpxml" -l inactive -d "Show inactive defined XML"

# virsh pool-edit
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-edit -d "Edit XML configuration for a storage pool"

# virsh pool-info
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-info -d "Storage pool information"
complete -c virsh -n "__fish_seen_subcommand_from pool-info" -l bytes -d "Reture pool info in bytes"

# virsh pool-list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-list -d "List pools"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l inactive -d "List inactive pools"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l all -d "List inactive & active pools"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l transient -d "List transient pools"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l persistent -d "List persistent pools"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l autostart -d "List pools with autostart enabled"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l no-autostart -d "List pools with autostart disabled"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l type -d "Only list pool of specified type(s) (if supported)"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l details -d "Display extended details for pools"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l uuid -d "List UUID of active pools only"
complete -c virsh -n "__fish_seen_subcommand_from pool-list" -l name -d "List name of active pools only"

# virsh pool-name
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-name -d "Convert a pool UUID to pool name"

# virsh pool-refresh
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-refresh -d "Refresh a pool"

# virsh pool-start
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-start -d "Start a (previously defined) inactive pool"
complete -c virsh -n "__fish_seen_subcommand_from pool-start" -l build -d "Build the pool as normal"
complete -c virsh -n "__fish_seen_subcommand_from pool-start" -l no-overwrite -d "Do not overwrite any existing data"
complete -c virsh -n "__fish_seen_subcommand_from pool-start" -l overwrite -d "Overwrite any existing data"

# virsh pool-undefine
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-undefine -d "Undefine an inactive pool"

# virsh pool-uuid
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-uuid -d "Convert a pool name to pool UUID"

# virsh pool-event
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a pool-event -d "Storage Pool Events"
complete -c virsh -n "__fish_seen_subcommand_from pool-event" -l pool -d "Filter by storage pool name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from pool-event" -l event -d "Which event type to wait for"
complete -c virsh -n "__fish_seen_subcommand_from pool-event" -l loop -d "Loop until timeout or interrupt, rather than one-shot"
complete -c virsh -n "__fish_seen_subcommand_from pool-event" -l timeout -d "Timeout seconds"
complete -c virsh -n "__fish_seen_subcommand_from pool-event" -l list -d "List valid event types"
complete -c virsh -n "__fish_seen_subcommand_from pool-event" -l timestamp -d "Show timestamp for each printed event"

# virsh vol-clone
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-clone -d "Clone a volume"
complete -c virsh -n "__fish_seen_subcommand_from vol-clone" -l pool -d "Pool name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from vol-clone" -l prealloc-metadata -d "Preallocate metadata (for qcow2 instead of full allocation)"
complete -c virsh -n "__fish_seen_subcommand_from vol-clone" -l reflink -d "Use btrfs COW lightweight copy"

# virsh vol-create-as
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-create-as -d "Create a volume from a set of args"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-as" -l allocation -d "Initial allocation size, as scaled integer (default bytes)"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-as" -l format -d "File format type raw,bochs,qcow,qcow2,qed,vmdk"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-as" -l backing-vol -d "The backing volume if taking a snapshot"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-as" -l backing-vol-format -d "Format of backing volume if taking a snapshot"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-as" -l prealloc-metadata -d "Preallocate metadata (for qcow2 instead of full allocation)"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-as" -l print-xml -d "Print XML document, but don't define/create"

# virsh vol-create
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-create -d "Create a vol from an XML file"
complete -c virsh -n "__fish_seen_subcommand_from vol-create" -l prealloc-metadata -d "Preallocate metadata (for qcow2 instead of full allocation)"

# virsh vol-create-from
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-create-from -d "Create a vol, using another volume as input"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-from" -l inputpool -d "Pool name or UUID of the input volume's pool"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-from" -l prealloc-metadata -d "Preallocate metadata (for qcow2 instead of full allocation)"
complete -c virsh -n "__fish_seen_subcommand_from vol-create-from" -l reflink -d "Use btrfs COW lightweight copy"

# virsh vol-delete
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-delete -d "Delete a vol"
complete -c virsh -n "__fish_seen_subcommand_from vol-delete" -l pool -d "Pool name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from vol-delete" -l delete-snapshots -d "Delete snapshots associated with volume (must be supported by storage driver)"

# virsh vol-download
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-download -d "Download volume contents to a file"
complete -c virsh -n "__fish_seen_subcommand_from vol-download" -l pool -d "Pool name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from vol-download" -l offset -d "Volume offset to download from"
complete -c virsh -n "__fish_seen_subcommand_from vol-download" -l length -d "Amount of data to download"
complete -c virsh -n "__fish_seen_subcommand_from vol-download" -l sparse -d "Preserve sparseness of volume"

# virsh vol-dumpxml
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-dumpxml -d "Vol information in XML"
complete -c virsh -n "__fish_seen_subcommand_from vol-dumpxml" -l pool -d "Pool name or UUID"

# virsh vol-info
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-info -d "Storage vol information"
complete -c virsh -n "__fish_seen_subcommand_from vol-info" -l pool -d "Pool name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from vol-info" -l bytes -d "Sizes are represented in bytes rather than pretty units"
complete -c virsh -n "__fish_seen_subcommand_from vol-info" -l physical -d "Return the physical size of the volume in allocation field"

# virsh vol-key
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-key -d "Returns the volume key for a given volume name or path"
complete -c virsh -n "__fish_seen_subcommand_from vol-key" -l pool -d "Pool name or UUID"

# virsh vol-list
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-list -d "List vols"
complete -c virsh -n "__fish_seen_subcommand_from vol-list" -l details -d "Display extended details for volumes"

# virsh vol-name
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-name -d "Returns the volume name for a given volume key or path"

# virsh vol-path
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-path -d "Returns the volume path for a given volume name or key"
complete -c virsh -n "__fish_seen_subcommand_from vol-path" -l pool -d "Pool name or UUID"

# virsh vol-pool
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-pool -d "Returns the storage pool for a given volume key or path"
complete -c virsh -n "__fish_seen_subcommand_from vol-pool" -l uuid -d "Return the pool UUID rather than pool name"

# virsh vol-resize
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-resize -d "Resize a vol"
complete -c virsh -n "__fish_seen_subcommand_from vol-resize" -l pool -d "Pool name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from vol-resize" -l allocate -d "Allocate the new capacity, rather than leaving it sparse"
complete -c virsh -n "__fish_seen_subcommand_from vol-resize" -l delta -d "Use capacity as a delta to current size, rather than the new size"
complete -c virsh -n "__fish_seen_subcommand_from vol-resize" -l shrink -d "Allow the resize to shrink the volume"

# virsh vol-upload
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-upload -d "Upload file contents to a volume"
complete -c virsh -n "__fish_seen_subcommand_from vol-upload" -l pool -d "Pool name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from vol-upload" -l offset -d "Volume offset to upload to"
complete -c virsh -n "__fish_seen_subcommand_from vol-upload" -l length -d "Amount of data to upload"
complete -c virsh -n "__fish_seen_subcommand_from vol-upload" -l sparse -d "Preserve sparseness of volume"

# virsh vol-wipe
complete -c virsh -n "not __fish_seen_subcommand_from $cmds" -a vol-wipe -d "Wipe a vol"
complete -c virsh -n "__fish_seen_subcommand_from vol-wipe" -l pool -d "Pool name or UUID"
complete -c virsh -n "__fish_seen_subcommand_from vol-wipe" -l algorithm -d "Perform selected wiping algorithm"
