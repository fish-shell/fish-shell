function __fish_sysadminctl_verb
    set -l toks (commandline -xpc)
    set -e toks[1]
    while set -q toks[1]
        switch $toks[1]
            case -adminUser -adminPassword
                set -e toks[1..2]
            case interactive
                set -e toks[1]
            case '-*'
                string sub -s 2 -- $toks[1]
                return
            case '*'
                set -e toks[1]
        end
    end
end

function __fish_sysadminctl_using_verb
    test "$(__fish_sysadminctl_verb)" = "$argv[1]"
end

function __fish_sysadminctl_users
    dscl . -list /Users | string match -rv '^_'
end

complete -c sysadminctl -f
complete -c sysadminctl -o addUser -x -d 'Create a new local user account'
complete -c sysadminctl -o deleteUser -x -a '(__fish_sysadminctl_users)' -d 'Delete a local user account'
complete -c sysadminctl -o newPassword -x -d 'Change the current user password (requires -oldPassword)'
complete -c sysadminctl -o resetPasswordFor -x -a '(__fish_sysadminctl_users)' -d 'Reset the password for a local user account'
complete -c sysadminctl -o secureTokenStatus -x -a '(__fish_sysadminctl_users)' -d 'Show Secure Token status for a user'
complete -c sysadminctl -o secureTokenOn -x -a '(__fish_sysadminctl_users)' -d 'Enable Secure Token for a user'
complete -c sysadminctl -o secureTokenOff -x -a '(__fish_sysadminctl_users)' -d 'Disable Secure Token for a user'
complete -c sysadminctl -o autologin -x -a 'set off status' -d 'Configure automatic login (set/off/status)'
complete -c sysadminctl -o guestAccount -x -a 'on off status' -d 'Enable, disable, or show Guest Account status'
complete -c sysadminctl -o afpGuestAccess -x -a 'on off status' -d 'Enable, disable, or show AFP guest access'
complete -c sysadminctl -o smbGuestAccess -x -a 'on off status' -d 'Enable, disable, or show SMB guest access'
complete -c sysadminctl -o automaticTime -x -a 'on off status' -d 'Enable, disable, or show automatic time setting'
complete -c sysadminctl -o use12HourClockForLoginWindow -x -a 'on off status' -d 'Enable, disable, or show 12-hour clock on login window'
complete -c sysadminctl -o filesystem -x -a status -d 'Show filesystem status'
complete -c sysadminctl -o screenLock -x -a 'status immediate off' -d 'Configure screen lock timeout (status/immediate/off/seconds)'

complete -c sysadminctl -d 'User to configure for automatic login (used with set)' -x -n '__fish_sysadminctl_using_verb autologin' -o userName
complete -c sysadminctl -d 'Password for the automatic-login user' -x -n '__fish_sysadminctl_using_verb autologin' -o password

complete -c sysadminctl -d 'Password required to change screen lock setting' -x -n '__fish_sysadminctl_using_verb screenLock' -o password

complete -c sysadminctl -d 'Securely erase the user home directory on deletion' -n '__fish_sysadminctl_using_verb deleteUser' -o secure
complete -c sysadminctl -d 'Keep the user home directory after deletion' -n '__fish_sysadminctl_using_verb deleteUser' -o keepHome

complete -c sysadminctl -d 'Full (display) name for the new user' -x -n '__fish_sysadminctl_using_verb addUser' -o fullName
complete -c sysadminctl -d 'Numeric user ID for the new account' -x -n '__fish_sysadminctl_using_verb addUser' -o UID
complete -c sysadminctl -d 'Primary group ID for the new account' -x -n '__fish_sysadminctl_using_verb addUser' -o GID
complete -c sysadminctl -d 'Login shell path for the new account' -r -n '__fish_sysadminctl_using_verb addUser' -o shell
complete -c sysadminctl -d 'Password for the new account' -x -n '__fish_sysadminctl_using_verb addUser' -o password
complete -c sysadminctl -d 'Password hint for the new account' -x -n '__fish_sysadminctl_using_verb addUser' -o hint
complete -c sysadminctl -d 'Full path to home directory for the new account' -r -n '__fish_sysadminctl_using_verb addUser' -o home
complete -c sysadminctl -d 'Grant administrator privileges to the new account' -n '__fish_sysadminctl_using_verb addUser' -o admin
complete -c sysadminctl -d 'Create as a role account (name must start with _, UID 450-499)' -n '__fish_sysadminctl_using_verb addUser' -o roleAccount
complete -c sysadminctl -d 'Full path to user account picture image' -r -n '__fish_sysadminctl_using_verb addUser' -o picture

complete -c sysadminctl -d 'Current (old) password' -x -n '__fish_sysadminctl_using_verb newPassword' -o oldPassword
complete -c sysadminctl -d 'New password hint' -x -n '__fish_sysadminctl_using_verb newPassword' -o passwordHint

complete -c sysadminctl -d 'Replacement password to set for the user' -x -n '__fish_sysadminctl_using_verb resetPasswordFor' -o newPassword
complete -c sysadminctl -d 'New password hint' -x -n '__fish_sysadminctl_using_verb resetPasswordFor' -o passwordHint

complete -c sysadminctl -x -n '__fish_sysadminctl_using_verb secureTokenOn; or __fish_sysadminctl_using_verb secureTokenOff' -o password -d 'Password for the target user'

complete -c sysadminctl -x -o adminUser -d 'Administrator user name for scripted authentication'
complete -c sysadminctl -d 'Administrator password (use - to be prompted interactively)' -x -o adminPassword
