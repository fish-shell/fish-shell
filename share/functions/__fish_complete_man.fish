
function __fish_complete_man
	if test (commandline -ct)

		# Try to guess what section to search in. If we don't know, we
		# use [^)]*, which should match any section

		set section ""
		set prev (commandline -poc)
		set -e prev[1]
		while count $prev
			switch $prev[1]
			case '-**'

			case '*'
				set section $prev[1]
			end
			set -e prev[1]
		end

		set section $section"[^)]*"

		# Do the actual search
		switch (uname)
		case Darwin
			apropos (commandline -ct) ^/dev/null | sgrep \^(commandline -ct) | awk -F " - " '
{
	split($1, a, ", ");
	for (i in a) {
		if (sub("\\\('$section'[^)]*\\\)", "\t&: ", a[i]) == 0) continue;
		sub("\\\(", "", a[i]);
		sub("\\\)", "", a[i]);
		print a[i]$2
	}
}'
		case '*'
			apropos (commandline -ct) ^/dev/null | sgrep \^(commandline -ct) | sed -n -e 's/\([^ ]*\).*(\('$section'\)) *- */\1'\t'\2: /p'
		end
	end
end

