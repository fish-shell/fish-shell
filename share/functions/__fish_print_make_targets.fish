function __fish_print_make_targets
	set files Makefile makefile GNUmakefile
	# Some seds (e.g. on Mac OS X), don't support \n in the RHS
	# Use a literal newline instead
	# http://sed.sourceforge.net/sedfaq4.html#s4.1
	sgrep -h -E '^[^#%=$[:space:]][^#%=$]*:([^=]|$)' $files ^/dev/null | cut -d ":" -f 1 | sed -e 's/^ *//;s/ *$//;s/  */\\
/g' ^/dev/null
end
