# Under (ba)sh, `hash` acts like a caching `which`, but it doesn't print
# anything on success. Its return code is used to determine success vs failure.
function hash
	if not set -q argv[1]
		echo "Usage: hash FILE" >&2
		return 1
	end
	which $argv[1] 2>/dev/null 1>/dev/null
	or printf "%s: not found!\n" $argv[1] >&2 && return 2 # ENOENT
end
