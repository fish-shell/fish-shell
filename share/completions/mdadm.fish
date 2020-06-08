# fish completions for mdadm, as documented under Debian Stretch (mdadm v3.4)
# Known lacks :
# - be more contextual : suggest according to available files, devices, arrays…
# - especially, suggest values for levels, metadata format… according to the already given parameters
# - some descriptions are not very useful, even a bit Captain Obvious-like
# - some options could see their meaning in different contexts explained with different completions with proper tests for context
# - misc and manage modes could be tested for, and the corresponding options proposed only in the right context
#
# -n tests have been simplified when __fish_contains_opt …; or _fish_not_contain_opt …: the options in the first test were removed from the second one,
#     as their absence has already been checked, because this absence caused evaluation of __fish_not_contain_opt

function __fish_mdadm_metadata_options
    echo -e "0\tUse original 0.90 format superblock"
    echo -e "0.90\tUse original 0.90 format superblock"
    echo -e "1\tUse last 1.x format superblock"
    echo -e "default\tUse last 1.x format superblock"
    echo -e "1.0\tUse 1.0 format superblock"
    echo -e "1.1\tUse 1.1 format superblock"
    echo -e "1.2\tUse 1.2 format superblock"
    echo -e "ddf\tUse DDF (Disk Data Format) format"
    echo -e "imsm\tUse Intel(R) Matrix Storage Manager format"
end

function __fish_mdadm_level_options
    echo -e "linear\tJBOD"
    echo -e "raid0\tStripped volume (RAID 0)"
    echo -e "0\tStripped volume (RAID 0)"
    echo -e "stripe\tStripped volume (RAID 0)"
    echo -e "raid1\tMirrored volume (RAID 1)"
    echo -e "1\tMirrored volume (RAID 1)"
    echo -e "mirror\tMirrored volume (RAID 1)"
    echo -e "raid4\tStripped volume with parity disk (RAID 4)"
    echo -e "4\tStripped volume with parity disk (RAID 4)"
    echo -e "raid5\tStripped volume with distributed parity (RAID 5)"
    echo -e "5\tStripped volume with distributed parity (RAID 5)"
    echo -e "raid6\tStripped volume with double distributed parity (RAID 5)"
    echo -e "6\tStripped volume with double distributed parity (RAID 5)"
    echo -e "raid10\tMirrored stripped volume (RAID 10)"
    echo -e "10\tMirrored stripped volume (RAID 10)"
    echo -e "multipath\tMultiple access device, AKA multipath (deprecated)"
    echo -e "mp\tMultiple access device, AKA multipath (deprecated)"
    echo -e "faulty\tPseudo RAID layer for single device (akin faulty RAID 1)"
    echo -e "container\tContainer" # To be clarified
end

function __fish_mdadm_layout_options # To be clarified
    echo -e left-asymmetric
    echo -e left-symmetric
    echo -e right-asymmetric
    echo -e right-symmetric
    echo -e "la\tAlias of left-asymmetric"
    echo -e "ra\tAlias of right-asymmetric"
    echo -e "ls\tAlias of left-symmetric"
    echo -e "rs\tAlias of right-symmetric"
    echo -e parity-first
    echo -e parity-last
    echo -e ddf-zero-restart
    echo -e ddf-N-restart
    echo -e ddf-N-continue
    echo -e left-symmetric-6
    echo -e right-symmetric-6
    echo -e left-asymmetric-6
    echo -e right-asymmetric-6
    echo -e parity-first-6
    echo -e write-transient
    echo -e "wt\tAlias of write-transient"
    echo -e read-transient
    echo -e "rt\tAlias of read-transient"
    echo -e write-persistent
    echo -e "wp\tAlias of write-persistent"
    echo -e read-persistent
    echo -e "rp\tAlias of read-persistent"
    echo -e write-all
    echo -e read-fixable
    echo -e "rf\tAlias of read-fixable"
    echo -e clear
    echo -e flush
    echo -e none
    echo -e n
    echo -e o
    echo -e f
    echo -e normalise
    echo -e preserve
end

function __fish_mdadm_level_options
    echo -e "yes\tUse standard format (default)"
    echo -e "md\tUse a non-partitionable array"
    echo -e "mdp\tUse a partitionable array"
    echo -e "part\tUse a partitionable array"
    echo -e "p\tUse a partitionable array"
end

