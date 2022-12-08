complete -c reflector -f
# options
complete -c reflector -s h -l help -d 'Show help'
complete -c reflector -l connection-timeout -d 'The number of seconds to wait before a connection times out'
complete -c reflector -l download-timeout -d 'The number of seconds to wait before a download times out'
complete -c reflector -l list-countries -d 'Display a table of the distribution of servers by country'
complete -c reflector -l cache-timeout -d 'The cache timeout in seconds for the data retrieved from the Arch Linux Mirror Status API'
complete -c reflector -l url -d 'The URL from which to retrieve the mirror data in JSON format'
complete -c reflector -l save -d 'Save the mirrorlist to the given path'
complete -c reflector -l sort -d 'Sort the mirrorlist' -xa 'age rate country score delay'
complete -c reflector -l threads -d 'The number of threads to use for downloading'
complete -c reflector -l verbose -d 'Print extra information'
complete -c reflector -l info -d 'Print mirror information instead of a mirror list'

# filters
complete -c reflector -s a -l age -d 'Only return mirrors that have synchronized in the last n hours'
complete -c reflector -l delay -d 'Only return mirrors with a reported sync delay of n hours or less, where n is a float'
complete -c reflector -s c -l country -d 'Restrict mirrors to selected countries' -xa "(reflector --list-countries | cut -f 1 -d ' ' | tail -n +3)"
complete -c reflector -s f -l fastest -d 'Return the n fastest mirrors that meet the other criteria'
complete -c reflector -s i -l include -d 'Include servers that match <regex>'
complete -c reflector -s x -l exclude -d 'Exclude servers that match <regex>'
complete -c reflector -s l -l latest -d 'Limit the list to the n most recently synchronized servers'
complete -c reflector -l score -d 'Limit the list to the n servers with the highest score'
complete -c reflector -s n -l number -d 'Return at most n mirrors'
complete -c reflector -s p -l protocol -d 'Match one of the given protocols' -xa 'http https ftp rsync'
complete -c reflector -l completion-percent -d 'Set the minimum completion percent for the returned mirrors'
complete -c reflector -l isos -d 'Only return mirrors that host ISOs'
complete -c reflector -l ipv4 -d 'Only return mirrors that support IPv4'
complete -c reflector -l ipv6 -d 'Only return mirrors that support IPv6'
