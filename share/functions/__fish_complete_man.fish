
function __fish_complete_man
	# Try to guess what section to search in. If we don't know, we
	# use [^)]*, which should match any section

	set section ""
	set prev (commandline -poc)
	set -e prev[1]
	while set -q prev[1]
		switch $prev[1]
			case '-**'

			case '*'
				set section $prev[1]
		end
		set -e prev[1]
	end

    set section $section"[^)]*"

    set -l token (commandline -ct)
    # If we don't have a token but a section, list all pages for that section.
    # Don't do it for all sections because that would be overwhelming.
    if test -z "$token" -a "$section" != "[^)]*"
        set token "."
    end

	if test -n "$token"
		# Do the actual search
		apropos $token ^/dev/null | awk '
		BEGIN { FS="[\t ]- "; OFS="\t"; }
		# BSD/Darwin
		/^[^( \t]+\('$section'\)/ {
		  split($1, pages, ", ");
		  for (i in pages) {
		    page = pages[i];
		    sub(/[ \t]+/, "", page);
		    paren = index(page, "(");
		    name = substr(page, 1, paren - 1);
		    sect = substr(page, paren + 1, length(page) - paren - 1);
		    print name, sect ": " $2;
		  }
		}
		# man-db
		/^[^( \t]+ +\('$section'\)/ {
		  split($1, t, " ");
		  sect = substr(t[2], 2, length(t[2]) - 2);
		  print t[1], sect ": " $2;
		}
		# man-db RHEL 5 with [aliases]
		/^[^( \t]+ +\[.*\] +\('$section'\)/ {
		  split($1, t, " ");
		  sect = substr(t[3], 2, length(t[3]) - 2);
		  print t[1], sect ": " $2;
		}
		# Solaris 11
		# Does not display descriptions
		# Solaris apropos outputs embedded backspace in descriptions
		/^[0-9]+\. [^( \t]*\('$section'\) / {
		  split($1, t, " ")
		  paren = index(t[2], "(");
		  name = substr(t[2], 1, paren - 1);
		  sect = substr(t[2], paren + 1, length(t[2]) - paren - 1);
		  print name, sect
		}
		'
    else
        return 1
	end
    return 0
end

