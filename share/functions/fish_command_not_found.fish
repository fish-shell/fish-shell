### Command-not-found handlers
# This can be overridden by defining a new fish_command_not_found function

# Read the OS/Distro from /etc/os-release.
# This has a "ID=" line that defines the exact distribution,
# and an "ID_LIKE=" line that defines what it is derived from or otherwise like.
# For our purposes, we use both.
set -l os
if test -r /etc/os-release
    set os (string match -r '^ID(?:_LIKE)?\s*=.*' < /etc/os-release | \
    string replace -r '^ID(?:_LIKE)?\s*=(.*)' '$1' | string trim -c '\'"' | string split " ")
end

function __fish_default_command_not_found_handler
    printf (_ "fish: Unknown command: %s\n") (string escape -- $argv[1]) >&2
end

# If an old handler already exists, defer to that.
if functions -q __fish_command_not_found_handler
    function fish_command_not_found
        # The fish_command_not_found event was removed in fish 3.2.0,
        # and future versions of fish will just call a function called "fish_command_not_found".
        # You have defined a custom handler, we suggest renaming it to "fish_command_not_found".
        __fish_command_not_found_handler $argv
    end
    # First check if we are on OpenSUSE since SUSE's handler has no options
    # but the same name and path as Ubuntu's.
else if contains -- suse $os || contains -- sles $os && type -q command-not-found
    function fish_command_not_found
        command-not-found $argv[1]
    end
    # Check for Fedora's handler
else if test -f /usr/libexec/pk-command-not-found
    function fish_command_not_found
        /usr/libexec/pk-command-not-found $argv
    end
    # Check in /usr/lib, where Ubuntu places this command
else if test -f /usr/lib/command-not-found
    function fish_command_not_found
        /usr/lib/command-not-found -- $argv[1]
    end
    # Check for NixOS handler
else if test -f /run/current-system/sw/bin/command-not-found
    function fish_command_not_found
        /run/current-system/sw/bin/command-not-found $argv
    end
    # Ubuntu Feisty places this command in the regular path instead
else if type -q command-not-found
    function fish_command_not_found
        command-not-found -- $argv[1]
    end
    # pkgfile is an optional, but official, package on Arch Linux
    # it ships with example handlers for bash and zsh, so we'll follow that format
else if type -q pkgfile
    function fish_command_not_found
        set -l __packages (pkgfile --binaries --verbose -- $argv[1] 2>/dev/null)
        if test $status -eq 0
            printf "%s may be found in the following packages:\n" "$argv[1]"
            printf "  %s\n" $__packages
        else
            __fish_default_command_not_found_handler $argv[1]
        end
    end
    # pacman is too slow, see #7841.
    # else if type -q pacman
    #     function fish_command_not_found
    #         set -l paths $argv[1]
    #         # If we've not been given an absolute path, try $PATH as the starting point,
    #         # otherwise pacman will try *every path*, and e.g. bash-completion
    #         # isn't helpful.
    #         string match -q '/*' -- $argv[1]; or set paths $PATH/$argv[1]
    #         # Pacman only prints the path, so we still need to print the error.
    #         __fish_default_command_not_found_handler $argv[1]
    #         pacman -F $paths
    #     end
else
    # Use standard fish command not found handler otherwise
    function fish_command_not_found --on-event fish_command_not_found
        __fish_default_command_not_found_handler $argv
    end
end
