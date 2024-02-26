# Helper functions for wireshark/tshark/dumpcap completion

function __fish_wireshark_choices
    string replace -rf -- '^\s*(\S+) \(?([^)]*)\)?$' '$1\t$2' $argv
end

function __fish_wireshark_interface
    # no remote capture yet
    command tshark -D 2>/dev/null | string replace -r ".*\. (\S+)\s*\(?([^)]*)\)?\$" '$1\t$2'
end

function __fish_wireshark_protocol
    command tshark -G protocols 2>/dev/null | awk -F\t '{print $3"\t"$1}'
end

function __fish_wireshark_heuristic
    command tshark -G heuristic-decodes 2>/dev/null | awk -F\t '{print $2"\t"$1}'
end

function __fish_tshark_name_resolving_flags
    printf -- (commandline -ct | string replace -r -- '^-N' '')%s\n \
        d\t"enable resolution from captured DNS packets" \
        m\t"enable MAC address resolution" \
        n\t"enable network address resolution" \
        N\t"enable using external resolvers (such as DNS) for network address resolution" \
        t\t"enable transport-layer port number resolution" \
        v\t"enable VLAN IDs to names resolution"
end

function __fish_tshark_decode_as
    set -l tok (commandline -ct | string collect)
    if string match -rq -- '==$' $tok
        return
    else if string match -rq -- '(==|,)' $tok
        set -l tok_no_comma (string replace -r -- ',.*$' '' $tok)
        command tshark -d (string replace -r -- '^-d' '' $tok) 2>|
            string replace -rf -- "^\t(\S+) \(?([^\)]*)\)?\$" "$tok_no_comma,\$1\t\$2"
    else
        command tshark -d . 2>| string replace -rf -- "^\t(\S+) \(?([^\)]*)\)?\$" '$1==\t$2'
    end
end

function __fish_complete_wireshark
    set -l shark $argv
    complete -c $shark -s a -l autostop -d 'Specify a criterion to stop writing the capture file' -xa '
duration:\t"Stop writing to capture files after N seconds have elapsed"
files:\t"Stop writing to capture files after N files were written"
filesize:\t"Stop writing a capture file after it reaches a size of N kB"
packets:\t"Stop writing a capture file after it contains N packets"'
    complete -c $shark -s b -l ring-buffer -d 'Write multiple capture files' -xa '
duration:\t"Switch to the next file after N seconds have elapsed"
files:\t"Begin again with the first file after N files were written"
filesize:\t"Switch to the next file after it reaches a size of N kB"
interval:\t"Switch to the next file when the time is an exact multiple of N seconds"
packets:\t"Switch to the next file after it contains N packets"'
    complete -c $shark -s B -l buffer-size -d 'Set capture buffer size (in MiB, default is 2 MiB)' -x
    complete -c $shark -s c -d 'Set the maximum number of packets to read' -x
    complete -c $shark -l capture-comment -d 'Add a capture comment to the output file' -x
    complete -c $shark -s D -l list-interfaces -d "Print a list of interfaces on which $shark can capture and exit"
    complete -c $shark -s f -d 'Set a capture filter expression' -x
    complete -c $shark -s i -ra '(__fish_wireshark_interface)'
    complete -c $shark -s i -ra '-\t"Capture from standard input"' \
        -d 'Network interface or pipe to use for live packet capture'
    complete -c $shark -s I -l monitor-mode -d 'Put the interface in "monitor mode"'
    complete -c $shark -s L -l list-data-link-types -d 'List the data link types supported by the interface and exit'
    complete -c $shark -l list-time-stamp-types -d 'List time stamp types supported for the interface'
    complete -c $shark -s p -l no-promiscuous-mode -d "Don't put the interface into promiscuous mode"
    complete -c $shark -s s -l snapshot-length -d 'Set the default snapshot length in bytes to use when capturing live data' -x
    complete -c $shark -l time-stamp-type -d "Change the interface's timestamp method" -xa '(__fish_wireshark_choices (command tshark --list-time-stamp-types 2>/dev/null))'
    complete -c $shark -s v -l version -d 'Print the version and exit'
    complete -c $shark -s w -d 'Write raw packet data to the given file ("-" means stdout)' -r
    complete -c $shark -s y -l linktype -d 'Set the data link type to use while capturing packets' -xa '(__fish_wireshark_choices (command tshark -L 2>/dev/null))'

    switch $shark
        case dumpcap tshark
            complete -c tshark -s q -d "Don't display the continuous count of packets captured"
            complete -c tshark -s g -d 'Create output files with greoup-read permissions'
    end

    switch $shark
        case wireshark tshark
            complete -c $shark -s C -d 'Run with the given configuration profile' -xa '(
set -l folders (tshark -G folders 2>/dev/null | awk \'/Personal configuration/{ print $NF}\')/profiles/*
string match -r "[^/]*\\$" -- $folders)'
            complete -c $shark -s d -d 'Specify how a layer type should be dissected' -xa '(__fish_tshark_decode_as)'
            complete -c $shark -l enable-protocol -d 'Enable dissection of the given protocol' -xa '(__fish_wireshark_protocol)'
            complete -c $shark -l disable-protocol -d 'Disable dissection of the given protocol' -xa '(__fish_wireshark_protocol)'
            complete -c $shark -l enable-heuristic -d 'Enable dissection of heuristic protocol' -xa '(__fish_wireshark_heuristic)'
            complete -c $shark -l disable-heuristic -d 'Disable dissection of heuristic protocol' -xa '(__fish_wireshark_heuristic)'
            complete -c $shark -s K -d 'Load kerberos crypt keys from the specified keytab file' -r
            complete -c $shark -s n -d 'Disable network object name resolution (hostname, TCP and UDP port names)'
            complete -c $shark -s N -d 'Turn on name resolution only for particular types of addresses and port numbers' -xa '( __fish_tshark_name_resolving_flags)'
            complete -c $shark -s o -d 'Override a preference value' -xa '(
    command tshark -G defaultprefs 2>/dev/null | string replace -rf -- \'^#([a-z].*):.*\' \'$1:\')'
            complete -c $shark -s r -l read-file -d 'Read packet data from the given file' -r
            complete -c $shark -s R -l read-filter -d 'Apply the given read filter' -x
            complete -c $shark -s t -d 'Set the format of the packet timestamp printed in summary lines' -xa '
a\t"absolute time"
ad\t"absolute time with date"
adoy\t"absolute time with date using day of year"
d\t"delta: time since the previous packet was captured"
dd\t"delta displayed: time since the previous displayed packet was captured"
e\t"epoch: time in seconds since Jan 1, 1970"
r\t"relative time elapsed between the first packet and the current packet"
u\t"absolute UTC time"
ud\t"absolute UTC time with date"
udoy\t"absolute UTC time with date using day of year"'
            complete -c $shark -s u -d "Specifies the seconds type" -xa 's\t"seconds" hms\t"hours, minutes and seconds"'
            complete -c $shark -s X -d "Specify an extension to be passed to a $shark module" -x # TODO
            complete -c $shark -s Y -l display-filter -d 'Apply the given display filter' -x
            complete -c $shark -s z -d 'Collect various types of statistics' -x # TODO
    end
end
