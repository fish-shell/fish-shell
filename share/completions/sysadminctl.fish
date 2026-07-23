# sysadminctl - administer macOS user accounts and system settings (man 8 sysadminctl)
#
# sysadminctl uses single-dash long verbs (e.g. `sysadminctl -addUser alice`).
# The first dashed token is the verb; shared auth flags (-adminUser/-adminPassword)
# and per-verb sub-flags (-fullName, -UID, etc.) follow.

# ── command-line introspection ────────────────────────────────────────
# Print the verb (first dashed token after the command), without its dash.
function __fish_sysadminctl_verb
    set -l toks (commandline -opc)
    set -e toks[1]
    for t in $toks
        if string match -q -- '-*' $t
            string trim -l -c - -- $t
            return 0
        end
    end
    return 1
end

# True when no verb has been chosen yet (so verbs should be offered).
function __fish_sysadminctl_no_verb
    not __fish_sysadminctl_verb >/dev/null 2>&1
end

# ── live enumerators ──────────────────────────────────────────────────
# List non-system local users (exclude accounts starting with _).
function __fish_sysadminctl_users
    dscl . -list /Users 2>/dev/null | grep -v '^_'
end

# ── verbs ─────────────────────────────────────────────────────────────
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o addUser -d 'Create a new local user account'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o deleteUser -d 'Delete a local user account'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o newPassword -d 'Change the current user password (requires -oldPassword)'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o resetPasswordFor -d 'Reset the password for a local user account'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o secureTokenStatus -d 'Show Secure Token status for a user'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o secureTokenOn -d 'Enable Secure Token for a user'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o secureTokenOff -d 'Disable Secure Token for a user'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o autologin -d 'Configure automatic login (set/off/status)'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o guestAccount -d 'Enable, disable, or show Guest Account status'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o afpGuestAccess -d 'Enable, disable, or show AFP guest access'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o smbGuestAccess -d 'Enable, disable, or show SMB guest access'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o automaticTime -d 'Enable, disable, or show automatic time setting'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o use12HourClockForLoginWindow -d 'Enable, disable, or show 12-hour clock on login window'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o filesystem -d 'Show filesystem status'
complete -c sysadminctl -n __fish_sysadminctl_no_verb -o screenLock -d 'Configure screen lock timeout (status/immediate/off/seconds)'

# ── verb arguments: user name operand ────────────────────────────────
# Verbs that take an existing local username as their primary operand.
complete -c sysadminctl -x \
    -n '__fish_sysadminctl_verb | string match -qr \'^(deleteUser|resetPasswordFor|secureTokenStatus|secureTokenOn|secureTokenOff)$\'' \
    -a '(__fish_sysadminctl_users)'

# ── -autologin operand ────────────────────────────────────────────────
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q autologin' \
    -a set -d 'Configure automatic login for a user'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q autologin' \
    -a off -d 'Disable automatic login'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q autologin' \
    -a status -d 'Display current automatic login setting'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q autologin' \
    -o userName -d 'User to configure for automatic login (used with set)'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q autologin' \
    -o password -d 'Password for the automatic-login user'

# ── on|off|status toggle verbs ────────────────────────────────────────
complete -c sysadminctl -f \
    -n '__fish_sysadminctl_verb | string match -qr \'^(guestAccount|afpGuestAccess|smbGuestAccess|automaticTime|use12HourClockForLoginWindow)$\'' \
    -a on -d Enable
complete -c sysadminctl -f \
    -n '__fish_sysadminctl_verb | string match -qr \'^(guestAccount|afpGuestAccess|smbGuestAccess|automaticTime|use12HourClockForLoginWindow)$\'' \
    -a off -d Disable
complete -c sysadminctl -f \
    -n '__fish_sysadminctl_verb | string match -qr \'^(guestAccount|afpGuestAccess|smbGuestAccess|automaticTime|use12HourClockForLoginWindow)$\'' \
    -a status -d 'Display current status'

# ── -filesystem operand ───────────────────────────────────────────────
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q filesystem' \
    -a status -d 'Display filesystem status'

# ── -screenLock operand ───────────────────────────────────────────────
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q screenLock' \
    -a status -d 'Display current screen lock setting'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q screenLock' \
    -a immediate -d 'Lock screen immediately when screen saver activates'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q screenLock' \
    -a off -d 'Disable screen lock'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q screenLock' \
    -o password -d 'Password required to change screen lock setting'

# ── -deleteUser sub-flags ─────────────────────────────────────────────
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q deleteUser' \
    -o secure -d 'Securely erase the user home directory on deletion'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q deleteUser' \
    -o keepHome -d 'Keep the user home directory after deletion'

# ── -addUser sub-flags ────────────────────────────────────────────────
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o fullName -d 'Full (display) name for the new user'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o UID -d 'Numeric user ID for the new account'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o GID -d 'Primary group ID for the new account'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o shell -d 'Login shell path for the new account'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o password -d 'Password for the new account'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o hint -d 'Password hint for the new account'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o home -d 'Full path to home directory for the new account'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o admin -d 'Grant administrator privileges to the new account'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o roleAccount -d 'Create as a role account (name must start with _, UID 450-499)'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q addUser' \
    -o picture -d 'Full path to user account picture image'

# ── -newPassword sub-flags ────────────────────────────────────────────
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q newPassword' \
    -o oldPassword -d 'Current (old) password'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q newPassword' \
    -o passwordHint -d 'New password hint'

# ── -resetPasswordFor sub-flags ───────────────────────────────────────
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q resetPasswordFor' \
    -o newPassword -d 'Replacement password to set for the user'
complete -c sysadminctl -f -n '__fish_sysadminctl_verb | string match -q resetPasswordFor' \
    -o passwordHint -d 'New password hint'

# ── -secureTokenOn / -secureTokenOff sub-flags ────────────────────────
complete -c sysadminctl -f \
    -n '__fish_sysadminctl_verb | string match -qr \'^secureToken(On|Off)$\'' \
    -o password -d 'Password for the target user'

# ── shared authentication flags ───────────────────────────────────────
# Accepted alongside any verb that performs a privileged operation.
complete -c sysadminctl -f -n 'not __fish_sysadminctl_no_verb' \
    -o adminUser -d 'Administrator user name for scripted authentication'
complete -c sysadminctl -f -n 'not __fish_sysadminctl_no_verb' \
    -o adminPassword -d 'Administrator password (use - to be prompted interactively)'
