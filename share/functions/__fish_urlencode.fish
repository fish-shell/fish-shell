function __fish_urlencode --description "URL-encode stdin"
	while read f
		set lines (echo "$f" | sed -E -e 's/./\n\\0/g;/^$/d;s/\n//')
		if [ (count $lines) -gt 0 ]
			printf '%%%02x' "'"$lines"'" | sed -e 's/%2[fF]/\//g';
		end
	end
	echo
end
