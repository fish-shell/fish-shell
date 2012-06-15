function __fish_complete_proc --description 'Complete by list of running processes'
	if test (uname) = Darwin
		ps -ao ucomm | awk 'FNR==1 { next } { gsub(/ +$/,"") } 1' | sort -u
	else
		ps a --no-headers --format comm | sort -u
	end
end
