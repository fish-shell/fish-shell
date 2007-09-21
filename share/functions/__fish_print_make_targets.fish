function __fish_print_make_targets
	set files Makefile makefile GNUmakefile
	sgrep -h -E '^[^#%=$[:space:]][^#%=$]*:([^=]|$)' $files | cut -d ":" -f 1 | sed -e 's/^ *//;s/ *$//;s/  */\n/g' ^/dev/null
end
