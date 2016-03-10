if date --version > /dev/null ^ /dev/null
	complete -c date -s d -l date --description "Display date described by string" -x
	complete -c date -s f -l file --description "Display date for each line in file" -r
	complete -c date -s I -l iso-8601 --description "Output in ISO 8601 format" -x -a "date hours minutes seconds"
	complete -c date -s s -l set --description "Set time" -x
	complete -c date -s R -l rfc-2822 --description "Output RFC-2822 compliant date string"
	complete -c date -s r -l reference --description "Display the last modification time of file" -r
	complete -c date -s u -l utc --description "Print or set Coordinated Universal Time"
	complete -c date -l universal --description "Print or set Coordinated Universal Time"
	complete -c date -s h -l help --description "Display help and exit"
	complete -c date -s v -l version --description "Display version and exit"
else # OS X
	complete -c date -s d -d 'Set the kernel\'s value for daylight saving time' -x
	complete -c date -s f -d 'Use format string to parse the date provided rather than default format'
	complete -c date -s j -d 'Do no try to set the date'
	complete -c date -s n -d 'Set the time for current machine only in the local group'
	complete -c date -s r -d 'Print date and time represented by SEC since Epoch' -x
	complete -c date -s t -d 'Set system\'s value for MINUTES west of GMT'
	complete -c date -s u -d 'Display or set the date int UTC time'
	complete -c date -s v -d 'Adjust the time component backward(-)/forward(+) according to VAL' -x
end
	
