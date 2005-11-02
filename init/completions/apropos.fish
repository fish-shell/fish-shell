
function __fish_complete_apropos
	if test (commandline -ct)
		set str (commandline -ct)
		apropos $str|sed -e "s/^\(.*$str\([^ ]*\).*\)$/$str\2\t\1/"
	end
end

complete -xc apropos -a '(__fish_complete_apropos)' -d "Whatis entry"

complete -c apropos -s h -l help -d "apropos command help"
complete -f -c apropos -s d -l debug -d "print debugging info"
complete -f -c apropos -s v -l verbose -d "print verbose warning"
complete -f -c apropos -s r -l regex -d "keyword as regex"
complete -f -c apropos -s w -l wildcard -d "keyword as wildwards"
complete -f -c apropos -s e -l exact -d "keyword as exactly match"
complete -x -c apropos -s m -l system -d "search for other system"
complete -x -c apropos -s M -l manpath -a '(echo $MANPATH)' -d "specify man path"
complete -x -c apropos -s C -l config-file -d "specify a conf file"
complete -f -c apropos -s V -l version -d "Display version"

