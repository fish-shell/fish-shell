complete -c date -s d -l date -d (_ "Display date described by string") -x
complete -c date -s f -l file -d (_ "Display date for each line in file") -r
complete -c date -s I -l iso-8601 -d (_ "Output in ISO 8601 format") -x -a "date hours minutes seconds"
complete -c date -s s -l set -d (_ "Set time") -x
complete -c date -s R -l rfc-2822 -d (_ "Output RFC-2822 compliant date string")
complete -c date -s r -l reference -d (_ "Display the last modification time of file") -r
complete -c date -s u -l utc -d (_ "Print or set Coordinated Universal Time")
complete -c date -l universal -d (_ "Print or set Coordinated Universal Time")
complete -c date -s h -l help -d (_ "Display help and exit")
complete -c date -s v -l version -d (_ "Display version and exit")

