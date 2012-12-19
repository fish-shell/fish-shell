
#
# help should use 'open' to find a suitable browser, but only
# if there is a mime database _and_ DISPLAY is set, since the
# browser will most likely be graphical. Since most systems which
# have a mime databe also have the htmlview program, this is mostly a
# theoretical problem.
#

function help --description "Show help for the fish shell"

	# Declare variables to set correct scope
	set -l fish_browser
	set -l fish_browser_bg

	set -l h syntax completion editor job-control todo bugs history killring help
	set h $h color prompt title variables builtin-overview changes expand
	set h $h expand-variable expand-home expand-brace expand-wildcard
	set -l help_topics $h expand-command-substitution expand-process

	# 'help -h' should launch 'help help'
	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help help
				return 0
		end
	end

	#
	# Find a suitable browser for viewing the help pages. This is needed
	# by the help function defined below.
	#
	set -l graphical_browsers htmlview x-www-browser firefox galeon mozilla konqueror epiphany opera netscape rekonq
	set -l text_browsers htmlview www-browser links elinks lynx w3m

	if test $BROWSER
		# User has manualy set a preferred browser, so we respect that
		set fish_browser $BROWSER

		# If browser is known to be graphical, put into background
		if contains -- $BROWSER $graphical_browsers
			set fish_browser_bg 1
		end
	else
		# Check for a text-based browser.
		for i in $text_browsers
			if type -f $i >/dev/null
				set fish_browser $i
				break
			end
		end

		# If we are in a graphical environment, check if there is a graphical
		# browser to use instead.
		if test "$DISPLAY" -a \( "$XAUTHORITY" = "$HOME/.Xauthority" -o "$XAUTHORITY" = "" \)
			for i in $graphical_browsers
				if type -f $i >/dev/null
					set fish_browser $i
					set fish_browser_bg 1
					break
				end
			end
		end

		# If xdg-open is available, just use that
		if type xdg-open > /dev/null
			set fish_browser xdg-open
		end

		# On OS X, just use open
		if test (uname) = Darwin
			set fish_browser (which open)
		end

	end

	if test -z $fish_browser
		printf (_ '%s: Could not find a web browser.\n') help
		printf (_ 'Please set the variable $BROWSER to a suitable browser and try again\n\n')
		return 1
	end

	set -l fish_help_item $argv[1]

	switch "$fish_help_item"
		case ""
			set fish_help_page index.html
		case "."
			set fish_help_page "commands.html\#source"
		case difference
			set fish_help_page difference.html
		case globbing
			set fish_help_page "index.html\#expand"

		# This command substitution should locate all commands with
		# documentation.  It's a bit of a hack, since it relies on the
		# Doxygen markup format to never change.

                case (sed -n 's/.*<h[12]><a class="anchor" \(id\|name\)="\([^"]*\)">.*/\2/p' $__fish_help_dir/commands.html)
			set fish_help_page "commands.html\#$fish_help_item"
		case $help_topics
			set fish_help_page "index.html\#$fish_help_item"
		case "*"
			if type -f $fish_help_item >/dev/null
				# Prefer to use fish's man pages, to avoid
				# the annoying useless "builtin" man page bash
				# installs on OS X
				set -l man_arg "$__fish_datadir/man/man1/$fish_help_item.1"
				if test ! -f "$man_arg"
					set man_arg $fish_help_item
				end
				man $man_arg
				return
			end
			set fish_help_page "index.html"
	end

	if test $fish_browser_bg

		switch $fish_browser
			case 'htmlview' 'x-www-browser'
				printf (_ 'help: Help is being displayed in your default browser\n')

			case '*'
				printf (_ 'help: Help is being displayed in %s\n') $fish_browser

		end

		eval "$fish_browser file://$__fish_help_dir/$fish_help_page &"
	else
		eval $fish_browser file://$__fish_help_dir/$fish_help_page
	end
end
