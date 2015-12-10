if head --version >/dev/null ^/dev/null
	complete -c head -s c -l bytes -d 'Print the first N bytes; Leading '-', truncate the last N bytes' -r
	complete -c head -s n -l lines -d 'Print the first N lines; Leading '-', truncate the last N lines' -r
	complete -c head -s q -l quiet -l silent -d 'Never print file names'
	complete -c head -s v -l verbose -d 'Always print file names'
	complete -f -c head -l version -d 'Display version'
	complete -f -c head -l help -d 'Display help'
else # OSX and similar - no longopts (and fewer shortopts)
	complete -c head -s c -d 'Print the first N bytes' -r
	complete -c head -s n -d 'Print the first N lines' -r
end
