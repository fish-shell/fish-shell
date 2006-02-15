# Bittorrent commands

function __fish_complete_bittorrent
	complete -c $argv -l max_uploads -x -d (_ "Maximum uploads at once")
	complete -c $argv -l keepalive_interval -x -d (_ "Number of seconds between keepalives")
	complete -c $argv -l download_slice_size -x -d (_ "Bytes per request")
	complete -c $argv -l request_backlog -x -d (_ "Requests per pipe")
	complete -c $argv -l max_message_length -x -d (_ "Maximum length prefix encoding")
	complete -c $argv -l ip -d (_ "IP to report to the tracker") -x -a "(__fish_print_addresses)"
	complete -c $argv -l minport -d (_ "Minimum port to listen to")
	complete -c $argv -l maxport -d (_ "Maximum port to listen to")
	complete -c $argv -l responsefile -r -d (_ "File for server response")
	complete -c $argv -l url -x -d (_ "URL to get file from")
	complete -c $argv -l saveas -r -d (_ "Local file target")
	complete -c $argv -l timeout -x -d (_ "Time to close inactive socket")
	complete -c $argv -l timeout_check_interval -x -d (_ "Time between checking timeouts")
	complete -c $argv -l max_slice_length -x -d (_ "Maximum outgoing slice length")
	complete -c $argv -l max_rate_period -x -d (_ "Maximum time to guess rate")
	complete -c $argv -l bind -x -d (_ "IP to bind to locally") -a "(__fish_print_addresses)"
	complete -c $argv -l display_interval -x -d (_ "Time between screen updates")
	complete -c $argv -l rerequest_interval -x -d (_ "Time to wait between requesting more peers")
	complete -c $argv -l min_peers -x -d (_ "Minimum number of peers to not do requesting")
	complete -c $argv -l http_timeout -x -d (_ "Number of seconds before assuming http timeout")
	complete -c $argv -l max_initiate -x -d (_ "Number of peers at which to stop initiating new connections")
	complete -c $argv -l max_allow_in -x -d (_ "Maximum number of connections to allow")
	complete -c $argv -l check_hashes -x -d (_ "Whether to check hashes on disk")
	complete -c $argv -l max_upload_rate -x -d (_ "Maximum kB/s to upload at")
	complete -c $argv -l snub_time -x -d (_ "Seconds to wait for data to come in before assuming choking")
	complete -c $argv -l spew -x -d (_ "Whether to display diagnostic info")
	complete -c $argv -l rarest_first_cutoff -x -d (_ "Number of downloads at which to switch from random to rarest first")
	complete -c $argv -l min_uploads -x -d (_ "Number of uploads to fill out to with optimistic unchokes")
	complete -c $argv -l report_hash_failiures -x -d (_ "Whether to inform the user that hash failures occur")
end
