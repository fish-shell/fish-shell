complete -x -c wvdial -a "(cat ~/.wvdialrc | grep '\[Dialer' | sed 's/\[Dialer \(.\+\)\]/\1/')" --description "wvdial connections"
