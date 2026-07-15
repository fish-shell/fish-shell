# pmset - manipulate power management settings (man 1 pmset)
#
# pmset has a mixed grammar:
#   - Selector flags (-a/-b/-c/-u/-g) appear first, optionally followed by
#     a subword (for -g) or setting key/value pairs.
#   - Action words (schedule, repeat, relative, touch, sleepnow, etc.) stand
#     alone as the first positional token.

# ── command-line introspection ────────────────────────────────────────────────

# Return the first bare-word (non-flag) token after "pmset", if any.
function __fish_pmset_action
    set -l toks (commandline -opc)
    set -e toks[1] # drop "pmset" itself
    for t in $toks
        if not string match -q -- '-*' $t
            echo $t
            return 0
        end
    end
    return 1
end

# True when no action word and no selector flag is present yet.
function __fish_pmset_no_verb
    set -l toks (commandline -opc)
    set -e toks[1]
    for t in $toks
        # Any recognised flag or action word counts as "verb seen"
        if string match -qr -- '^-[abcug]$' $t
            return 1
        end
        if contains -- $t schedule repeat relative touch sleepnow \
                displaysleepnow boot restoredefaults resetdisplayambientparams noidle
            return 1
        end
    end
    return 0
end

# True when -g is on the command line (get mode).
function __fish_pmset_has_g
    contains -- -g (commandline -opc)
end

# True when a specific action word is present.
function __fish_pmset_action_is
    set -l want $argv
    set -l act (__fish_pmset_action 2>/dev/null)
    contains -- $act $want
end

# True when the commandline already has a -g sub-argument (live/custom/cap/…).
function __fish_pmset_g_has_subarg
    set -l toks (commandline -opc)
    set -l saw_g 0
    for t in $toks
        if test $saw_g -eq 1
            # The token after -g — if it exists and is not a flag, it's the sub-arg
            if not string match -q -- '-*' $t
                return 0
            end
        end
        if test "$t" = -g
            set saw_g 1
        end
    end
    return 1
end

# True when "schedule" is the action and we have fewer than 2 positional
# tokens after it (i.e. the event type has not yet been given, or it has
# been given but the date hasn't — we can only complete the type).
function __fish_pmset_schedule_needs_type
    __fish_pmset_action_is schedule
    and not __fish_pmset_schedule_has_type
end

function __fish_pmset_schedule_has_type
    set -l toks (commandline -opc)
    set -l saw_sched 0
    for t in $toks
        if test $saw_sched -eq 1
            if contains -- $t sleep wake poweron shutdown wakeorpoweron cancel cancelall
                return 0
            end
        end
        if test "$t" = schedule
            set saw_sched 1
        end
    end
    return 1
end

# True when "repeat" is the action and we need a type token.
function __fish_pmset_repeat_needs_type
    __fish_pmset_action_is repeat
    and not __fish_pmset_repeat_has_type
end

function __fish_pmset_repeat_has_type
    set -l toks (commandline -opc)
    set -l saw 0
    for t in $toks
        if test $saw -eq 1
            if contains -- $t sleep wake poweron shutdown wakeorpoweron cancel
                return 0
            end
        end
        if test "$t" = repeat
            set saw 1
        end
    end
    return 1
end

# ── selector flags (no action word present) ───────────────────────────────────
complete -c pmset -n __fish_pmset_no_verb -s a -d 'Apply setting to all power sources'
complete -c pmset -n __fish_pmset_no_verb -s b -d 'Apply setting to battery power source'
complete -c pmset -n __fish_pmset_no_verb -s c -d 'Apply setting to charger (AC) power source'
complete -c pmset -n __fish_pmset_no_verb -s u -d 'Apply setting to UPS power source'
complete -c pmset -n __fish_pmset_no_verb -s g -d 'Get (display) current power management settings'

# ── action words (first positional token) ─────────────────────────────────────
complete -c pmset -n __fish_pmset_no_verb -f -a schedule -d 'Schedule a one-time sleep/wake/poweron/shutdown event'
complete -c pmset -n __fish_pmset_no_verb -f -a repeat -d 'Schedule a repeating power on/off event'
complete -c pmset -n __fish_pmset_no_verb -f -a relative -d 'Schedule a relative wake or poweron in seconds from sleep end'
complete -c pmset -n __fish_pmset_no_verb -f -a touch -d 'Re-read existing power management settings from disk'
complete -c pmset -n __fish_pmset_no_verb -f -a sleepnow -d 'Cause an immediate system sleep'
complete -c pmset -n __fish_pmset_no_verb -f -a displaysleepnow -d 'Cause the display to sleep immediately'
complete -c pmset -n __fish_pmset_no_verb -f -a boot -d 'Tell the kernel that system boot is complete'
complete -c pmset -n __fish_pmset_no_verb -f -a restoredefaults -d 'Restore power management settings to default values'
complete -c pmset -n __fish_pmset_no_verb -f -a resetdisplayambientparams -d 'Reset ambient light parameters for certain Apple displays'
complete -c pmset -n __fish_pmset_no_verb -f -a noidle -d 'Prevent idle sleep (deprecated — use caffeinate instead)'

