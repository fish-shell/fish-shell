function __fish_hashcat_types --description "Get hashcat hash types"
    set -l modes (hashcat --example-hashes | string replace -f -r '^(?:MODE: |Hash mode #)(\d+)' '$1')
    set -l types (hashcat --example-hashes | string replace -f -r '^(?:TYPE:|\s+Name\.+:)\s+(.+)' '$1')
    for i in (seq (count $modes))
        echo -e "$modes[$i]\t$types[$i]"
    end
end

function __fish_hashcat_outfile_formats --description "Get hashcat outfile formats"
    echo -e "
1\thash[:salt]
2\tplain
3\thex_plain
4\tcrack_pos
5\ttimestamp absolute
6\ttimestamp relative"
end

function __fish_hashcat_device_types --description "Get hashcat device types"
    echo -e "
1\tCPU
2\tGPU
3\tFPGA, DSP, Co-Processor"
end

complete -c hashcat -s m -l hash-type -xa "(__fish_hashcat_types)" -d Hash-type
complete -c hashcat -s a -l attack-mode -d Attack-mode -xa "
                                                0\t'Straight'
                                                1\t'Combination'
                                                3\t'Brute-force'
                                                6\t'Hybrid Wordlist + Mask'
                                                7\t'Hybrid Mask + Wordlist'
                                                9\t'Association'"
complete -c hashcat -s V -l version -d "Print version"
complete -c hashcat -s h -l help -d "Print help"
complete -c hashcat -l quiet -d "Suppress output"
complete -c hashcat -l hex-charset -d "Assume charset is given in hex"
complete -c hashcat -l hex-salt -d "Assume salt is given in hex"
complete -c hashcat -l hex-wordlist -d "Assume words in wordlist are given in hex"
complete -c hashcat -l force -d "Ignore warnings"
complete -c hashcat -l deprecated-check-disable -d "Enable deprecated plugins"
complete -c hashcat -l status -d "Enable automatic update of the status screen"
complete -c hashcat -l status-json -d "Enable JSON format for status ouput"
complete -c hashcat -l status-timer -x -d "Sets seconds between status screen updates"
complete -c hashcat -l stdin-timeout-abort -x -d "Abort if there is no input from stdin for X seconds"
complete -c hashcat -l machine-readable -d "Display the status view in a machine-readable format"
complete -c hashcat -l keep-guessing -d "Keep guessing the hash after it has been cracked"
complete -c hashcat -l self-test-disable -d "Disable self-test functionality on startup"
complete -c hashcat -l loopback -d "Add new plains to induct directory"
complete -c hashcat -l markov-hcstat2 -rF -d "Specify hcstat2 file to use"
complete -c hashcat -l markov-disable -d "Disables markov-chains, emulates classic brute-force"
complete -c hashcat -l markov-classic -d "Enables classic markov-chains, no per-position"
complete -c hashcat -l markov-inverse -d "Enables inverse markov-chains, no per-position"
complete -c hashcat -s t -l markov-threshold -x -d "Threshold when to stop accepting new markov-chains"
complete -c hashcat -l runtime -x -d "Abort session after X seconds of runtime"
complete -c hashcat -l session -x -d "Define specific session name"
complete -c hashcat -l restore -d "Restore session from --session"
complete -c hashcat -l restore-disable -d "Do not write restore file"
complete -c hashcat -l restore-file-path -rF -d "Specific path to restore file"
complete -c hashcat -s o -l outfile -rF -d "Define outfile for recovered hash"
complete -c hashcat -l outfile-format -xa "(__fish_complete_list , __fish_hashcat_outfile_formats)" -d "Outfile formats to use"
complete -c hashcat -l outfile-autohex-disable -d "Disable the use of \$HEX[] in output plains"
complete -c hashcat -l outfile-check-timer -x -d "Sets seconds between outfile checks"
complete -c hashcat -l wordlist-autohex-disable -d "Disable the conversion of \$HEX[] from the wordlist"
complete -c hashcat -s p -l separator -x -d "Separator char for hashlists and outfile"
complete -c hashcat -l stdout -d "Do not crack a hash, instead print candidates only"
complete -c hashcat -l show -d "Compare hashlist with potfile; show cracked hashes"
complete -c hashcat -l left -d "Compare hashlist with potfile; show uncracked hashes"
complete -c hashcat -l username -d "Enable ignoring of usernames in hashfile"
complete -c hashcat -l remove -d "Enable removal of hashes once they are cracked"
complete -c hashcat -l remove-timer -x -d "Update input hash file each X seconds"
complete -c hashcat -l potfile-disable -d "Do not write potfile"
complete -c hashcat -l potfile-path -rF -d "Specific path to potfile"
complete -c hashcat -l encoding-from -x -d "Force internal wordlist encoding from X"
complete -c hashcat -l encoding-to -x -d "Force internal wordlist encoding to X"
complete -c hashcat -l debug-mode -d "Defines the debug mode" -xa "
                                            1\t'Finding-Rule'
                                            2\t'Original-Word'
                                            3\t'Original-Word:Finding-Rule'
                                            4\t'Original-Word:Finding-Rule:Processed-Word'"
