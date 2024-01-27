function __fish_systemctl --description 'Call systemctl with some options from the current commandline'
    # These options are all global - before or after subcommand/arguments.
    # There's a bunch of long-only options in here, so we need to be creative with the mandatory short version.
    set -l opts t/type= s-state= p/property= a/all
    set opts $opts r/recursive R-reverse A-after B-before
    set opts $opts l/full V-value S-show-types J-job-mode=
    set opts $opts F-fail i/ignore-inhibitors q/quiet N-no-block
    set opts $opts W-wait U-user Y-system I-failed O-no-wall
    set opts $opts G-global E-reload K-no-ask-password 4-kill-who
    set opts $opts ü-signal f-force 5-message ö-now ä-root
    set opts $opts Ü-runtime Ö-preset-mode= n/lines= o/output=
    set opts $opts Ä-firmware-setup ß-plain H/host= M/machine=
    set opts $opts 1-no-pager 2-no-legend h/help 3-version

    set -l args $argv
    set -l cmdline (commandline -xpc) (commandline -ct)
    set -e cmdline[1]
    argparse $opts -- $cmdline 2>/dev/null
    or return

    # If no subcommand has been given, return so this can be used as a condition.
    test -n "$argv[1]"
    or return
    set -l cmd $argv[1]
    set -e argv[1]

    # Flags we want to pass on.
    set -l passflags $_flag_user $_flag_system $_flag_failed
    switch "$cmd"
        # These are the normal commands, so just complete all units.
        # For "restart" et al, also complete non-running ones, since it can be used regardless of state.
        case reenable status reload {try-,}{reload-or-,}restart is-{active,enabled,failed} show cat \
            help reset-failed list-dependencies list-units revert add-{wants,requires} edit clean thaw
        case enable
            # This will only work for "list-unit-files", but won't print an error for "list-units".
            set -q _flag_state; or set _flag_state disabled
        case disable
            set -q _flag_state; or set _flag_state enabled
        case start
            # Running `start` on an already started unit isn't an _error_, but useless.
            set -q _flag_state; or set _flag_state dead,failed
        case mask
            set -q _flag_state; or set _flag_state loaded
        case unmask
            set -q _flag_state; or set _flag_state masked
        case stop kill freeze
            # TODO: Is "kill" useful on other unit types?
            # Running as the catch-all, "mounted" for .mount units, "active" for .target.
            set -q _flag_state; or set _flag_state running,mounted,active
        case isolate set-default
            # These only take one unit.
            set -q argv[1]; and return
        case list-sockets
            set _flag_type socket
        case list-timers
            set _flag_type timer
        case get-default show-environment daemon-{reload,reexec} is-system-running default rescue emergency halt poweroff kexec \
            suspend hibernate hybrid-sleep
            # Accept no arguments.
            return
        case '*'
            # Unknown subcommand. Since we don't want to execute just anything, return.
            # Note that this could also be a partial token, which is completed elsewhere.
            return
    end

    # Add the flags back so we can pass them to our systemctl invocations.
    set -q _flag_type; and set passflags $passflags --type=$_flag_type
    set -q _flag_state; and set passflags $passflags --state=$_flag_state
    set -q _flag_property; and set passflags $passflags --property=$_flag_property
    set -q _flag_machine; and set passflags $passflags --machine=$_flag_machine
    set -q _flag_host; and set passflags $passflags --host=$_flag_host

    # Output looks like
    # systemd-tmpfiles-clean.timer      [more whitespace] loaded    active     waiting   Daily Cleanup[...]
    # Use the last part as the description.
    systemctl --full --no-legend --no-pager --plain --all list-units $passflags | string replace -r "(?: +(\S+)){4}" \t'$1'
    # We need this for disabled/static units. Also instance units without an active instance.
    # Output looks like
    # systemd-tmpfiles-clean.timer               static
    # Just use the state as the description, since we won't get it here.
    # This is an issue for units that appear in both.
    systemctl --full --no-legend --no-pager --plain --all list-unit-files $passflags | string replace -r "(?: +(\S+)){1}" \t'$1'
end
