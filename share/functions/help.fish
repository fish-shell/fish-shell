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
            set need_trampoline 1
        end
    end

    if set -q __fish_help_dir[1]; and path is -f $__fish_help_dir/index.html
        # If Help pages are installed, get section titles from them.
        function __get_sections
            string replace -a -r '"#|" title="Link to this heading"' '' (string match -a -r '"[^"]+" title="Link to this heading"' < $__fish_help_dir/$argv.html)
        end
        set -f intropages (__get_sections index)
        set -f for_bash_pages (__get_sections fish_for_bash_users)
        set -f faqpages (__get_sections faq)
        set -f interactivepages (__get_sections interactive)
        set -f langpages (__get_sections language)
        set -f tutpages (__get_sections tutorial)
        functions -e __get_sections
    else
        # HACK: If Help pages are not installed, fall back to hardcoding section titles for each page.
        set -f intropages introduction where-to-go setup installation starting-and-exiting default-shell uninstalling shebang-line configuration examples resources other-help-pages
        set -f for_bash_pages fish-for-bash-users command-substitutions variables wildcards-globs quoting string-manipulation special-variables process-substitution heredocs test-test arithmetic-expansion prompts blocks-and-loops subshells builtins-and-other-commands other-facilities
        set -f faqpages frequently-asked-questions what-is-the-equivalent-to-this-thing-from-bash-or-other-shells how-do-i-set-or-clear-an-environment-variable how-do-i-check-whether-a-variable-is-defined how-do-i-check-whether-a-variable-is-not-empty why-doesn-t-set-ux-exported-universal-variables-seem-to-work how-do-i-run-a-command-every-login-what-s-fish-s-equivalent-to-bashrc-or-profile how-do-i-set-my-prompt why-does-my-prompt-show-a-i how-do-i-customize-my-syntax-highlighting-colors how-do-i-change-the-greeting-message how-do-i-run-a-command-from-history why-doesn-t-history-substitution-etc-work how-do-i-run-a-subcommand-the-backtick-doesn-t-work my-command-pkg-config-gives-its-output-as-a-single-long-string how-do-i-get-the-exit-status-of-a-command my-command-prints-no-matches-for-wildcard-but-works-in-bash why-won-t-ssh-scp-rsync-connect-properly-when-fish-is-my-login-shell i-m-getting-weird-graphical-glitches-a-staircase-effect-ghost-characters-cursor-in-the-wrong-position uninstalling-fish
        set -f interactivepages interactive-use help autosuggestions tab-completion syntax-highlighting syntax-highlighting-variables pager-color-variables abbreviations programmable-prompt configurable-greeting programmable-title command-line-editor shared-bindings emacs-mode-commands vi-mode-commands command-mode insert-mode visual-mode custom-bindings key-sequences copy-and-paste-kill-ring multiline-editing searchable-command-history private-mode navigating-directories directory-history directory-stack
        set -f langpages the-fish-language syntax-overview terminology quotes escaping-characters input-output-redirection piping combining-pipes-and-redirections job-control functions defining-aliases autoloading-functions comments conditions the-if-statement the-switch-statement combiners-and-or loops-and-blocks parameter-expansion wildcards-globbing variable-expansion quoting-variables dereferencing-variables variables-as-command command-substitution brace-expansion combining-lists slices home-directory-expansion combining-different-expansions table-of-operators shell-variables variable-scope overriding-variables-for-a-single-command universal-variables exporting-variables lists argument-handling path-variables special-variables the-status-variable locale-variables builtin-commands command-lookup querying-for-user-input shell-variable-and-function-names configuration-files future-feature-flags event-handlers debugging-fish-scripts profiling-fish-scripts
        set -f tutpages tutorial why-fish getting-started learning-fish running-commands getting-help syntax-highlighting autosuggestions tab-completions variables exports-shell-variables lists wildcards pipes-and-redirections command-substitutions separating-commands-semicolon exit-status combiners-and-or-not conditionals-if-else-switch functions loops prompt path startup-where-s-bashrc autoloading-functions universal-variables ready-for-more
    end

    set -l fish_help_page
    switch "$fish_help_item"
        case "!"
            set fish_help_page "cmds/not.html"
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
        case terminal-compatiblity
            set fish_help_page terminal-compatibility.html
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
            set fish_help_page "index.html#$fish_help_item"
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
            set -l tmpdir (__fish_mktemp_relative -d fish-help)
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
