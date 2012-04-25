
#
# Alias for gettext (or a fallback if gettext isn't installed)
#

set -l path (which gettext ^/dev/null)
if test -x (echo $path)
	function _ --description "Alias for the gettext command"
		gettext fish $argv
	end
else
	function _ --description "Alias for the gettext command"
		echo -n $argv
	end
end

