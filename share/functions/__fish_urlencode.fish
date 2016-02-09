function __fish_urlencode --description "URL-encode stdin"
	set -l output
	set -l chars
	# Set locale to C and IFS to "" in order to split a line into bytes.
	while begin; set -lx LC_ALL C; set -lx IFS ''; read --array chars; end
		if count $chars > /dev/null
			set output $output (printf '%%%02x' "'"$chars)
		end
	end
	echo -s $output | sed -e 's/%2[fF]/\//g'
end
