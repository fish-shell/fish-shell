function help --description 'Show help for the fish shell'
    set -l options h/help
    argparse -n help $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help help
        return 0
    end

    set -l fish_help_item $argv[1]
    if test (count $argv) -gt 1
        if string match -q string $argv[1]
            set fish_help_item (string join '-' $argv[1] $argv[2])
        else
            echo "help: Expected at most 1 args, got 2" >&2
            return 1
        end
    end

    # Find a suitable browser for viewing the help pages.
    # The first thing we try is $fish_help_browser.
    set -l fish_browser $fish_help_browser

    # A list of graphical browsers we know about.
    set -l graphical_browsers htmlview x-www-browser firefox galeon mozilla xdg-open
    set -a graphical_browsers konqueror epiphany opera netscape rekonq google-chrome chromium-browser

    # On mac we may have to write a temporary file that redirects to the desired
    # help page, since `open` will drop fragments from file URIs (issue #4480).
    set -l need_trampoline

    if not set -q fish_browser[1]
        if set -q BROWSER
            # User has manually set a preferred browser, so we respect that
            echo $BROWSER | read -at fish_browser
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
            set -f is_graphical 0
            if test -n "$DISPLAY" -a \( "$XAUTHORITY" = "$HOME/.Xauthority" -o -z "$XAUTHORITY" \)
                set is_graphical 1
                for i in $graphical_browsers
                    if type -q -f $i
                        set fish_browser $i
                        break
                    end
                end
            end

            # If we're SSH'd into a desktop installation, don't use a regular browser unless X is being forwarded
            if not set -q SSH_CLIENT || test $is_graphical -eq 1
                # We use the macOS open, but not otherwise.
                # On Debian, there is an open command that's a symlink to openvt.
                if uname | string match -q Darwin && command -sq open
                    set fish_browser open
                    # The open command needs a trampoline because the macOS version can't handle #-fragments.
                    set need_trampoline 1
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
            set need_trampoline 1
        end
    end

    # HACK: Hardcode all section titles for each page.
    # This could possibly be automated.
    set -l intropages introduction where-to-go installation starting-and-exiting default-shell uninstalling shebang-line configuration examples resources other-help-pages
    set -l for_bash_pages arithmetic-expansion bash-command-substitutions blocks-and-loops builtins-and-other-commands command-substitutions fish-for-bash-users heredocs process-substitution prompts quoting special-variables string-manipulation subshells test-test variables wildcards-globs
    set -l faqpages faq-ssh-interactive faq-unicode faq-uninstalling frequently-asked-questions how-can-i-use-as-a-shortcut-for-cd how-do-i-change-the-greeting-message how-do-i-check-whether-a-variable-is-defined how-do-i-check-whether-a-variable-is-not-empty how-do-i-customize-my-syntax-highlighting-colors how-do-i-get-the-exit-status-of-a-command how-do-i-make-fish-my-default-shell how-do-i-run-a-command-every-login-what-s-fish-s-equivalent-to-bashrc-or-profile how-do-i-run-a-command-from-history how-do-i-run-a-subcommand-the-backtick-doesn-t-work how-do-i-set-my-prompt how-do-i-set-or-clear-an-environment-variable i-accidentally-entered-a-directory-path-and-fish-changed-directory-what-happened i-m-getting-weird-graphical-glitches-a-staircase-effect-ghost-characters-cursor-in-the-wrong-position i-m-seeing-weird-output-before-each-prompt-when-using-screen-what-s-wrong my-command-pkg-config-gives-its-output-as-a-single-long-string my-command-prints-no-matches-for-wildcard-but-works-in-bash the-open-command-doesn-t-work uninstalling-fish what-is-the-equivalent-to-this-thing-from-bash-or-other-shells where-can-i-find-extra-tools-for-fish why-does-my-prompt-show-a-i why-doesn-t-history-substitution-etc-work why-doesn-t-set-ux-exported-universal-variables-seem-to-work why-won-t-ssh-scp-rsync-connect-properly-when-fish-is-my-login-shell
    set -l interactivepages abbreviations autosuggestions color command-line-editor command-mode configurable-greeting copy-and-paste-kill-ring custom-bindings custom-binds directory-stack editor emacs-mode emacs-mode-commands greeting help history-search id7 insert-mode interactive interactive-use killring multiline multiline-editing navigating-directories pager-color-variables private-mode programmable-prompt programmable-title prompt searchable-command-history shared-bindings shared-binds syntax-highlighting syntax-highlighting-variables tab-completion title variables-color variables-color-pager vi-mode vi-mode-command vi-mode-commands vi-mode-insert vi-mode-visual visual-mode
    set -l langpages argument-handling autoloading-functions brace-expansion builtin-commands builtin-overview cartesian-product combine combining-different-expansions combining-lists-cartesian-product command-substitution comments conditions debugging debugging-fish-scripts defining-aliases escapes escaping-characters event event-handlers expand expand-brace expand-command-substitution expand-home expand-index-range expand-variable expand-wildcard exporting-variables featureflags functions future-feature-flags home-directory-expansion identifiers index-range-expansion input-output-redirection job-control language lists locale-variables loops-and-blocks more-on-universal-variables overriding-variables-for-a-single-command parameter-expansion path-variables pipes piping quotes redirects shell-variable-and-function-names shell-variables special-variables syntax syntax-conditional syntax-function syntax-function-autoloading syntax-function-wrappers syntax-job-control syntax-loops-and-blocks syntax-overview terminology the-fish-language the-status-variable variable-expansion variables variables-argv variable-scope variable-scope-for-functions variables-export variables-functions variables-lists variables-locale variables-override variables-path variables-scope variables-special variables-status variables-universal wildcards-globbing configuration
    set -l tutpages autoloading-functions autosuggestions combiners-and-or-not command-substitutions conditionals-if-else-switch exit-status exports-shell-variables functions getting-help getting-started learning-fish lists loops pipes-and-redirections prompt ready-for-more running-commands separating-commands-semicolon startup-where-s-bashrc switching-to-fish syntax-highlighting tab-completions tut-combiners tut-conditionals tut-config tut-exports tut-lists tutorial tut-semicolon tut-universal universal-variables variables why-fish wildcards

    set -l fish_help_page
    switch "$fish_help_item"
        case "."
            set fish_help_page "cmds/source.html"
        case ":"
            set fish_help_page "cmds/true.html"
        case "["
            set fish_help_page "cmds/test.html"
        case globbing
            set fish_help_page "language.html#expand"
        case 'completion-*'
            set fish_help_page "completions.html#$fish_help_item"
        case 'tut-*'
            set fish_help_page "tutorial.html#"(string sub -s 5 -- $fish_help_item | string replace -a -- _ -)
        case tutorial
            set fish_help_page "tutorial.html"
        case releasenotes
            set fish_help_page relnotes.html
        case completions
            set fish_help_page completions.html
        case commands
            set fish_help_page commands.html
        case faq
            set fish_help_page faq.html
        case fish-for-bash-users
            set fish_help_page fish_for_bash_users.html
        case custom-prompt
            set fish_help_page prompt.html
        case $faqpages
            set fish_help_page "faq.html#$fish_help_item"
        case $for_bash_pages
            set fish_help_page "fish_for_bash_users.html#$fish_help_item"
        case $langpages
            set fish_help_page "language.html#$fish_help_item"
        case $interactivepages
            set fish_help_page "interactive.html#$fish_help_item"
        case $tutpages
            set fish_help_page "tutorial.html#$fish_help_item"
        case (builtin -n) (__fish_print_commands)
            # If the docs aren't installed, __fish_print_commands won't print anything
            # Since we document all our builtins, check those at least.
            # The alternative is to create this list at build time.
            set fish_help_page "cmds/$fish_help_item.html"
        case ''
            set fish_help_page "index.html"
        case $intropages
            set fish_help_page "index.html$fish_help_item"
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
    set -l version_string (string split . -f 1,2 -- $version | string join .)
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
        set need_trampoline
    end

    if not set -q fish_browser[1]
        printf (_ '%s: Could not find a web browser.\n') help >&2
        printf (_ 'Please try `BROWSER=some_browser help`, `man fish-doc`, or `man fish-tutorial`.\n\n') >&2
        printf (_ 'Or open %s in your browser of choice.\n') $ext_url >&2
        return 1
    end

    if set -q need_trampoline[1]
        # If string replace doesn't replace anything, we don't actually need a
        # trampoline (they're only needed if there's a fragment in the path)
        if set -l clean_url (string match -re '#' $page_url)
            # Write a temporary file that will redirect where we want.
            set -q TMPDIR
            or set -l TMPDIR /tmp
            set -l tmpdir (mktemp -d $TMPDIR/help.XXXXXX)
            or return 1
            set -l tmpname $tmpdir/help.html
            echo '<meta http-equiv="refresh" content="0;URL=\''$clean_url'\'" />' >$tmpname
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
    end

    printf (_ 'help: Help is being displayed in %s.\n') $fish_browser[1]
    # cmd.exe and powershell needs more coaxing.
    if string match -qr 'powershell\.exe$|cmd\.exe$' -- $fish_browser[1]
        # The space before the /c is to prevent msys2 from expanding it to a path
        $fish_browser " /c" start $page_url
    else if contains -- $fish_browser[1] $graphical_browsers
        /bin/sh -c '( "$@" ) &' -- $fish_browser $page_url
    else
        $fish_browser $page_url
    end

    # Show the online URL anyway in case we did not manage to open something successfully.
    # (we can't check because we need to background it)
    printf (_ 'help: If no help could be displayed, go to %s to view the documentation online.\n') $ext_url
end
