complete -c speedtest-cli -s h -l help -d 'Display help and exit'
complete -c speedtest-cli -l no-download -d 'Do not perform download test'
complete -c speedtest-cli -l no-upload -d 'Do not perform upload test'
complete -c speedtest-cli -l single -d 'Only use a single connection. Simulates typical file transfer'
complete -c speedtest-cli -l bytes -d 'Display values in bytes. Ignored by JSON or CSV output'
complete -c speedtest-cli -l share -d 'Generate a URL to the speedtest.net share results image'
complete -c speedtest-cli -l simple -d 'Suppress verbose output'
complete -c speedtest-cli -l csv -d 'Only show basic information in CSV format (bits/s)'
complete -c speedtest-cli -l csv-delimiter -a "',' ';' ' '" -d 'Single character delimiter to use in CSV output' -rf
complete -c speedtest-cli -l csv-header -d 'Print CSV headers'
complete -c speedtest-cli -l json -d 'Only show basic information in JSON format (bits/s)'
complete -c speedtest-cli -l list -d 'Display a list of speedtest.net servers sorted by distance'
complete -c speedtest-cli -l server -d 'Specify a server ID to test against' -rf
complete -c speedtest-cli -l exclude -d 'Exclude a server from selection' -rf
complete -c speedtest-cli -l mini -d 'URL of the Speedtest Mini server' -rf
complete -c speedtest-cli -l source -d 'Source IP address to bind to' -rf
complete -c speedtest-cli -l timeout -d 'HTTP timeout in seconds' -rf
complete -c speedtest-cli -l secure -d 'Use HTTPS instead of HTTP with speedtest.net operated servers'
complete -c speedtest-cli -l no-pre-allocate -d 'Do not pre allocate upload data'
complete -c speedtest-cli -l version -d 'Show the version number and exit'
