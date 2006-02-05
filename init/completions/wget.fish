#
# Completions for the wget command
#

complete -c wget -s V -l version -d (_ "Display version and exit")
complete -c wget -s h -l help -d (_ "Display help and exit")
complete -c wget -s b -l background -d (_ "Go to background immediately after startup")
complete -c wget -s e -l execute -d (_ "Execute command as if part of .wgetrc") -x
complete -c wget -s o -l output-file -d (_ "Log all messages to logfile") -r
complete -c wget -s a -l append-output -d (_ "Append all messages to logfile")
complete -c wget -s d -l debug -d (_ "Turn on debug output")
complete -c wget -s q -l quiet -d (_ "Quiet mode")
complete -c wget -s v -l verbose -d (_ "Verbose mode")
complete -c wget -l non-verbose -d (_ "Turn off verbose without being completely quiet")
complete -c wget -o nv -d (_ "Turn off verbose without being completely quiet")
complete -c wget -s i -l input-file -d (_ "Read URLs from file") -r
complete -c wget -s F -l force-html -d (_ "Force input to be treated as HTML")
complete -c wget -s B -l base -d (_ "Prepend string to relative links") -x
complete -c wget -l bind-adress -d (_ "Bind address on local machine") -xa "(__fish_print_addresses; __fish_print_hostnames)"
complete -c wget -s t -l tries -d (_ "Set number of retries to number") -xa "0 1 2 4 8 16 32 64 128"
complete -c wget -s O -l output-document -d (_ "Concatenate output to file") -r
complete -c wget -l no-clobber -d (_ "Never overwrite files with same name")
complete -c wget -o nc  -d (_ "Never overwrite files with same name")
complete -c wget -s c -l continue -d (_ "Continue getting a partially-downloaded file")
complete -c wget -l progress -d (_ "Select progress meter type") -a "
	dot\t'Print one dot for every kB of data, 50 dots per line'
	dot:default\t'Print one dot for every kB of data, 50 dots per line'
	dot:binary\t'Print one dot for every 8 kB of data, 48 dots per line'
	dot:mega\t'Print one dot for every 64 kB of data, 48 dots per line'
	bar\t'Print progress bar'
"
complete -c wget -s N -l timestamping -d (_ "Turn on time-stamping")
complete -c wget -s S -l server-response -d (_ "Print the headers/responses sent by servers")
complete -c wget -l spider -d (_ "Do not download the pages, just check that they are there")
complete -c wget -s T -l timeout -d (_ "Set the network timeout") -x
complete -c wget -l dns-timeout -d (_ "Set the DNS lookup timeout") -x
complete -c wget -l connect-timeout -d (_ "Set the connect timeout") -x
complete -c wget -l read-timeout -d (_ "Set the read (and write) timeout") -x
complete -c wget -l limit-rate -d (_ "Limit the download speed") -x
complete -c wget -s w -l wait -d (_ "Wait the specified number of seconds between the retrievals") -x
complete -c wget -l waitretry -d (_ "Wait time between retries") -x
complete -c wget -l random-wait -d (_ "Wait random amount of time between retrievals")
complete -c wget -s Y -l proxy -d (_ "Toggle proxy support") -xa "on off"
complete -c wget -s Q -l quota -d (_ "Specify download quota for automatic retrievals") -x
complete -c wget -l dns-cache -d (_ "Turn off caching of DNS lookups") -xa "off"
complete -c wget -l restrict-file-names -d (_ "Change which characters found in remote URLs may show up in local file names") -a "
	unix\t'Escape slash and non-printing characters'
	windows\t'Escape most non-alphabetical characters'
"

# HTTP options

