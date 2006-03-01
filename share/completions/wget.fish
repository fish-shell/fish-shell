#
# Completions for the wget command
#

complete -c wget -s V -l version -d (N_ "Display version and exit")
complete -c wget -s h -l help -d (N_ "Display help and exit")
complete -c wget -s b -l background -d (N_ "Go to background immediately after startup")
complete -c wget -s e -l execute -d (N_ "Execute command as if part of .wgetrc") -x
complete -c wget -s o -l output-file -d (N_ "Log all messages to logfile") -r
complete -c wget -s a -l append-output -d (N_ "Append all messages to logfile")
complete -c wget -s d -l debug -d (N_ "Turn on debug output")
complete -c wget -s q -l quiet -d (N_ "Quiet mode")
complete -c wget -s v -l verbose -d (N_ "Verbose mode")
complete -c wget -l non-verbose -d (N_ "Turn off verbose without being completely quiet")
complete -c wget -o nv -d (N_ "Turn off verbose without being completely quiet")
complete -c wget -s i -l input-file -d (N_ "Read URLs from file") -r
complete -c wget -s F -l force-html -d (N_ "Force input to be treated as HTML")
complete -c wget -s B -l base -d (N_ "Prepend string to relative links") -x
complete -c wget -l bind-adress -d (N_ "Bind address on local machine") -xa "(__fish_print_addresses; __fish_print_hostnames)"
complete -c wget -s t -l tries -d (N_ "Set number of retries to number") -xa "0 1 2 4 8 16 32 64 128"
complete -c wget -s O -l output-document -d (N_ "Concatenate output to file") -r
complete -c wget -l no-clobber -d (N_ "Never overwrite files with same name")
complete -c wget -o nc  -d (N_ "Never overwrite files with same name")
complete -c wget -s c -l continue -d (N_ "Continue getting a partially-downloaded file")
complete -c wget -l progress -d (N_ "Select progress meter type") -a "
	dot\t'Print one dot for every kB of data, 50 dots per line'
	dot:default\t'Print one dot for every kB of data, 50 dots per line'
	dot:binary\t'Print one dot for every 8 kB of data, 48 dots per line'
	dot:mega\t'Print one dot for every 64 kB of data, 48 dots per line'
	bar\t'Print progress bar'
"
complete -c wget -s N -l timestamping -d (N_ "Turn on time-stamping")
complete -c wget -s S -l server-response -d (N_ "Print the headers/responses sent by servers")
complete -c wget -l spider -d (N_ "Do not download the pages, just check that they are there")
complete -c wget -s T -l timeout -d (N_ "Set the network timeout") -x
complete -c wget -l dns-timeout -d (N_ "Set the DNS lookup timeout") -x
complete -c wget -l connect-timeout -d (N_ "Set the connect timeout") -x
complete -c wget -l read-timeout -d (N_ "Set the read (and write) timeout") -x
complete -c wget -l limit-rate -d (N_ "Limit the download speed") -x
complete -c wget -s w -l wait -d (N_ "Wait the specified number of seconds between the retrievals") -x
complete -c wget -l waitretry -d (N_ "Wait time between retries") -x
complete -c wget -l random-wait -d (N_ "Wait random amount of time between retrievals")
complete -c wget -s Y -l proxy -d (N_ "Toggle proxy support") -xa "on off"
complete -c wget -s Q -l quota -d (N_ "Specify download quota for automatic retrievals") -x
complete -c wget -l dns-cache -d (N_ "Turn off caching of DNS lookups") -xa "off"
complete -c wget -l restrict-file-names -d (N_ "Change which characters found in remote URLs may show up in local file names") -a "
	unix\t'Escape slash and non-printing characters'
	windows\t'Escape most non-alphabetical characters'
"

# HTTP options

