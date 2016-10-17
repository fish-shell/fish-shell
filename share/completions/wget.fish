#
# Completions for the wget command
#

complete -c wget -s V -l version --description "Display version and exit"
complete -c wget -s h -l help --description "Display help and exit"
complete -c wget -s b -l background --description "Go to background immediately after startup"
complete -c wget -s e -l execute --description "Execute command as if part of .wgetrc" -x
complete -c wget -s o -l output-file --description "Log all messages to logfile" -r
complete -c wget -s a -l append-output --description "Append all messages to logfile"
complete -c wget -s d -l debug --description "Turn on debug output"
complete -c wget -s q -l quiet --description "Quiet mode"
complete -c wget -s v -l verbose --description "Verbose mode"
complete -c wget -l non-verbose --description "Turn off verbose without being completely quiet"
complete -c wget -o nv --description "Turn off verbose without being completely quiet"
complete -c wget -s i -l input-file --description "Read URLs from file" -r
complete -c wget -s F -l force-html --description "Force input to be treated as HTML"
complete -c wget -s B -l base --description "Prepend string to relative links" -x
complete -c wget -l bind-adress --description "Bind address on local machine" -xa "(__fish_print_addresses; __fish_print_hostnames)"
complete -c wget -s t -l tries --description "Set number of retries to number" -xa "0 1 2 4 8 16 32 64 128"
complete -c wget -s O -l output-document --description "Concatenate output to file" -r
complete -c wget -l no-clobber --description "Never overwrite files with same name"
complete -c wget -o nc  --description "Never overwrite files with same name"
complete -c wget -s c -l continue --description "Continue getting a partially-downloaded file"
complete -c wget -l progress --description "Select progress meter type" -a "
	dot\t'Print one dot for every kB of data, 50 dots per line'
	dot:default\t'Print one dot for every kB of data, 50 dots per line'
	dot:binary\t'Print one dot for every 8 kB of data, 48 dots per line'
	dot:mega\t'Print one dot for every 64 kB of data, 48 dots per line'
	bar\t'Print progress bar'
"
complete -c wget -s N -l timestamping --description "Turn on time-stamping"
complete -c wget -s S -l server-response --description "Print the headers/responses sent by servers"
complete -c wget -l spider --description "Do not download the pages, just check that they are there"
complete -c wget -s T -l timeout --description "Set the network timeout" -x
complete -c wget -l dns-timeout --description "Set the DNS lookup timeout" -x
complete -c wget -l connect-timeout --description "Set the connect timeout" -x
complete -c wget -l read-timeout --description "Set the read (and write) timeout" -x
complete -c wget -l limit-rate --description "Limit the download speed" -x
complete -c wget -s w -l wait --description "Wait the specified number of seconds between the retrievals" -x
complete -c wget -l waitretry --description "Wait time between retries" -x
complete -c wget -l random-wait --description "Wait random amount of time between retrievals"
complete -c wget -s Y -l proxy --description "Toggle proxy support" -xa "on off"
complete -c wget -s Q -l quota --description "Specify download quota for automatic retrievals" -x
complete -c wget -l dns-cache --description "Turn off caching of DNS lookups" -xa "off"
complete -c wget -l restrict-file-names --description "Change which characters found in remote URLs may show up in local file names" -a "
	unix\t'Escape slash and non-printing characters'
	windows\t'Escape most non-alphabetical characters'
"

# HTTP options

complete -c wget -l no-directories --description "Do not create a hierarchy of directories"
complete -c wget -o nd --description "Do not create a hierarchy of directories"
complete -c wget -s x -l force-directories --description "Force creation of a hierarchy of directories"
complete -c wget -l no-host-directories --description "Disable generation of host-prefixed directories"
complete -c wget -o nH --description "Disable generation of host-prefixed directories"
complete -c wget -l protocal-directories --description "Use the protocol name as a directory component"
complete -c wget -l cut-dirs --description "Ignore specified number of directory components" -xa "1 2 3 4 5"
complete -c wget -s P -l directory-prefix --description "Set directory prefix" -r
complete -c wget -s E -l html-extension --description "Force html files to have html extension"
complete -c wget -l http-user --description "Specify the http username" -xa "(__fish_complete_users)"
complete -c wget -l http-passwd --description "Specify the http password" -x
complete -c wget -l no-cache --description "Disable server-side cache"
complete -c wget -l no-cookies --description "Disable the use of cookies"
complete -c wget -l load-cookies --description "Load cookies from file" -r
complete -c wget -l save-cookies --description "Save cookies to file"
complete -c wget -l keep-session-cookies --description "Save session cookies"
complete -c wget -l ignore-length --description "Ignore 'Content-Length' header"
complete -c wget -l header --description "Define an additional-header to be passed to the HTTP servers" -x
complete -c wget -l proxy-user --description "Specify the proxy username" -xa "(__fish_complete_users)"
complete -c wget -l proxy-password --description "Specify the proxy password" -x
complete -c wget -l referer --description "Set referer URL" -x
complete -c wget -l save-headers --description "Save the headers sent by the HTTP server"
complete -c wget -s U -l user-agent --description "Identify as agent-string" -x
complete -c wget -l post-data --description "Use POST as the method for all HTTP requests and send the specified data in the request body" -x
complete -c wget -l post-file --description "Use POST as the method for all HTTP requests and send the specified data in the request body" -r
complete -c wget -l no-http-keep-alive --description "Turn off keep-alive for http downloads"

