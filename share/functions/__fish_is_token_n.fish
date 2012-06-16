function __fish_is_token_n --description 'Test if current token is on Nth place' --argument n
	set -l num (count (commandline -poc))
	#if test $cur
	expr $n = $num + 1 > /dev/null
	#else
	#expr $n '=' $num + 1 > /dev/null
	#end


end
