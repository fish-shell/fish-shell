function __fish_urlencode --description "URL-encode stdin"
	set -l IFS ''
	set -l output
	while read --array --local lines
		if [ (count $lines) -gt 0 ]
			set output $output (printf '%%%02x' "'"$lines"'" | sed -e 's/%2[fF]/\//g')
		end
	end
	echo -s $output
end
