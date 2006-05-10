
function __fish_complete_apropos
	if test (commandline -ct)
		set str (commandline -ct)
		apropos $str|sed -e "s/^\(.*$str\([^ ]*\).*\)\$/$str\2"\t"\1/"
	end
end

complete -xc apropos -a '(__fish_complete_apropos)' -d (N_ "whatis entry")

complete -c apropos -s h -l help -d (N_ "Display help and exit")
complete -f -c apropos -s d -l debug -d (N_ "Print debugging info")
complete -f -c apropos -s v -l verbose -d (N_ "Verbose mode")
complete -f -c apropos -s r -l regex -d (N_ "Keyword as regex")
complete -f -c apropos -s w -l wildcard -d (N_ "Keyword as wildcards")
complete -f -c apropos -s e -l exact -d (N_ "Keyword as exactly match")
complete -x -c apropos -s m -l system -d (N_ "Search for other system")
complete -x -c apropos -s M -l manpath -a '(echo $MANPATH)' -d (N_ "Specify man path")
complete -x -c apropos -s C -l config-file -d (N_ "Specify a configuration file")
complete -f -c apropos -s V -l version -d (N_ "Display version and exit")

