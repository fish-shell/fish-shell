
function __fish_complete_apropos
	if test (commandline -ct)
		set str (commandline -ct)
		apropos $str 2>/dev/null |sed -e "s/^\(.*$str\([^ ]*\).*\)\$/$str\2"\t"\1/"
	end
end

complete -xc apropos -a '(__fish_complete_apropos)' -d "whatis entry"

complete -c apropos -s h -l help -d "Display help and exit"
complete -f -c apropos -s d -l debug -d "Print debugging info"
complete -f -c apropos -s v -l verbose -d "Verbose mode"
complete -f -c apropos -s r -l regex -d "Keyword as regex"
complete -f -c apropos -s w -l wildcard -d "Keyword as wildcards"
complete -f -c apropos -s e -l exact -d "Keyword as exactly match"
complete -x -c apropos -s m -l system -d "Search for other system"
complete -x -c apropos -s M -l manpath -a '(echo $MANPATH)' -d "Specify man path"
complete -x -c apropos -s C -l config-file -d "Specify a configuration file"
complete -f -c apropos -s V -l version -d "Display version and exit"