# HTTPS options

complete -c wget -f -r -l secure-protocol -a 'auto SSLv2 SSLv3 TLSv1 PFS' --description "Choose secure protocol"
complete -c wget -f -l https-only --description "Only follow secure HTTPS links"
complete -c wget -f -l no-check-certificate --description "Don't validate the server's certificate"
complete -c wget -r -l certificate --description "Client certificate file"
complete -c wget -f -r -l certificate-type --arguments 'PEM DER' --description "Client certificate type"
complete -c wget -r -l private-key --description "Private key file"
complete -c wget -f -r -l private-key-type --arguments 'PEM DER' --description "Private key type"
complete -c wget -r -l ca-certificate --description "File with the bundle of CAs"
complete -c wget -r -l ca-directory --description "Directory where hash list of CAs is stored"
complete -c wget -r -l crl-file --description "File with bundle of CRLs"
complete -c wget -r -l random-file --description "File with random data for seeding the SSL PRNG"
complete -c wget -r -l egd-file --description "File naming the EGD socket with random data"

# HSTS options

complete -c wget -f -l no-hsts --description "Disable HSTS"
complete -c wget -l hsts-file --description "Path of HSTS database"

#FTP options

complete -c wget -l no-remove-listing --description "Don't remove the temporary .listing files generated"
complete -c wget -l no-glob --description "Turn off FTP globbing"
complete -c wget -l passive-ftp --description "Use the passive FTP retrieval scheme"
complete -c wget -l retr-symlinks --description "Traverse symlinks and retrieve pointed-to files"

# Recursive options

complete -c wget -s r -l recursive --description "Turn on recursive retrieving"
complete -c wget -n '__fish_contains_opt -s r recursive' -s l -l level --description "Specify recursion maximum depth" -x
complete -c wget -l delete-after --description "Delete every single file downloaded"
complete -c wget -s k -l convert-links --description "Convert the links in the document to make them suitable for local viewing"
complete -c wget -s K -l backup-converted --description "Back up the original version"
complete -c wget -s m -l mirror --description "Turn on options suitable for mirroring"
complete -c wget -s p -l page-requisites --description "Download all the files that are necessary to properly display a given HTML page"
complete -c wget -l strict-comments --description "Turn on strict parsing of HTML comments"

#Recursive accept/reject options

complete -c wget -s A -l accept --description "Comma-separated lists of file name suffixes or patterns to accept" -x
complete -c wget -s R -l reject --description "Comma-separated lists of file name suffixes or patterns to reject" -x
complete -c wget -s D -l domains --description "Set domains to be followed" -x
complete -c wget -l exclude-domains --description "Specify the domains that are not to be followed" -x
complete -c wget -l follow-ftp --description "Follow FTP links from HTML documents"
complete -c wget -l follow-tags --description "HTML tags to follow" -x
complete -c wget -l ignore-tags --description "HTML tags to ignore" -x
complete -c wget -s H -l span-hosts --description "Enable spanning across hosts"
complete -c wget -s L -l relative --description "Follow relative links only"
complete -c wget -s I -l include-directories --description "Specify a comma-separated list of directories you wish to follow" -x
complete -c wget -s X -l exclude-directories --description "Specify a comma-separated list of directories you wish to exclude" -x
complete -c wget -l no-parent --description "Do not ever ascend to the parent directory"
complete -c wget -o np  --description "Do not ever ascend to the parent directory"

