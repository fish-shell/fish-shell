# Query for USERDOMAIN to shorten waiting times when OS isn't Windows.
set -q USERDOMAIN
and switch (uname)
    case 'CYGWIN_*'
        # Cygwin's hostname is broken when computer name contains Unicode
        # characters. This hack "fixes" hostname in Cygwin.
        function hostname --description "Show or set the system's host name"
            echo $USERDOMAIN
        end
end
