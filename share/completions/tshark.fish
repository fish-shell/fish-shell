# tshark - Dump and analyze network traffic

__fish_complete_wireshark tshark

function __fish_tshark_protocols
    set -l tok (commandline -ct | string collect)
    set -l tok_param (string replace -r -- '^-O' '' $tok)
    command tshark -G protocols 2>/dev/null | while read -l -d \t name shortname identifier
        printf "%s%s\t%s\n" (string replace -r -- '(.+),[^,]*$' '$1,' $tok_param) $tok_no_comma $identifier $name
    end
end

complete -c tshark -s 2 -d 'Perform a two-pass analysis'
# This is fairly expensive, but only done upon the user pressing tab.
complete -c tshark -s e -d 'Add a field to the list of fields to display' -xa '(command tshark -G fields 2> /dev/null | awk -F\t \'{print $3"\t"$2}\')'
complete -c tshark -s E -d 'Set an option controlling the printing of fields' -xa '
bom=y\t"Prepend output with the UTF-8 byte order mark"
header=y\t"Print a list of the selected field names"
separator=\t"Set the separator character to use for fields"
occurrence=\t"Select which occurrence to use for fields that have multiple: f=first, l=last, a=all"
aggregator=\t"Set the aggregator character to use for fields that have multiple occurrences"
quote=\t"Set the quote character to use to surround fields d=\", s=\', n=no quotes"'

complete -c tshark -s F -d 'Set the output capture file format' -xa '(command tshark -F 2>| string replace -rf "\s+(\S+) - (.*)" \'$1\t$2\')'
complete -c tshark -s G -d 'Print a glossary' -xa '(
printf "help\tList available report types\n"
command tshark -G help 2>/dev/null | string replace -rf "\s+-G (\S+)\s+(.*)" \'$1\t$2\'
)'
complete -c tshark -s H -d 'Read a list of entries from a "hosts" file' -r
complete -c tshark -s j -d 'Protocol match filter used for ek|json|jsonraw|pdml output file types' -x
complete -c tshark -s J -d 'Protocol match filter used, includes all child protocols' -x
complete -c tshark -s l -d 'Flush the standard output after the information for each packet is printed'
complete -c tshark -s O -d 'Show a detailed view of the comma-separated list of protocols' -xa '(__fish_tshark_protocols)'
complete -c tshark -s P -l print -d 'Decode and display packet summary or details'
complete -c tshark -s Q -d "When capturing packets, don't display, on the standard error, the initial message"
complete -c tshark -s S -d 'Set the line separator to be printed between packets' -x
complete -c tshark -s T -d 'Set the format of the output when viewing decoded packet data' -xa '
ek\t"Newline delimited JSON for bulk import into Elasticsearch"
fields\t"The values of fields specified with the -e option, in a form specified by the -E option"
json\t"JSON file format"
jsonraw\t"JSON file format including only raw hex-encoded packet data"
pdml\t"Packet Details Markup Language, an XML-based format for the details of a decoded packet"
ps\t"PostScript for a human-readable summary of each of the packets"
psml\t"Packet Summary Markup Language"
tabs\t"Tab-separated human-readable one-line packet summaries"
text\t"Default"'

complete -c tshark -s U -d "PDUs export according to given tap name" -xa '(
    printf "%s\tTap name\n" (command tshark -U "" 2>| string replace -rf "^tshark:\s*" "")[2..-1])'
complete -c tshark -s V -d 'Causes TShark to print a view of packet details'
complete -c tshark -s W -d 'Save extra information in the capture file if the format supports it' -xa n
complete -c tshark -s x -d 'Print hex and ASCII dumps of packet data'
complete -c tshark -s M -d 'Automatically reset internal session when reaching N packets' -x
complete -c tshark -l color -d 'Enable coloring of packets according to standard Wireshark color filters'
complete -c tshark -l no-duplicate-keys -d 'Merge duplicate keys in json output'
complete -c tshark -l elastic-mapping-filter -d 'Only put specified protocols in an ElasticSearch mapping file' -x # TODO
complete -c tshark -l export-objects -d 'Export all objects within a protocol into directory given destination directory' -x # TODO