complete -c hashcat -l debug-file -rF -d "Output file for debugging rules"
complete -c hashcat -l induction-dir -xa "(__fish_complete_directories)" -d "Specify the induction directory to use for loopback"
complete -c hashcat -l outfile-check-dir -xa "(__fish_complete_directories)" -d "Specify the outfile directory to monitor for plains"
complete -c hashcat -l logfile-disable -d "Disable the logfile"
complete -c hashcat -l hccapx-message-pair -x -d "Load only message pairs from hccapx matching X"
complete -c hashcat -l nonce-error-corrections -x -d "The BF size range to replace AP's nonce last bytes"
complete -c hashcat -l keyboard-layout-mapping -rF -d "Keyboard layout mapping table for special hash-modes"
complete -c hashcat -l truecrypt-keyfiles -rF -d "Keyfiles to use"
complete -c hashcat -l veracrypt-keyfiles -rF -d "Keyfiles to use"
complete -c hashcat -l veracrypt-pim-start -x -d "VeraCrypt personal iterations multiplier start"
complete -c hashcat -l veracrypt-pim-stop -x -d "VeraCrypt personal iterations multiplier stop"
complete -c hashcat -s b -l benchmark -d "Run benchmark of selected hash-modes"
complete -c hashcat -l benchmark-all -d "Run benchmark of all hash-modes (requires -b)"
complete -c hashcat -l speed-only -d "Return expected speed of the attack, then quit"
complete -c hashcat -l progress-only -d "Return ideal progress step size and time to process"
complete -c hashcat -s c -l segment-size -x -d "Sets size in MB to cache from the wordfile"
complete -c hashcat -l bitmap-min -x -d "Sets minimum bits allowed for bitmaps"
complete -c hashcat -l bitmap-max -x -d "Sets maximum bits allowed for bitmaps"
complete -c hashcat -l cpu-affinity -x -d "Locks to CPU devices"
complete -c hashcat -l hook-threads -x -d "Sets number of threads for a hook (per compute unit)"
complete -c hashcat -l hash-info -d "Show information for each hash-mode"
complete -c hashcat -l example-hashes -d "Alias of --hash-info"
complete -c hashcat -l backend-ignore-cuda -d "Do not try to open CUDA interface on startup"
complete -c hashcat -l backend-ignore-hip -d "Do not try to open HIP interface on startup"
complete -c hashcat -l backend-ignore-metal -d "Do not try to open Metal interface on startup"
complete -c hashcat -l backend-ignore-opencl -d "Do not try to open OpenCL interface on startup"
complete -c hashcat -s I -l backend-info -d "Show info about detected backend API devices"
complete -c hashcat -s d -l backend-devices -x -d "Backend devices to use"
complete -c hashcat -s D -l opencl-device-types -xa "(__fish_complete_list , __fish_hashcat_device_types)" -d "OpenCL device-types to use"
complete -c hashcat -s O -l optimized-kernel-enable -d "Enable optimized kernels (limits password length)"
complete -c hashcat -s M -l multiply-accel-disable -d "Disable multiply kernel-accel with processor count"
complete -c hashcat -s w -l workload-profile -d "Enable a specific workload profile" -xa "
                                                        1\tLow
                                                        2\tDefault
                                                        3\tHigh
                                                        4\tNightmare"
