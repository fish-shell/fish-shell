function __fish_is_token_n --description 'Test if current token is on Nth place' --argument n
	# Add a fake element to increment without calling math
	set -l num (count (commandline -poc) additionalelement)
	test $n -eq $num
end
