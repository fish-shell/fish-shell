function contains_seq --description 'Return true if array contains a sequence'
	set -l printnext
	switch $argv[1]
	case --printnext
		set printnext[1] 1
		set -e argv[1]
	end
	set -l pattern
	set -l string
	set -l dest pattern
	for i in $argv
		if test "$i" = --
			set dest string
			continue
		end
		set $dest $$dest $i
	end
	set -l nomatch 1
	set -l i 1
	for s in $string
		if set -q printnext[2]
			return 0
		end
		if test "$s" = "$pattern[$i]"
			set -e nomatch[1]
			set i (math $i + 1)
			if not set -q pattern[$i]
				if set -q printnext[1]
					set printnext[2] 1
					continue
				end
				return 0
			end
		else
			if not set -q nomatch[1]
				set nomatch 1
				set i 1
			end
		end
	end
	if set -q printnext[1]
		echo ''
	end
	set -q printnext[2]
end

