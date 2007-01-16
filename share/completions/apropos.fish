
function __fish_complete_apropos
	if test (commandline -ct)
		set str (commandline -ct)
		apropos $str|sed -e "s/^\(.*$str\([^ ]*\).*\)\$/$str\2"\t"\1/"
	end
end

complete -xc apropos -a '(__fish_complete_apropos)' --description "whatis entry"

complete -c apropos -s h -l help --description "Display help and exit"
complete -f -c apropos -s d -l debug --description "Print debugging info"
complete -f -c apropos -s v -l verbose --description "Verbose mode"
complete -f -c apropos -s r -l regex --description "Keyword as regex"
complete -f -c apropos -s w -l wildcard --description "Keyword as wildcards"
complete -f -c apropos -s e -l exact --description "Keyword as exactly match"
complete -x -c apropos -s m -l system --description "Search for other system"
complete -x -c apropos -s M -l manpath -a '(echo $MANPATH)' --description "Specify man path"
complete -x -c apropos -s C -l config-file --description "Specify a configuration file"
complete -f -c apropos -s V -l version --description "Display version and exit"

