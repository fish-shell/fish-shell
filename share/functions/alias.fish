
function alias -d (_ "Legacy function for creating shellscript functions using an alias-like syntax")
	set -l name
	set -l body
	switch (count $argv)

		case 1
			set -l tmp (echo $argv|sed -e "s/\([^=]\)=/\1\n/")
			set name $tmp[1]
			set body $tmp[2]

		case 2
			set name $argv[1]
			set body $argv[2]

		case \*
			printf ( _ "%s: Expected one or two arguments, got %d") alias (count $argv)
			return 1
	end

	eval "function $name; $body \$argv; end"
end
