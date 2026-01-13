# localization: tier1
function help --description 'Show help for the fish shell'
    set -l options h/help
    argparse -n help $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help help
        return
    end

    set -l fish_help_item $argv[1]
    if test (count $argv) -gt 1
        if test $argv[1] = string
            set fish_help_item (string join -- '-' $argv[1] $argv[2])
        else
            echo "help: Expected at most 1 args, got 2" >&2
            return 1
        end
    end

    # Find a suitable browser for viewing the help pages.
    # The first thing we try is $fish_help_browser.
    set -l fish_browser $fish_help_browser

    # A list of graphical browsers we know about.
    set -l graphical_browsers (printf %s "\
htmlview
x-www-browser
firefox
galeon
mozilla
xdg-open
konqueror
epiphany
opera
netscape
rekonq
google-chrome
chromium-browser
")

    # On mac we may have to write a temporary file that redirects to the desired
    # help page, since `open` will drop fragments from file URIs (issue #4480).
    set -l need_trampoline false

    if not set -q fish_browser[1]
        if set -q BROWSER
            # User has manually set a preferred browser, so we respect that
            echo $BROWSER | read -at fish_browser
            if not type -q $fish_browser[1]
                printf (_ 'help: %s is not a valid command: %s\n') '$fish_browser' "$fish_browser"
                return 2
            end
        else
            # No browser set up, inferring.
            # We check a bunch and use the last we find.

            # Check for a text-based browser.
            for i in htmlview www-browser links elinks lynx w3m
                if type -q -f $i
                    set fish_browser $i
                    break
                end
            end

            # If we are in a graphical environment, check if there is a graphical
            # browser to use instead.
            set -l is_graphical false
            if test -n "$DISPLAY" &&
                    { test -z "$XAUTHORITY" ||
                            test "$XAUTHORITY" = "$HOME/.Xauthority"
                    }
                set is_graphical true
                for i in $graphical_browsers
                    if type -q -f $i
                        set fish_browser $i
                        break
                    end
                end
            end

            # If we're SSH'd into a desktop installation, don't use a regular browser unless X is being forwarded
            if not set -q SSH_CLIENT || $is_graphical
                # We use the macOS open, but not otherwise.
                # On Debian, there is an open command that's a symlink to openvt.
                if uname | string match -q Darwin && command -sq open
                    set fish_browser open
                    # The open command needs a trampoline because the macOS version can't handle #-fragments.
                    set need_trampoline true
                end

                # If the OS appears to be Windows (graphical), try to use cygstart
                if type -q cygstart
                    set fish_browser cygstart
                else if type -q xdg-open
                    # If xdg-open is available, just use that
                    set fish_browser xdg-open
                end

                # Try to find cmd.exe via $PATH or one of the paths that it's often at.
                #
                # We use this instead of xdg-open because that's useless without a backend
                # like wsl-open which we'll check in a minute.
                if test -f /proc/version
                    and string match -riq 'Microsoft|WSL|MSYS|MINGW' </proc/version
                    and set -l cmd (command -s powershell.exe cmd.exe /mnt/c/Windows/System32/cmd.exe)
                    # Use the first of these.
                    set fish_browser $cmd[1]
                end

                if type -q wsl-open
                    set fish_browser wsl-open
                end
            end
        end
    else
        if not type -q $fish_browser[1]
            printf (_ 'help: %s is not a valid command: %s\n') '$fish_help_browser' "$fish_browser"
            return 2
        end
    end

    # In Cygwin, start the user-specified browser using cygstart,
    # only if a Windows browser is to be used.
    if type -q cygstart
        if test "$fish_browser" != cygstart
            and set -q fish_browser[1]
            and not command -sq $fish_browser[1]
            # Escaped quotes are necessary to work with spaces in the path
            # when the command is finally eval'd.
            set fish_browser cygstart $fish_browser
        else
            set need_trampoline true
        end
    end

    set fish_help_item (string replace -r -- '\b#$' '' $fish_help_item)
    set -l fish_help_page
    switch "$fish_help_item"
        case ''
            set fish_help_page index.html
        case (status get-file help_sections | string replace -r "^index(#|\$)" introduction\$1)
            set fish_help_page (
                printf %s $fish_help_item |
                    string replace -r '^introduction(#|$)' 'index$1' |
                    string replace -r -- '(#|$)' '.html$1'
            )

        case {cmds/,}"!"
            set fish_help_page cmds/not.html
        case {cmds/,}"."
            set fish_help_page cmds/source.html
        case {cmds/,}":"
            set fish_help_page cmds/true.html
        case {cmds/,}"["
            set fish_help_page cmds/test.html
        case {cmds/,}"{"
            set fish_help_page cmds/begin.html
        case (__fish_print_commands)
            set fish_help_page cmds/$fish_help_item.html

        case "*"
            printf (_ "%s: no fish help topic '%s', try 'man %s'\n") help $fish_help_item $fish_help_item
            return 1
    end

    # In Crostini Chrome OS Linux, the default browser opens URLs in Chrome running outside the
    # linux VM. This browser does not have access to the Linux filesystem. This uses Garcon, see e.g.
    # https://chromium.googlesource.com/chromiumos/platform2/+/master/vm_tools/garcon/#opening-urls
    # https://source.chromium.org/search?q=garcon-url-handler
    string match -q '*garcon-url-handler*' $fish_browser[1]
    and set -l chromeos_linux_garcon

    # Generate the online URL, with one dot in the version string (major.minor)
    set -l version_string (string match -rg '(\d+\.\d+(?:b\d+)?).*' -- $version)
    set -l ext_url https://fishshell.com/docs/$version_string/$fish_help_page
    set -l page_url
    if set -q __fish_help_dir[1]; and test -f $__fish_help_dir/index.html; and not set -lq chromeos_linux_garcon
        # Help is installed, use it
        set page_url file://$__fish_help_dir/$fish_help_page

        # For Windows (Cygwin, msys2 and WSL), we need to convert the base
        # help dir to a Windows path before converting it to a file URL
        # but only if a Windows browser is being used
        if type -q cygpath
            and string match -qr '(cygstart|\.exe)(\s+|$)' $fish_browser[1]
            set page_url file://(cygpath -m $__fish_help_dir)/$fish_help_page
        else if type -q wslpath
            and string match -qr '\.exe(\s+|$)' $fish_browser[1]
            set page_url file://(wslpath -w $__fish_help_dir)/$fish_help_page
        end
    else
        set page_url $ext_url
        # We don't need a trampoline for a remote URL.
        set need_trampoline false
    end

    if not set -q fish_browser[1]
        printf (_ '%s: Could not find a web browser.\n') help >&2
        printf (_ 'Please try `BROWSER=some_browser help`, `man fish-doc`, or `man fish-tutorial`.\n\n') >&2
        printf (_ 'Or open %s in your browser of choice.\n') $ext_url >&2
        return 1
    end

    if $need_trampoline && string match -rq '#' $page_url
        # Write a temporary file that will redirect where we want.
        set -l tmpdir (__fish_mktemp_relative -d fish-help)
        or return 1
        set -l tmpname $tmpdir/help.html
        echo '<meta http-equiv="refresh" content="0;URL=\''$page_url'\'" />' >$tmpname
        set page_url file://$tmpname

        # For Windows (Cygwin, msys2 and WSL), we need to convert the base help dir to a Windows path
        # before converting it to a file URL, but only if a Windows browser is being used
        if type -q cygpath
            and string match -qr '(cygstart|\.exe)(\s+|$)' $fish_browser[1]
            set page_url file://(cygpath -m $tmpname)
        else if type -q wslpath
            and string match -qr '\.exe(\s+|$)' $fish_browser[1]
            set page_url file://(wslpath -w $tmpname)
        end
    end

    printf (_ 'help: Help is being displayed in %s.\n') $fish_browser[1]
    # cmd.exe and powershell needs more coaxing.
    if string match -qr 'powershell\.exe$|cmd\.exe$' -- $fish_browser[1]
        # The space before the /c is to prevent msys2 from expanding it to a path
        $fish_browser " /c" start $page_url
    else if contains -- $fish_browser[1] $graphical_browsers
        set -l sh (__fish_posix_shell)
        $sh -c '( "$@" ) &' -- $fish_browser $page_url
    else
        $fish_browser $page_url
    end

    # Show the online URL anyway in case we did not manage to open something successfully.
    # (we can't check because we need to background it)
    printf (_ 'help: If no help could be displayed, go to %s to view the documentation online.\n') $ext_url
end