function __fish_mdadm_update_options
    echo -e "sparc2.2\tRemove superblock misalignement from SPARC kernel 2.2"
    echo -e "summaries\tCorrect superblock summaries"
    echo -e "uuid\tUpdate array UUID"
    echo -e "name\tUpdate array name"
    echo -e "nodes\tUpdate array nodes"
    echo -e "homehost\tUpdate array homehost"
    echo -e "home-cluster\tUpdate array cluster name"
    echo -e "resync\tMark the array as dirty, thus forcing resync"
    echo -e "byteorder\tReverse superblock endianness"
    echo -e "devicesize\tRefresh device size"
    echo -e "no-bitmap\tAssume bitmap absence"
    echo -e "bbl\tReserve space for bad block list"
    echo -e "no-bbl\tFree reserved space for bad block list"
    echo -e "metadata\tConvert 0.90 metadata to 1.0"
    echo -e "super-minor\tReset preferred minor to current one"
end

function __fish_mdadm_action_options
    echo -e "idle\tAbort currently running actions"
    echo -e "frozen\tAbort currently running actions, prevent their restart"
    echo -e "check\tScrub the array (i.e. check constistency)"
    echo -e "repair\tCheck, then resync"
end

# In the next 7 lines, the tested option is maintained into the search list to prevent suggesting it if it has already been used, as they cannot be used twice
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s A -l assemble -d "Assemble a pre-existing array"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s B -l build -d "Build a legacy array without superblocks"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s C -l create -d "Create a new array"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s F -l follow -l monitor -d "Select monitor mode"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s G -l grow -d "Change the size or shape of an active array"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s I -l incremental -d "Manage devices in array, and possibly start it"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l auto-detect -d "Start all auto-detected arrays"
complete -c mdadm -s h -l help -d "Display help"
complete -c mdadm -l help-options -d "Display more detailed help"
complete -c mdadm -s V -l version -d "Print version information"
complete -c mdadm -s v -l verbose -d "Be more verbose"
complete -c mdadm -s q -l quiet -d "Be quiet"
complete -c mdadm -s f -l force -d "Force operation"
complete -c mdadm -s c -l config -r -d "Specify config file or directory"
complete -c mdadm -s s -l scan -d "Scan for missing information"
complete -c mdadm -s e -l metadata -a __fish_mdadm_metadata_options -x -d "Set metadata style to use"
complete -c mdadm -l homehost -x -d "Provide home host identity"
complete -c mdadm -l prefer -x -d "Give name format preference"
complete -c mdadm -l home-cluster -x -d "Give cluster name"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow; or __fish_not_contain_opt -s A assemble -s F follow monitor -s I incremental auto-detect' -s n -l raid-devices -x -d "Specify the number of active devices"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -s x -l spare-devices -x -d "Specify the number of spare devices"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -s z -l size -x -d "Specify the space to use from each drive"
complete -c mdadm -n '__fish_contains_opt -s G grow' -s Z -l array-size -x -d "Specify the size made available on the array"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -s c -l chunk -x -d "Specify the chunk size"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -l rounding -x -d "Specify rounding factor"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -s l -l level -a __fish_mdadm_level_options -x -d "Specify RAID level"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -s p -l layout -l parity -a __fish_mdadm_layout_options -x -d "Specify data layout"
complete -c mdadm -n '__fish_contains_opt -s A assemble -s B build -s C create -s G grow' -s b -l bitmap -r -d "Specify file for write-intent bitmap"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -l bitmap-chunk -x -d "Specify chunksize of bitmap"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow -s a add' -s W -l write-mostly -d "Prefer reading from other devices than these"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l readwrite -d "Enable writes on array or device"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create; or __fish_not_contain_opt -s A assemble -s F follow monitor -s G grow -s I incremental' -s o -l readonly -d "Disable writes on array"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -l write-behind -x -d "Enable write-behind mode"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -l assume-clean -d "Assume the array is clean"
complete -c mdadm -n '__fish_contains_opt -s A assemble -s G grow' -l backup-file -r -d "Use this file as a backup"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -l data-offset -x -d "Specify start of array data"
complete -c mdadm -n '__fish_contains_opt -s G grow' -l continue -d "Resume frozen --grow command"
complete -c mdadm -n '__fish_contains_opt -s A assemble -s B build -s C create' -s N -l name -x -d "Set array name"
complete -c mdadm -n '__fish_contains_opt -s A assemble -s B build -s C create -s I incremental; or __fish_not_contain_opt -s F follow monitor -s G grow' -s R -l run -d "Run array despite warnings"
complete -c mdadm -n '__fish_contains_opt -s A assemble -s B build -s C create' -s a -l auto -l level -a __fish_mdadm_level_options -x -d "Give instruction for device file" # May be improved with device numbers management
complete -c mdadm -n '__fish_contains_opt -s A assemble -s G grow' -s a -l add -d "Add devices to array"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -l nodes -d "Specify max nodes in the cluster"
complete -c mdadm -n '__fish_contains_opt -s B build -s C create -s G grow' -l write-journal -d "Specify journal device for RAID-4/5/6 array"
complete -c mdadm -n '__fish_contains_opt -s A assemble' -s u -l uuid -x -d "UUID of array to assemble"
complete -c mdadm -n '__fish_contains_opt -s A assemble' -s m -l super-minor -x -d "Minor number of array device"
complete -c mdadm -n '__fish_contains_opt -s A assemble; and __fish_contains_opt -s s scan' -l no-degraded -d "Refuse to start without all drives"
complete -c mdadm -n '__fish_contains_opt -s A assemble' -l invalid-backup -d "Do not ask for backup file, unavailable"
complete -c mdadm -n '__fish_contains_opt -s A assemble' -s U -l update -a __fish_mdadm_update_options -x -d "Update superblock properties"
complete -c mdadm -n '__fish_contains_opt -s A assemble' -l freeze-reshape -d "Freeze --grow command"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor; or __fish_not_contain_opt -s A assemble -s B build -s C create -s G grow -s I incremental auto-detect' -s t -l test -d "Test mode" # To be clarified
complete -c mdadm -n '__fish_contains_opt -s G grow; or __fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s I incremental auto-detect' -s a -l add -d "Hot-add listed devices"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l re-add -d "Re-add a previously removed device"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l add-spare -d "Hot-add listed devices as spare"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s r -l remove -d "Remove listed inactive devices"
complete -c mdadm -n '__fish_contains_opt -s I incremental; or __fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow auto-detect' -s f -l fail -l set-faulty -d "Mark listed devices as faulty"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l replace -d "Mark listed devices as requiring replacement"
complete -c mdadm -n '__fish_contains_opt replace' -l with -d "Give devices as replacement"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l cluster-confirm -d "Confirm existence of device"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s Q -l query -d "Examine device for md use"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s D -l detail -d "Print details on array"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l detail-platform -d "Print details on platform capabilities"
complete -c mdadm -n '__fish_contains_opt -s I incremental; or __fish_contains_opt -s D detail; or __fish_contains_opt detail-platform; or __fish_contains_opt -s E examine' -s Y -l export -d "Format data output as key=value pairs"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s E -l examine -d "Print content of device metadata"
complete -c mdadm -n '__fish_contains_opt -s E examine; or __fish_contains_opt -s A assemble' -l sparc2.2 -d "Fix examination for buggy SPARC 2.2 kernel RAID"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s X -l examine-bitmap -d "Print report about bitmap"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l examine-badblocks -d "List recorded bad blocks"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l dump -r -d "Dump metadata to directory"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l restore -r -d "Restore metadata from directory"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s S -l stop -d "Deactivate array"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l zero-superblock -d "Erase possible superblock"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l kill-subarray -r -d "Delete subarray"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l update-subarray -r -d "Update subarray" # To be clarified
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -s W -l wait -d "Wait for pending operations"
complete -c mdadm -l wait-clean -d "Mark array as clean ASAP"
complete -c mdadm -n '__fish_not_contain_opt -s A assemble -s B build -s C create -s F follow monitor -s G grow -s I incremental auto-detect' -l action -a __fish_mdadm_action_options -x -d "Set sync action for md devices"
complete -c mdadm -n '__fish_contains_opt -s I incremental' -s r -l rebuild-map -d "Rebuild /run/mdadm/map"
complete -c mdadm -n '__fish_contains_opt -s I incremental; and __fish_contains_opt -s f fail set-faulty' -l path -r -d "Automatically add eventually appearing device to array"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor' -s m -l mail -x -d "Mail address to send alerts to"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor' -s p -l program -l alert -x -d "Program to run in case of an event"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor' -s y -l syslog -d "Record events in syslog"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor' -s d -l delay -x -d "Polling interval"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor' -s r -l increment -x -d "Generate RebuildNN events each given percentage"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor' -s f -l daemonise -d "Run monitor mode as a daemon"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor; and __fish_contains_opt -s 1 oneshot' -s i -l pid-file -r -d "Write PID file when running as a daemon"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor' -s 1 -l oneshot -d "Check arrays only once"
complete -c mdadm -n '__fish_contains_opt -s F follow monitor' -l no-sharing -d "Do not move spares between arrays"
complete -c mdadm -n '__fish_contains_opt -s D detail; or __fish_contains_opt -s E examine' -s b -l brief -d "Be more concise"