# ── -g sub-arguments ──────────────────────────────────────────────────────────
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a live -d 'Display settings currently in use'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a custom -d 'Display custom settings for all power sources'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a cap -d 'Display supported power management features'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a sched -d 'Display scheduled startup/wake and shutdown/sleep events'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a ups -d 'Display UPS emergency thresholds'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a ps -d 'Display status of batteries and UPSs'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a batt -d 'Display battery and UPS status (alias for ps)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a pslog -d 'Display ongoing log of power source state'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a rawlog -d 'Display ongoing log of battery state read directly from battery'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a therm -d 'Show thermal conditions affecting CPU speed'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a thermlog -d 'Show log of thermal notifications affecting CPU speed'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a assertions -d 'Display summary of power assertions (10.6+)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a assertionslog -d 'Show log of assertion creations and releases (10.6+)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a sysload -d 'Display system load advisory summary (10.6+)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a sysloadlog -d 'Display ongoing log of system load advisory changes (10.6+)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a ac -d 'Display details about attached AC power adapter (MacBook/Pro)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a adapter -d 'Display details about attached AC power adapter (alias for ac)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a log -d 'Display history of sleeps, wakes, and power management events'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a uuid -d 'Display currently active sleep/wake UUID'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a uuidlog -d 'Display active sleep/wake UUID and print new UUIDs as set'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a history -d 'Print timeline of system sleep/wake UUIDs (debug; needs boot-arg)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a historydetailed -d 'Print driver-level timings for a sleep/wake (pass a UUID)'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a powerstate -d 'Print current power states for I/O Kit drivers'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a powerstatelog -d 'Periodically print power state residency times for drivers'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a stats -d 'Print counts of sleeps and wakes since boot'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a systemstate -d 'Print current power state of system and available capabilities'
complete -c pmset -n '__fish_pmset_has_g; and not __fish_pmset_g_has_subarg' -f -a everything -d 'Print output from every -g argument (10.8+)'

# ── setting keywords (used after -a/-b/-c/-u, not after -g or actions) ────────
# Condition: a selector (-a/-b/-c/-u) is present, -g is NOT present, and no
# action word is present.
function __fish_pmset_setting_mode
    set -l toks (commandline -opc)
    set -l has_sel 0
    for t in $toks
        if string match -qr -- '^-[abcu]$' $t
            set has_sel 1
        end
        if test "$t" = -g
            return 1
        end
        if contains -- $t schedule repeat relative touch sleepnow \
                displaysleepnow boot restoredefaults resetdisplayambientparams noidle
            return 1
        end
    end
    test $has_sel -eq 1
end

