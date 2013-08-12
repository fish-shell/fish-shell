# Query for USERDOMAIN to shorten waiting times when OS isn't Windows.
if set -q USERDOMAIN; and test (uname -o) = Cygwin
	# Cygwin's hostname is broken when computer name contains Unicode
	# characters. This hack "fixes" hostname in Cygwin.
	function hostname --description "Show or set the system's host name"
		echo $USERDOMAIN
	end
end
