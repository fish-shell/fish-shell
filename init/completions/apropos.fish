
function __fish_complete_apropos
	if test (commandline -ct)
		set str (commandline -ct)
		apropos $str|sed -e "s/^\(.*$str\([^ ]*\).*\)$/$str\2\t\1/"
	end
end

complete -xc apropos -a '(__fish_complete_apropos)' -d (_ "whatis entry")

complete -c apropos -s h -l help -d (_ "Display help and exit")
complete -f -c apropos -s d -l debug -d (_ "Print debugging info")
complete -f -c apropos -s v -l verbose -d (_ "Verbose mode")
complete -f -c apropos -s r -l regex -d (_ "Keyword as regex")
complete -f -c apropos -s w -l wildcard -d (_ "Keyword as wildwards")
complete -f -c apropos -s e -l exact -d (_ "Keyword as exactly match")
complete -x -c apropos -s m -l system -d (_ "Search for other system")
complete -x -c apropos -s M -l manpath -a '(echo $MANPATH)' -d (_ "Specify man path")
complete -x -c apropos -s C -l config-file -d (_ "Specify a configuration file")
complete -f -c apropos -s V -l version -d (_ "Display version and exit")

