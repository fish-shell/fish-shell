#apm
complete -f -c apm -s V -l version -d (N_ "Display version and exit")
complete -f -c apm -s v -l verbose -d (N_ "Print APM info")
complete -f -c apm -s m -l minutes -d (N_ "Print time remaining")
complete -f -c apm -s M -l monitor -d (N_ "Monitor status info")
complete -f -c apm -s S -l standby -d (N_ "Request APM standby mode")
complete -f -c apm -s s -l suspend -d (N_ "Request APM suspend mode")
complete -f -c apm -s d -l debug -d (N_ "APM status debugging info")