complete -c wget -l no-directories -d (_ "Do not create a hierarchy of directories")
complete -c wget -o nd -d (_ "Do not create a hierarchy of directories")
complete -c wget -s x -l force-directories -d (_ "Force creation of a hierarchy of directories")
complete -c wget -l no-host-directories -d (_ "Disable generation of host-prefixed directories")
complete -c wget -o nH -d (_ "Disable generation of host-prefixed directories")
complete -c wget -l protocal-directories -d (_ "Use the protocol name as a directory component")
complete -c wget -l cut-dirs -d (_ "Ignore specified number of directory components") -xa "1 2 3 4 5"
complete -c wget -s P -l directory-prefix -d (_ "Set directory prefix") -r
complete -c wget -s E -l html-extension -d (_ "Force html files to have html extension")
complete -c wget -l http-user -d (_ "Specify the http username") -xa "(__fish_complete_users)"
complete -c wget -l http-passwd -d (_ "Specify the http password") -x
complete -c wget -l no-cache -d (_ "Disable server-side cache")
complete -c wget -l no-cookies -d (_ "Disable the use of cookies")
complete -c wget -l load-cookies -d (_ "Load cookies from file") -r
complete -c wget -l save-cookies -d (_ "Save cookies to file")
complete -c wget -l keep-session-cookies -d (_ "Save session cookies")
complete -c wget -l ignore-length -d (_ "Ignore 'Content-Length' header")
complete -c wget -l header -d (_ "Define an additional-header to be passed to the HTTP servers") -x
complete -c wget -l proxy-user -d (_ "Specify the proxy username") -xa "(__fish_complete_users)"
complete -c wget -l proxy-password -d (_ "Specify the proxy password") -x
complete -c wget -l referer -d (_ "Set referer URL") -x
complete -c wget -l save-headers -d (_ "Save the headers sent by the HTTP server")
complete -c wget -s U -l user-agent -d (_ "Identify as agent-string") -x
complete -c wget -l post-data -d (_ "Use POST as the method for all HTTP requests and send the specified data in the request body") -x
complete -c wget -l post-file -d (_ "Use POST as the method for all HTTP requests and send the specified data in the request body") -r
complete -c wget -l no-http-keep-alive -d (_ "Turn off keep-alive for http downloads")

#FTP options

complete -c wget -l no-remove-listing -d (_ "Don't remove the temporary .listing files generated")
complete -c wget -l no-glob -d (_ "Turn off FTP globbing")
complete -c wget -l passive-ftp -d (_ "Use the passive FTP retrieval scheme")
complete -c wget -l retr-symlinks -d (_ "Traverse symlinks and retrieve pointed-to files")

# Recursive options

complete -c wget -s r -l recursive -d (_ "Turn on recursive retrieving")
complete -c wget -n '__fish_contains_opt -s r recursive' -s l -l level -d (_ "Specify recursion maximum depth") -x
complete -c wget -l delete-after -d (_ "Delete every single file downloaded")
complete -c wget -s k -l convert-links -d (_ "Convert the links in the document to make them suitable for local viewing")
complete -c wget -s K -l backup-converted -d (_ "Back up the original version")
complete -c wget -s m -l mirror -d (_ "Turn on options suitable for mirroring")
complete -c wget -s p -l page-requisites -d (_ "Download all the files that are necessary to properly display a given HTML page")
complete -c wget -l strict-comments -d (_ "Turn on strict parsing of HTML comments")

#Recursive accept/reject options

complete -c wget -s A -l accept -d (_ "Comma-separated lists of file name suffixes or patterns to accept") -x
complete -c wget -s R -l reject -d (_ "Comma-separated lists of file name suffixes or patterns to reject") -x
complete -c wget -s D -l domains -d (_ "Set domains to be followed") -x
complete -c wget -l exclude-domains -d (_ "Specify the domains that are not to be followed") -x
complete -c wget -l follow-ftp -d (_ "Follow FTP links from HTML documents")
complete -c wget -l follow-tags -d (_ "HTML tags to follow") -x
complete -c wget -l ignore-tags -d (_ "HTML tags to ignore") -x
complete -c wget -s H -l span-hosts -d (_ "Enable spanning across hosts")
complete -c wget -s L -l relative -d (_ "Follow relative links only")
complete -c wget -s I -l include-directories -d (_ "Specify a comma-separated list of directories you wish to follow") -x
complete -c wget -s X -l exclude-directories -d (_ "Specify a comma-separated list of directories you wish to exclude") -x
complete -c wget -l no-parent -d (_ "Do not ever ascend to the parent directory")
complete -c wget -o np  -d (_ "Do not ever ascend to the parent directory")

