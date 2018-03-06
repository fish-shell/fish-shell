#!/usr/local/bin/fish
# Emits a safe version of completion functions that will not spew errors
# if the target is not installed.

if grep 'magic completion safety check' $argv[1]
	# file already processed, return as-is
	cat $argv[1]
	exit
end

set -l stage 0

for l in (cat $argv[1])
	if test $stage -eq 0
		if string match -qr '^\s*#' -- $l
			echo $l
			continue
		end
		set stage 1
	end

	if test $stage -eq 1
		# check if we already processed this file
		set -l cmd (basename -s .fish $argv[1])
		echo -e -n '\n# magic completion safety check (do not remove this comment)\n'
		echo -e -n "if not type -q $cmd\n    exit\nend\n"
		set stage 2
	end

	if test $stage -eq 2
		echo $l
	end
end
