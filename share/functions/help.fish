
#
# help should use 'open' to find a suitable browser, but only
# if there is a mime database _and_ DISPLAY is set, since the
# browser will most likely be graphical. Since most systems which
# have a mime databe also have the htmlview program, this is mostly a
# theoretical problem.
#

function help -d (N_ "Show help for the fish shell")

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
				set argv help
		end
	end

	#
	# Find a suitable browser for viewing the help pages. This is needed
	# by the help function defined below.
	#
	set -l graphical_browsers htmlview x-www-browser firefox galeon mozilla konqueror epiphany opera netscape
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
	end

	if test -z $fish_browser
		printf (_ '%s: Could not find a web browser.\n') help
		printf (_ 'Please set the variable $BROWSER to a suitable browser and try again\n\n')
		return 1
	end

	set fish_help_item $argv[1]

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

		case (sed -n < $__fish_help_dir/commands.html -e "s/.*<h2><a class=\"anchor\" name=\"\([^\"]*\)\">.*/\1/p")
			set fish_help_page "commands.html\#$fish_help_item"
		case $help_topics
			set fish_help_page "index.html\#$fish_help_item"
		case "*"
			if type -f $fish_help_item >/dev/null
				man $fish_help_item
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

		eval $fish_browser file://$__fish_help_dir/$fish_help_page \&
	else
		eval $fish_browser file://$__fish_help_dir/$fish_help_page
	end
end
