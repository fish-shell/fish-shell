# Completions for the FreeBSD `kldload` kernel module load utility

# Only attempt to match a local file if there isn't a match in /boot/kernel,
# as odds are that is the desired source.
complete -c kldload -xa '(
	set results (__fish_complete_suffix /boot/kernel/(commandline -ct) ".ko" | sed "s@.*/@@g;s/\.ko//");
	set -q results[1]; and printf "%s\n" $results;
	or __fish_complete_suffix .ko
)'
