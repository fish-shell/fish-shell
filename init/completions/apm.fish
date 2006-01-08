#apm
complete -f -c apm -s V -l version -d (_ "Display version and exit")
complete -f -c apm -s v -l verbose -d (_ "print APM info")
complete -f -c apm -s m -l minutes -d (_ "print time remaining")
complete -f -c apm -s M -l monitor -d (_ "monitor status info")
complete -f -c apm -s S -l standby -d (_ "request APM standby mode")
complete -f -c apm -s s -l suspend -d (_ "request APM suspend mode")
complete -f -c apm -s d -l debug -d (_ "APM status debugging info")
