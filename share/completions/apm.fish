#apm
complete -f -c apm -s V -l version --description "Display version and exit"
complete -f -c apm -s v -l verbose --description "Print APM info"
complete -f -c apm -s m -l minutes --description "Print time remaining"
complete -f -c apm -s M -l monitor --description "Monitor status info"
complete -f -c apm -s S -l standby --description "Request APM standby mode"
complete -f -c apm -s s -l suspend --description "Request APM suspend mode"
complete -f -c apm -s d -l debug --description "APM status debugging info"
