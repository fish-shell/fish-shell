function __fish_print_make_targets
	# Some seds (e.g. on Mac OS X), don't support \n in the RHS
	# Use a literal newline instead
	# http://sed.sourceforge.net/sedfaq4.html#s4.1
	# The 'rev | cut | rev' trick removes everything after the last colon
	for file in GNUmakefile Makefile makefile
		if test -f $file
			__fish_sgrep -h -o -E '^[^#%=$[:space:]][^#%=$]*:([^=]|$)' $file ^/dev/null | rev | cut -d ":" -f 2- | rev | sed -e 's/^ *//;s/ *$//;s/  */\\
/g' ^/dev/null
			# On case insensitive filesystems, Makefile and makefile are the same; stop now so we don't double-print 
			break
		end
	end
end

