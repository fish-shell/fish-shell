#
# Completions for the wget command
#

complete -c wget -s V -l version -d "Display version and exit"
complete -c wget -s h -l help -d "Display help and exit"
complete -c wget -s b -l background -d "Go to background immediately after startup"
complete -c wget -s e -l execute -d "Execute command as if part of .wgetrc" -x
complete -c wget -s o -l output-file -d "Log all messages to logfile" -r
complete -c wget -s a -l append-output -d "Append all messages to logfile"
complete -c wget -s d -l debug -d "Turn on debug output"
complete -c wget -s q -l quiet -d "Quiet mode"
complete -c wget -s v -l verbose -d "Verbose mode"
complete -c wget -l non-verbose -d "Turn off verbose without being completely quiet"
complete -c wget -o nv -d "Turn off verbose without being completely quiet"
complete -c wget -s i -l input-file -d "Read URLs from file" -r
complete -c wget -s F -l force-html -d "Force input to be treated as HTML"
complete -c wget -s B -l base -d "Prepend string to relative links" -x
complete -c wget -l bind-address -d "Bind address on local machine" -xa "(__fish_print_addresses; __fish_print_hostnames)"
complete -c wget -s t -l tries -d "Set number of retries to number" -xa "0 1 2 4 8 16 32 64 128"
complete -c wget -s O -l output-document -d "Concatenate output to file" -r
complete -c wget -l no-clobber -d "Never overwrite files with same name"
complete -c wget -o nc -d "Never overwrite files with same name"
complete -c wget -s c -l continue -d "Continue getting a partially-downloaded file"
complete -c wget -l progress -d "Select progress meter type" -a "
	dot\t'Print one dot for every kB of data, 50 dots per line'
	dot:default\t'Print one dot for every kB of data, 50 dots per line'
	dot:binary\t'Print one dot for every 8 kB of data, 48 dots per line'
	dot:mega\t'Print one dot for every 64 kB of data, 48 dots per line'
	bar\t'Print progress bar'
"
complete -c wget -s N -l timestamping -d "Turn on time-stamping"
complete -c wget -s S -l server-response -d "Print the headers/responses sent by servers"
complete -c wget -l spider -d "Do not download the pages, just check that they are there"
complete -c wget -s T -l timeout -d "Set the network timeout" -x
complete -c wget -l dns-timeout -d "Set the DNS lookup timeout" -x
complete -c wget -l connect-timeout -d "Set the connect timeout" -x
complete -c wget -l read-timeout -d "Set the read (and write) timeout" -x
complete -c wget -l limit-rate -d "Limit the download speed" -x
complete -c wget -s w -l wait -d "Wait the specified number of seconds between the retrievals" -x
complete -c wget -l waitretry -d "Wait time between retries" -x
complete -c wget -l random-wait -d "Wait random amount of time between retrievals"
complete -c wget -s Y -l proxy -d "Toggle proxy support" -xa "on off"
complete -c wget -s Q -l quota -d "Specify download quota for automatic retrievals" -x
complete -c wget -l dns-cache -d "Turn off caching of DNS lookups" -xa off
complete -c wget -l restrict-file-names -d "Change which characters found in remote URLs may show up in local file names" -a "
	unix\t'Escape slash and non-printing characters'
	windows\t'Escape most non-alphabetical characters'
"

# HTTP options

complete -c wget -l no-directories -d "Do not create a hierarchy of directories"
complete -c wget -o nd -d "Do not create a hierarchy of directories"
complete -c wget -s x -l force-directories -d "Force creation of a hierarchy of directories"
complete -c wget -l no-host-directories -d "Disable generation of host-prefixed directories"
complete -c wget -o nH -d "Disable generation of host-prefixed directories"
complete -c wget -l protocol-directories -d "Use the protocol name as a directory component"
complete -c wget -l cut-dirs -d "Ignore specified number of directory components" -xa "1 2 3 4 5"
complete -c wget -s P -l directory-prefix -d "Set directory prefix" -r
complete -c wget -s E -l html-extension -d "Force html files to have html extension"
complete -c wget -l http-user -d "Specify the http username" -xa "(__fish_complete_users)"
complete -c wget -l http-passwd -d "Specify the http password" -x
complete -c wget -l no-cache -d "Disable server-side cache"
complete -c wget -l no-cookies -d "Disable the use of cookies"
complete -c wget -l load-cookies -d "Load cookies from file" -r
complete -c wget -l save-cookies -d "Save cookies to file"
complete -c wget -l keep-session-cookies -d "Save session cookies"
complete -c wget -l ignore-length -d "Ignore 'Content-Length' header"
complete -c wget -l header -d "Define an additional-header to be passed to the HTTP servers" -x
complete -c wget -l proxy-user -d "Specify the proxy username" -xa "(__fish_complete_users)"
complete -c wget -l proxy-password -d "Specify the proxy password" -x
complete -c wget -l referer -d "Set referer URL" -x
complete -c wget -l save-headers -d "Save the headers sent by the HTTP server"
complete -c wget -s U -l user-agent -d "Identify as agent-string" -x
complete -c wget -l post-data -d "Use POST for all HTTP requests and send the specified data in the request body" -x
complete -c wget -l post-file -d "Use POST for all HTTP requests and send the specified file in the request body" -r
complete -c wget -l no-http-keep-alive -d "Turn off keep-alive for http downloads"

