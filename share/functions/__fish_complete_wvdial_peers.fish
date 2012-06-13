function __fish_complete_wvdial_peers --description 'Complete wvdial peers' --argument cfgfiles
	set -q cfgfiles[0]
	or set -l cfgfiles /etc/wvdial.conf ~/.wvdialrc

	# test if there is an alternative config file specified
	set -l store_next
	for opt in (commandline -cpo)
		if set -q store_next[1]
			set store_next
			set cfgfiles $opt
			continue
		end

		switch $opt
		case -C --config
			set store_next true
		case '--config=*'
			set cfgfiles (echo $opt | sed 's/--config=//')
		end
	end

	for file in $cfgfiles
        if test -f $file
            cat $file | grep '\[Dialer' | sed 's/\[Dialer \(.\+\)\]/\1/'
        end
	end | sort -u | grep -v Defaults


end