complete -c hashcat -s n -l kernel-accel -x -d "Manual workload tuning, set outerloop step size"
complete -c hashcat -s u -l kernel-loops -x -d "Manual workload tuning, set innerloop step size"
complete -c hashcat -s T -l kernel-threads -x -d "Manual workload tuning, set thread count"
complete -c hashcat -l backend-vector-width -x -d "Manually override backend vector-width"
complete -c hashcat -l spin-damp -x -d "Use CPU for device synchronization, in percent"
complete -c hashcat -l hwmon-disable -d "Disable temperature and fanspeed reads and triggers"
complete -c hashcat -l hwmon-temp-abort -x -d "Abort if temperature reaches X degrees Celsius"
complete -c hashcat -l scrypt-tmto -x -d "Manually override TMTO value for scrypt to X"
complete -c hashcat -s s -l skip -x -d "Skip X words from the start"
complete -c hashcat -s l -l limit -x -d "Limit X words from the start + skipped words"
complete -c hashcat -l keyspace -d "Show keyspace base:mod values and quit"
complete -c hashcat -s j -l rule-left -x -d "Single rule applied to each word from left wordlist"
complete -c hashcat -s k -l rule-right -x -d "Single rule applied to each word from right wordlist"
complete -c hashcat -s r -l rules-file -rF -d "Multiple rules applied to each word from wordlists"
complete -c hashcat -s g -l generate-rules -x -d "Generate X random rules"
complete -c hashcat -l generate-rules-func-min -x -d "Force min X functions per rule"
complete -c hashcat -l generate-rules-func-max -x -d "Force max X functions per rule"
complete -c hashcat -l generate-rules-func-sel -x -d "Pool of rule operators valid for random rule engine"
complete -c hashcat -l generate-rules-seed -x -d "Force RNG seed set to X"
complete -c hashcat -s 1 -l custom-charset1 -x -d "User-defined charset ?1"
complete -c hashcat -s 2 -l custom-charset2 -x -d "User-defined charset ?2"
complete -c hashcat -s 3 -l custom-charset3 -x -d "User-defined charset ?3"
complete -c hashcat -s 4 -l custom-charset4 -x -d "User-defined charset ?4"
complete -c hashcat -s i -l identify -d "Shows all supported algorithms for input hashes"
complete -c hashcat -s i -l increment -d "Enable mask increment mode"
complete -c hashcat -l increment-min -x -d "Start mask incrementing at X"
complete -c hashcat -l increment-max -x -d "Stop mask incrementing at X"
complete -c hashcat -s S -l slow-candidates -d "Enable slower (but advanced) candidate generators"
complete -c hashcat -l brain-server -d "Enable brain server"
complete -c hashcat -l brain-server-timer -x -d "Update the brain server dump each X seconds (min:60)"
complete -c hashcat -s z -l brain-client -d "Enable brain client, activates -S"
complete -c hashcat -l brain-client-features -d "Define brain client features" -xa "
                                                    1\t'Send hashed passwords'
                                                    2\t'Send attack positions'
                                                    3\t'Send hashed passwords and attack positions'"
complete -c hashcat -l brain-host -xa "(__fish_print_hostnames)" -d "Brain server host (IP or domain)"
complete -c hashcat -l brain-port -x -d "Brain server port"
complete -c hashcat -l brain-password -x -d "Brain server authentication password"
complete -c hashcat -l brain-session -x -d "Overrides automatically calculated brain session"
complete -c hashcat -l brain-session-whitelist -x -d "Allow given sessions only"
