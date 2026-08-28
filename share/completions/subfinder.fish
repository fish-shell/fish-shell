# https://github.com/projectdiscovery/subfinder
# https://docs.projectdiscovery.io/opensource/subfinder

complete -c subfinder -f
complete -c subfinder -o h -o help -l help -d "Display help and exit"

# INPUT
complete -c subfinder -o d -o domain -x -d "Domains to find subdomains for"
complete -c subfinder -o dL -o list -r -d "File containing list of domains"

# SOURCE
complete -c subfinder -o s -o sources -x -d "Specific sources to use for discovery"
complete -c subfinder -o recursive -d "Use only sources that can handle subdomains recursively"
complete -c subfinder -o all -d "Use all sources for enumeration (slow)"
complete -c subfinder -o es -o exclude-sources -x -d "Sources to exclude from enumeration"

# FILTER
complete -c subfinder -o m -o match -x -d "Subdomains to match"
complete -c subfinder -o f -o filter -x -d "Subdomains to filter"

# RATE-LIMIT
complete -c subfinder -o rl -o rate-limit -x -d "Maximum HTTP requests per second (global)"
complete -c subfinder -o rls -o rate-limits -x -d "Maximum HTTP requests per second for providers (key=value)"
complete -c subfinder -o t -x -d "Number of concurrent goroutines for resolving (-active only)"

# UPDATE
complete -c subfinder -o up -o update -d "Update subfinder to latest version"
complete -c subfinder -o duc -o disable-update-check -d "Disable automatic update check"

# OUTPUT
complete -c subfinder -o o -o output -r -d "File to write output to"
complete -c subfinder -o oJ -o json -d "Write output in JSONL format"
complete -c subfinder -o oD -o output-dir -x -a "(__fish_complete_directories)" -d "Directory to write output (-dL only)"
complete -c subfinder -o cs -o collect-sources -d "Include all sources in the output (-json only)"
complete -c subfinder -o oI -o ip -d "Include host IP in output (-active only)"

# CONFIGURATION
complete -c subfinder -o config -r -d "Flag config file"
complete -c subfinder -o pc -o provider-config -r -d "Provider config file"
complete -c subfinder -o r -x -d "Comma separated list of resolvers"
complete -c subfinder -o rL -o rlist -r -d "File containing list of resolvers"
complete -c subfinder -o nW -o active -d "Display active subdomains only"
complete -c subfinder -o proxy -x -d "HTTP proxy to use"
complete -c subfinder -o ei -o exclude-ip -d "Exclude IPs from the list of domains"
complete -c subfinder -o mr -o max-results -x -d "Limit the number of results per source"

# DEBUG
complete -c subfinder -o silent -d "Show only subdomains"
complete -c subfinder -o version -d "Display version and exit"
complete -c subfinder -o v -d "Show verbose output"
complete -c subfinder -o nc -o no-color -d "Disable color in output"
complete -c subfinder -o ls -o list-sources -d "List all available sources"
complete -c subfinder -o stats -d "Report source statistics"

# OPTIMIZATION
complete -c subfinder -o timeout -x -d "Seconds to wait before timing out"
complete -c subfinder -o max-time -x -d "Minutes to wait for enumeration results"
complete -c subfinder -o rsr -o response-size-read -x -d "Max response body size to read (bytes)"