complete -c pmset -n __fish_pmset_setting_mode -f -a displaysleep -d 'Display sleep timer in minutes (0=never)'
complete -c pmset -n __fish_pmset_setting_mode -f -a disksleep -d 'Disk spindown timer in minutes (0=never)'
complete -c pmset -n __fish_pmset_setting_mode -f -a sleep -d 'System sleep timer in minutes (0=never)'
complete -c pmset -n __fish_pmset_setting_mode -f -a womp -d 'Wake on ethernet magic packet (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a ring -d 'Wake on modem ring (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a autorestart -d 'Automatic restart on power loss (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a autorestartatconnect -d 'Automatic restart on power connect (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a powernap -d 'Enable Power Nap on supported machines (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a proximitywake -d 'Wake based on proximity of nearby iCloud devices (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a tcpkeepalive -d 'Enable TCP keep-alive during sleep (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a lidwake -d 'Wake when laptop lid is opened (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a acwake -d 'Wake when power source changes (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a lessbright -d 'Slightly dim display when switching to this source (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a halfdim -d 'Use half-brightness state before display sleep (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a sms -d 'Use Sudden Motion Sensor to park disk heads (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a ttyskeepawake -d 'Prevent idle sleep when a tty session is active (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a destroyfvkeyonstandby -d 'Destroy FileVault key on standby (0=retain, 1=destroy)'
complete -c pmset -n __fish_pmset_setting_mode -f -a gpuswitch -d 'GPU switching mode (0=integrated, 1=discrete, 2=dynamic)'
complete -c pmset -n __fish_pmset_setting_mode -f -a hibernatemode -d 'Hibernation mode (0=no image, 3=safe sleep, 25=full hibernate)'
complete -c pmset -n __fish_pmset_setting_mode -a hibernatefile -d 'Path for hibernation image (root volume only)'
complete -c pmset -n __fish_pmset_setting_mode -f -a standby -d 'Auto-hibernate after sleep for standbydelay seconds (0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a standbydelay -d 'Seconds before writing hibernate image and powering off memory (legacy)'
complete -c pmset -n __fish_pmset_setting_mode -f -a standbydelayhigh -d 'Standby delay when battery above highstandbythreshold (seconds)'
complete -c pmset -n __fish_pmset_setting_mode -f -a standbydelaylow -d 'Standby delay when battery below highstandbythreshold (seconds)'
complete -c pmset -n __fish_pmset_setting_mode -f -a highstandbythreshold -d 'Battery % threshold selecting high vs low standby delay (default 50)'
complete -c pmset -n __fish_pmset_setting_mode -f -a autopoweroff -d 'Auto-hibernate into lower-power chipset sleep (Lot 6; 0/1)'
complete -c pmset -n __fish_pmset_setting_mode -f -a autopoweroffdelay -d 'Seconds before entering autopoweroff mode'

# ── UPS-specific settings (only meaningful after -u) ─────────────────────────
function __fish_pmset_ups_mode
    contains -- -u (commandline -opc)
    and not __fish_pmset_has_g
end

complete -c pmset -n __fish_pmset_ups_mode -f -a haltlevel -d 'UPS battery % at which to trigger emergency shutdown'
complete -c pmset -n __fish_pmset_ups_mode -f -a haltafter -d 'Minutes on UPS power after which to trigger emergency shutdown'
complete -c pmset -n __fish_pmset_ups_mode -f -a haltremain -d 'Remaining UPS minutes at which to trigger emergency shutdown'

# ── schedule sub-arguments ────────────────────────────────────────────────────
# "schedule [cancel|cancelall] type date+time [owner]"
# Complete cancel/cancelall OR event types after "schedule"
complete -c pmset -n '__fish_pmset_action_is schedule; and not __fish_pmset_schedule_has_type' \
    -f -a cancel -d 'Cancel a scheduled power event (requires type and date)'
complete -c pmset -n '__fish_pmset_action_is schedule; and not __fish_pmset_schedule_has_type' \
    -f -a cancelall -d 'Cancel all scheduled power events'
complete -c pmset -n '__fish_pmset_action_is schedule; and not __fish_pmset_schedule_has_type' \
    -f -a sleep -d 'Schedule a one-time sleep event'
complete -c pmset -n '__fish_pmset_action_is schedule; and not __fish_pmset_schedule_has_type' \
    -f -a wake -d 'Schedule a one-time wake event'
complete -c pmset -n '__fish_pmset_action_is schedule; and not __fish_pmset_schedule_has_type' \
    -f -a poweron -d 'Schedule a one-time power-on event'
complete -c pmset -n '__fish_pmset_action_is schedule; and not __fish_pmset_schedule_has_type' \
    -f -a shutdown -d 'Schedule a one-time shutdown event'
complete -c pmset -n '__fish_pmset_action_is schedule; and not __fish_pmset_schedule_has_type' \
    -f -a wakeorpoweron -d 'Schedule a one-time wake-or-power-on event'

# ── repeat sub-arguments ──────────────────────────────────────────────────────
# "repeat cancel" or "repeat type weekdays time [type weekdays time]"
complete -c pmset -n '__fish_pmset_action_is repeat; and not __fish_pmset_repeat_has_type' \
    -f -a cancel -d 'Cancel all repeating power events'
complete -c pmset -n '__fish_pmset_action_is repeat; and not __fish_pmset_repeat_has_type' \
    -f -a sleep -d 'Repeat: sleep on given weekdays at given time'
complete -c pmset -n '__fish_pmset_action_is repeat; and not __fish_pmset_repeat_has_type' \
    -f -a wake -d 'Repeat: wake on given weekdays at given time'
complete -c pmset -n '__fish_pmset_action_is repeat; and not __fish_pmset_repeat_has_type' \
    -f -a poweron -d 'Repeat: power on on given weekdays at given time'
complete -c pmset -n '__fish_pmset_action_is repeat; and not __fish_pmset_repeat_has_type' \
    -f -a shutdown -d 'Repeat: shutdown on given weekdays at given time'
complete -c pmset -n '__fish_pmset_action_is repeat; and not __fish_pmset_repeat_has_type' \
    -f -a wakeorpoweron -d 'Repeat: wake or power on on given weekdays at given time'

# ── relative sub-arguments ────────────────────────────────────────────────────
# "relative [wake | poweron] seconds"
complete -c pmset -n '__fish_pmset_action_is relative' -f -a wake -d 'Wake N seconds after sleep ends'
complete -c pmset -n '__fish_pmset_action_is relative' -f -a poweron -d 'Power on N seconds after shutdown ends'