# HTTPS options

complete -c wget -f -r -l secure-protocol -a 'auto SSLv2 SSLv3 TLSv1 PFS' -d "Choose secure protocol"
complete -c wget -f -l https-only -d "Only follow secure HTTPS links"
complete -c wget -f -l no-check-certificate -d "Don't validate the server's certificate"
complete -c wget -r -l certificate -d "Client certificate file"
complete -c wget -f -r -l certificate-type --arguments 'PEM DER' -d "Client certificate type"
complete -c wget -r -l private-key -d "Private key file"
complete -c wget -f -r -l private-key-type --arguments 'PEM DER' -d "Private key type"
complete -c wget -r -l ca-certificate -d "File with the bundle of CAs"
complete -c wget -r -l ca-directory -d "Directory where hash list of CAs is stored"
complete -c wget -r -l crl-file -d "File with bundle of CRLs"
complete -c wget -r -l random-file -d "File with random data for seeding the SSL PRNG"
complete -c wget -r -l egd-file -d "File naming the EGD socket with random data"

# HSTS options

complete -c wget -f -l no-hsts -d "Disable HSTS"
complete -c wget -l hsts-file -d "Path of HSTS database"

#FTP options

complete -c wget -l no-remove-listing -d "Don't remove the temporary .listing files generated"
complete -c wget -l no-glob -d "Turn off FTP globbing"
complete -c wget -l passive-ftp -d "Use the passive FTP retrieval scheme"
complete -c wget -l retr-symlinks -d "Traverse symlinks and retrieve pointed-to files"

# Recursive options

complete -c wget -s r -l recursive -d "Turn on recursive retrieving"
complete -c wget -n '__fish_contains_opt -s r recursive' -s l -l level -d "Specify recursion maximum depth" -x
complete -c wget -l delete-after -d "Delete every single file downloaded"
complete -c wget -s k -l convert-links -d "Convert the links in the document to make them suitable for local viewing"
complete -c wget -s K -l backup-converted -d "Back up the original version"
complete -c wget -s m -l mirror -d "Turn on options suitable for mirroring"
complete -c wget -s p -l page-requisites -d "Download all the files that are necessary to properly display a given HTML page"
complete -c wget -l strict-comments -d "Turn on strict parsing of HTML comments"

#Recursive accept/reject options

complete -c wget -s A -l accept -d "Comma-separated lists of file name suffixes or patterns to accept" -x
complete -c wget -s R -l reject -d "Comma-separated lists of file name suffixes or patterns to reject" -x
complete -c wget -s D -l domains -d "Set domains to be followed" -x
complete -c wget -l exclude-domains -d "Specify the domains that are not to be followed" -x
complete -c wget -l follow-ftp -d "Follow FTP links from HTML documents"
complete -c wget -l follow-tags -d "HTML tags to follow" -x
complete -c wget -l ignore-tags -d "HTML tags to ignore" -x
complete -c wget -s H -l span-hosts -d "Enable spanning across hosts"
complete -c wget -s L -l relative -d "Follow relative links only"
complete -c wget -s I -l include-directories -d "Specify a comma-separated list of directories you wish to follow" -x
complete -c wget -s X -l exclude-directories -d "Specify a comma-separated list of directories you wish to exclude" -x
complete -c wget -l no-parent -d "Do not ever ascend to the parent directory"
complete -c wget -o np -d "Do not ever ascend to the parent directory"
