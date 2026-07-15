# systemsetup - configure macOS machine settings (man 8 systemsetup)
#
# systemsetup uses single-dash long verbs (e.g. `systemsetup -getdate`).
# The first dashed token is the verb; some verbs take an argument such as
# on|off, minutes, a timezone, or a startup-disk path.

# ── command-line introspection ───────────────────────────────────────
# Print the verb (first dashed token after the command), without its dash.
function __fish_systemsetup_verb
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
function __fish_systemsetup_no_verb
    not __fish_systemsetup_verb >/dev/null 2>&1
end

# ── live enumerators ─────────────────────────────────────────────────
function __fish_systemsetup_timezones
    systemsetup -listtimezones 2>/dev/null | tail -n +2 | string match -rv 'administrator access|exiting'
end

function __fish_systemsetup_startupdisks
    systemsetup -liststartupdisks 2>/dev/null | string match -rv 'administrator access|exiting'
end

# ── verbs ────────────────────────────────────────────────────────────
complete -c systemsetup -n __fish_systemsetup_no_verb -o getdate -d 'Display the current date'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setdate -d 'Set the current date (mm:dd:yy)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o gettime -d 'Display the current time in 24-hour format'
complete -c systemsetup -n __fish_systemsetup_no_verb -o settime -d 'Set the current time (hh:mm:ss)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o gettimezone -d 'Display the current time zone'
complete -c systemsetup -n __fish_systemsetup_no_verb -o listtimezones -d 'List all time zones supported by this machine'
complete -c systemsetup -n __fish_systemsetup_no_verb -o settimezone -d 'Set the local time zone'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getusingnetworktime -d 'Display whether network time is on or off'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setusingnetworktime -d 'Set network time to on or off'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getnetworktimeserver -d 'Display the currently set network time server'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setnetworktimeserver -d 'Set the network time server (IP or DNS name)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getsleep -d 'Display idle time until machine sleeps'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setsleep -d 'Set idle minutes until computer sleeps (or Never/Off)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getcomputersleep -d 'Display idle time until computer sleeps'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setcomputersleep -d 'Set idle minutes until computer sleeps (or Never/Off)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getdisplaysleep -d 'Display idle time until display sleeps'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setdisplaysleep -d 'Set idle minutes until display sleeps (or Never/Off)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getharddisksleep -d 'Display idle time until hard disk sleeps'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setharddisksleep -d 'Set idle minutes until hard disk sleeps (or Never/Off)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getwakeonmodem -d 'Display whether wake on modem is on or off'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setwakeonmodem -d 'Set whether the server wakes from sleep on modem activity'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getwakeonnetworkaccess -d 'Display whether wake on network access is on or off'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setwakeonnetworkaccess -d 'Set whether server wakes from sleep on network admin packet'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getrestartpowerfailure -d 'Display whether restart on power failure is on or off'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setrestartpowerfailure -d 'Set whether server auto-restarts after a power failure'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getrestartfreeze -d 'Display whether restart on freeze is on or off'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setrestartfreeze -d 'Set whether server restarts automatically after a system freeze'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getallowpowerbuttontosleepcomputer -d 'Display whether power button can sleep the computer'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setallowpowerbuttontosleepcomputer -d 'Enable or disable the power button sleeping the computer'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getremotelogin -d 'Display whether remote login (SSH) is on or off'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setremotelogin -d 'Set remote login (SSH) on or off (requires Full Disk Access)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getremoteappleevents -d 'Display whether remote Apple Events are on or off'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setremoteappleevents -d 'Set whether server responds to remote Apple Events'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getcomputername -d 'Display the computer name'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setcomputername -d 'Set the computer name (used by AFP)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getlocalsubnetname -d 'Display the local subnet name'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setlocalsubnetname -d 'Set the local subnet name'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getstartupdisk -d 'Display the current startup disk'
complete -c systemsetup -n __fish_systemsetup_no_verb -o liststartupdisks -d 'List all valid startup disks on this computer'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setstartupdisk -d 'Set the current startup disk to the indicated path'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getwaitforstartupafterpowerfailure -d 'Get seconds before computer starts up after a power failure'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setwaitforstartupafterpowerfailure -d 'Set seconds before startup after power failure (multiple of 30)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getdisablekeyboardwhenenclosurelockisengaged -d 'Get whether keyboard is disabled when enclosure lock is engaged'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setdisablekeyboardwhenenclosurelockisengaged -d 'Set whether keyboard is disabled when enclosure lock is engaged'
complete -c systemsetup -n __fish_systemsetup_no_verb -o getkernelbootarchitecturesetting -d 'Get the kernel boot architecture setting from com.apple.Boot.plist'
complete -c systemsetup -n __fish_systemsetup_no_verb -o setkernelbootarchitecture -d 'Set kernel boot architecture for next boot (i386, x86_64, or default)'
complete -c systemsetup -n __fish_systemsetup_no_verb -o version -d 'Display version of systemsetup tool'
complete -c systemsetup -n __fish_systemsetup_no_verb -o help -d 'Display all commands with explanatory information'
complete -c systemsetup -n __fish_systemsetup_no_verb -o printCommands -d 'Display a list of commands with no detail'

# ── verb arguments ────────────────────────────────────────────────────

# -setremotelogin accepts an optional -f flag before on|off
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) setremotelogin" \
    -a -f -d 'Suppress prompting when turning remote login off'

# on|off verbs
set -l onoff_verbs setusingnetworktime setwakeonmodem setwakeonnetworkaccess \
    setrestartpowerfailure setrestartfreeze setallowpowerbuttontosleepcomputer \
    setremotelogin setremoteappleevents
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) $onoff_verbs" \
    -a on -d Enable
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) $onoff_verbs" \
    -a off -d Disable

# yes|no verbs
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) setdisablekeyboardwhenenclosurelockisengaged" \
    -a yes -d Yes
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) setdisablekeyboardwhenenclosurelockisengaged" \
    -a no -d No

# Never/Off for sleep setters (minutes or Never/Off)
set -l sleep_verbs setsleep setcomputersleep setdisplaysleep setharddisksleep
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) $sleep_verbs" \
    -a Never -d 'Never sleep'
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) $sleep_verbs" \
    -a Off -d 'Never sleep'

# -settimezone — live enumeration via -listtimezones
complete -c systemsetup -x -n "contains -- (__fish_systemsetup_verb) settimezone" \
    -a '(__fish_systemsetup_timezones)'

# -setstartupdisk — live enumeration via -liststartupdisks
complete -c systemsetup -x -n "contains -- (__fish_systemsetup_verb) setstartupdisk" \
    -a '(__fish_systemsetup_startupdisks)'

# -setkernelbootarchitecture
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) setkernelbootarchitecture" \
    -a i386 -d '32-bit kernel'
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) setkernelbootarchitecture" \
    -a x86_64 -d '64-bit kernel'
complete -c systemsetup -f -n "contains -- (__fish_systemsetup_verb) setkernelbootarchitecture" \
    -a default -d 'Remove boot architecture override'