complete -c wget -l no-directories -d (N_ "Do not create a hierarchy of directories")
complete -c wget -o nd -d (N_ "Do not create a hierarchy of directories")
complete -c wget -s x -l force-directories -d (N_ "Force creation of a hierarchy of directories")
complete -c wget -l no-host-directories -d (N_ "Disable generation of host-prefixed directories")
complete -c wget -o nH -d (N_ "Disable generation of host-prefixed directories")
complete -c wget -l protocal-directories -d (N_ "Use the protocol name as a directory component")
complete -c wget -l cut-dirs -d (N_ "Ignore specified number of directory components") -xa "1 2 3 4 5"
complete -c wget -s P -l directory-prefix -d (N_ "Set directory prefix") -r
complete -c wget -s E -l html-extension -d (N_ "Force html files to have html extension")
complete -c wget -l http-user -d (N_ "Specify the http username") -xa "(__fish_complete_users)"
complete -c wget -l http-passwd -d (N_ "Specify the http password") -x
complete -c wget -l no-cache -d (N_ "Disable server-side cache")
complete -c wget -l no-cookies -d (N_ "Disable the use of cookies")
complete -c wget -l load-cookies -d (N_ "Load cookies from file") -r
complete -c wget -l save-cookies -d (N_ "Save cookies to file")
complete -c wget -l keep-session-cookies -d (N_ "Save session cookies")
complete -c wget -l ignore-length -d (N_ "Ignore 'Content-Length' header")
complete -c wget -l header -d (N_ "Define an additional-header to be passed to the HTTP servers") -x
complete -c wget -l proxy-user -d (N_ "Specify the proxy username") -xa "(__fish_complete_users)"
complete -c wget -l proxy-password -d (N_ "Specify the proxy password") -x
complete -c wget -l referer -d (N_ "Set referer URL") -x
complete -c wget -l save-headers -d (N_ "Save the headers sent by the HTTP server")
complete -c wget -s U -l user-agent -d (N_ "Identify as agent-string") -x
complete -c wget -l post-data -d (N_ "Use POST as the method for all HTTP requests and send the specified data in the request body") -x
complete -c wget -l post-file -d (N_ "Use POST as the method for all HTTP requests and send the specified data in the request body") -r
complete -c wget -l no-http-keep-alive -d (N_ "Turn off keep-alive for http downloads")

#FTP options

complete -c wget -l no-remove-listing -d (N_ "Don't remove the temporary .listing files generated")
complete -c wget -l no-glob -d (N_ "Turn off FTP globbing")
complete -c wget -l passive-ftp -d (N_ "Use the passive FTP retrieval scheme")
complete -c wget -l retr-symlinks -d (N_ "Traverse symlinks and retrieve pointed-to files")

# Recursive options

complete -c wget -s r -l recursive -d (N_ "Turn on recursive retrieving")
complete -c wget -n '__fish_contains_opt -s r recursive' -s l -l level -d (N_ "Specify recursion maximum depth") -x
complete -c wget -l delete-after -d (N_ "Delete every single file downloaded")
complete -c wget -s k -l convert-links -d (N_ "Convert the links in the document to make them suitable for local viewing")
complete -c wget -s K -l backup-converted -d (N_ "Back up the original version")
complete -c wget -s m -l mirror -d (N_ "Turn on options suitable for mirroring")
complete -c wget -s p -l page-requisites -d (N_ "Download all the files that are necessary to properly display a given HTML page")
complete -c wget -l strict-comments -d (N_ "Turn on strict parsing of HTML comments")

#Recursive accept/reject options

complete -c wget -s A -l accept -d (N_ "Comma-separated lists of file name suffixes or patterns to accept") -x
complete -c wget -s R -l reject -d (N_ "Comma-separated lists of file name suffixes or patterns to reject") -x
complete -c wget -s D -l domains -d (N_ "Set domains to be followed") -x
complete -c wget -l exclude-domains -d (N_ "Specify the domains that are not to be followed") -x
complete -c wget -l follow-ftp -d (N_ "Follow FTP links from HTML documents")
complete -c wget -l follow-tags -d (N_ "HTML tags to follow") -x
complete -c wget -l ignore-tags -d (N_ "HTML tags to ignore") -x
complete -c wget -s H -l span-hosts -d (N_ "Enable spanning across hosts")
complete -c wget -s L -l relative -d (N_ "Follow relative links only")
complete -c wget -s I -l include-directories -d (N_ "Specify a comma-separated list of directories you wish to follow") -x
complete -c wget -s X -l exclude-directories -d (N_ "Specify a comma-separated list of directories you wish to exclude") -x
complete -c wget -l no-parent -d (N_ "Do not ever ascend to the parent directory")
complete -c wget -o np  -d (N_ "Do not ever ascend to the parent directory")

