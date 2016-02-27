function __fish_urlencode --description "URL-encode stdin"
	set -l join ''
	set -l chars
	# Set locale to C and IFS to "" in order to split a line into bytes.
	while begin; set -lx LC_ALL C; set -lx IFS ''; read -az chars; end
		printf '%s' $join
		# chomp off a trailing newline
		if test "$chars[-1]" = \n
			set -e chars[-1]
			set join '%0A%00'
		else
			set join '%00'
		end
		for c in $chars
			if string match -q -r '[/._~A-Za-z0-9-]' $c
				printf '%s' $c
			else
				printf '%%%02X' "'$c"
			end
		end
	end
end
