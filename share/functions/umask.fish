
function __fish_umask_parse -d "Internal umask function"
	# Test if already a valid octal mask, and pad it with zeros
	if echo $argv | __fish_sgrep -E '^0?[0-7]{1,3}$' >/dev/null
		set -l char_count (echo $argv| wc -c)
		for i in (seq (math 5 - $char_count)); set argv 0$argv; end
		echo $argv
	else
		# Test if argument really is a valid symbolic mask
		if not echo $argv | __fish_sgrep -E '^(((u|g|o|a|)(=|\+|-)|)(r|w|x)*)(,(((u|g|o|a|)(=|\+|-)|)(r|w|x)*))*$' >/dev/null
			printf (_ "%s: Invalid mask '%s'\n") umask $argv >&2
			return 1
		end

		set -l implicit_all

		# Insert inverted umask into res variable

		set -l mode
		set -l val
		set -l tmp $umask
		set -l res

		for i in 1 2 3
			set tmp (echo $tmp|cut -c 2-)
			set -l char_count (echo $tmp|cut -c 1)
			set res[$i] (math 7 - $char_count)
		end

		set -l el (echo $argv|tr , \n)
		for i in $el
			switch $i
				case 'u*'
					set idx 1
					set i (echo $i| cut -c 2-)

				case 'g*'
					set idx 2
					set i (echo $i| cut -c 2-)

				case 'o*'
					set idx 3
					set i (echo $i| cut -c 2-)

				case 'a*'
					set idx 1 2 3
					set i (echo $i| cut -c 2-)

				case '*'
					set implicit_all 1
					set idx 1 2 3
			end

			switch $i
				case '=*'
					set mode set
					set i (echo $i| cut -c 2-)

				case '+*'
					set mode add
					set i (echo $i| cut -c 2-)

				case '-*'
					set mode remove
					set i (echo $i| cut -c 2-)

				case '*'
					if not count $implicit_all >/dev/null
						printf (_ "%s: Invalid mask '%s'\n") umask $argv >&2
						return
					end
					set mode set
			end

			if not echo $perm| __fish_sgrep -E '^(r|w|x)*$' >/dev/null
				printf (_ "%s: Invalid mask '%s'\n") umask $argv >&2
				return
			end

			set val 0
			if echo $i | __fish_sgrep 'r' >/dev/null
				set val 4
			end
			if echo $i | __fish_sgrep 'w' >/dev/null
				set val (math $val + 2)
			end
			if echo $i | __fish_sgrep 'x' >/dev/null
				set val (math $val + 1)
			end

			for j in $idx
				switch $mode
					case set
						 set res[$j] $val

					case add
						set res[$j] (perl -e 'print( ( '$res[$j]'|'$val[$j]' )."\n" )')

					case remove
						set res[$j] (perl -e 'print( ( (7-'$res[$j]')&'$val[$j]' )."\n" )')
				end
			end
		end

		for i in 1 2 3
			set res[$i] (math 7 - $res[$i])
		end
		echo 0$res[1]$res[2]$res[3]
	end
end

function __fish_umask_print_symbolic
	set -l res ""
	set -l letter a u g o

	for i in 2 3 4
		set res $res,$letter[$i]=
		set val (echo $umask|cut -c $i)

		if contains $val 0 1 2 3
			set res {$res}r
		end

		if contains $val 0 1 4 5
			set res {$res}w
		end

		if contains $val 0 2 4 6
			set res {$res}x
		end

	end

	echo $res|cut -c 2-
end

function umask --description "Set default file permission mask"

	set -l as_command 0
	set -l symbolic 0
	
	set -l options
	set -l shortopt pSh
	if not getopt -T >/dev/null
		# GNU getopt
		set longopt -l as-command,symbolic,help
		set options -o $shortopt $longopt --
		# Verify options
		if not getopt -n umask $options $argv >/dev/null
			return 1
		end
	else
		# Old getopt, used on OS X
		set options $shortopt
		# Verify options
		if not getopt $options $argv >/dev/null
			return 1
		end
	end

	set -l tmp (getopt $options $argv)
	eval set opt $tmp

	while count $opt >/dev/null

		switch $opt[1]
			case -h --help
				__fish_print_help umask
				return 0

			case -p --as-command
				set as_command 1

			case -S --symbolic
				set symbolic 1

			case --
				set -e opt[1]
				break

		end

		set -e opt[1]

	end

	switch (count $opt)

		case 0
			if not set -q umask
				set -g umask 113
			end
			if test $as_command -eq 1
				echo umask $umask
			else
				if test $symbolic -eq 1
					__fish_umask_print_symbolic $umask
				else
					echo $umask
				end
			end

		case 1
			set -l parsed (__fish_umask_parse $opt)
			if test (count $parsed) -eq 1
				set -g umask $parsed
				return 0
			end
			return 1

		case '*'
			printf (_ '%s: Too many arguments\n') umask >&2

	end

end
