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

