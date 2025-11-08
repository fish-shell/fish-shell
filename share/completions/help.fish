complete -c help -n __fish_is_first_arg -x -a '(
    {
        status help-sections | string replace -r "^index(#|\$)" introduction\$1
        printf cmds/%s\n ! . : \[ \{
    } |
        while read -l item
            printf "%s\t%s\n" $item "$(__fish_help_describe $item)"
        end
)'

function __fish_help_describe -a help_item
    switch $help_item
        case 'cmds/*'
            echo (_ "Help for this command")
        case commands
            return
        case commands#helper-commands
            return
        case commands#helper-functions
            return
        case commands#keywords
            return
        case commands#known-functions
            return
        case commands#the-full-list
            return
        case commands#tools
            return
        case completions
            echo (_ "How to write completions")
        case completions#useful-functions-for-writing-completions
            echo (_ 'Useful functions for writing completions')
        case completions#where-to-put-completions
            echo (_ 'Where to put completions')
        case contributing
            return
        case contributing#adding-translations-for-a-new-language
            return
        case contributing#code-style
            return
        case contributing#configuring-your-editor-for-fish-scripts
            return
        case contributing#contributing-completions
            return
        case contributing#contributing-documentation
            return
        case contributing#contributing-translations
            return
        case contributing#editing-po-files
            return
        case contributing#fish-script-style-guide
            return
        case contributing#github
            return
        case contributing#guidelines
            return
        case contributing#local-testing
            return
        case contributing#mailing-list
            return
        case contributing#minimum-supported-rust-version-msrv-policy
            return
        case contributing#modifications-to-strings-in-source-files
            return
        case contributing#modifying-existing-translations
            return
        case contributing#setting-code-up-for-translations
            return
        case contributing#testing
            return
        case contributing#updating-dependencies
            return
        case contributing#versioning
            return
        case design
            return
        case design#configurability-is-the-root-of-all-evil
            return
        case design#the-law-of-discoverability
            return
        case design#the-law-of-orthogonality
            return
        case design#the-law-of-responsiveness
            return
        case design#the-law-of-user-focus
            return
        case faq
            echo (_ "Frequently Asked Questions")
        case faq#fish-does-not-work-in-a-specific-terminal
            return
        case faq#how-do-i-change-the-greeting-message
            return
        case faq#how-do-i-check-whether-a-variable-is-defined
            return
        case faq#how-do-i-check-whether-a-variable-is-not-empty
            return
        case faq#how-do-i-customize-my-syntax-highlighting-colors
            return
        case faq#how-do-i-get-the-exit-status-of-a-command
            return
        case faq#how-do-i-run-a-command-every-login-what-s-fish-s-equivalent-to-bashrc-or-profile
            return
        case faq#how-do-i-run-a-command-from-history
            return
        case faq#how-do-i-run-a-subcommand-the-backtick-doesn-t-work
            return
        case faq#how-do-i-set-my-prompt
            return
        case faq#how-do-i-set-or-clear-an-environment-variable
            return
        case faq#i-m-getting-weird-graphical-glitches-a-staircase-effect-ghost-characters-cursor-in-the-wrong-position
            return
        case faq#my-command-pkg-config-gives-its-output-as-a-single-long-string
            return
        case faq#my-command-prints-no-matches-for-wildcard-but-works-in-bash
            return
        case faq#uninstalling-fish
            return
        case faq#what-is-the-equivalent-to-this-thing-from-bash-or-other-shells
            return
        case faq#why-does-my-prompt-show-a-i
            return
        case faq#why-doesn-t-history-substitution-etc-work
            return
        case faq#why-doesn-t-set-ux-exported-universal-variables-seem-to-work
            return
        case faq#why-won-t-ssh-scp-rsync-connect-properly-when-fish-is-my-login-shell
            return
        case fish_for_bash_users
            echo (_ "Differences from bash")
        case fish_for_bash_users#arithmetic-expansion
            return
        case fish_for_bash_users#blocks-and-loops
            return
        case fish_for_bash_users#builtins-and-other-commands
            return
        case fish_for_bash_users#command-substitutions
            return
        case fish_for_bash_users#heredocs
            return
        case fish_for_bash_users#other-facilities
            return
        case fish_for_bash_users#process-substitution
            return
        case fish_for_bash_users#prompts
            echo (_ 'How to make your own prompt')
        case fish_for_bash_users#quoting
            echo (_ 'How "" and \'\' work')
        case fish_for_bash_users#special-variables
            return
        case fish_for_bash_users#string-manipulation
            return
        case fish_for_bash_users#subshells
            return
        case fish_for_bash_users#test-test
            return
        case fish_for_bash_users#variables
            return
        case fish_for_bash_users#wildcards-globs
            return
        case introduction
            echo (_ Introduction)
        case introduction#configuration
            echo (_ 'Initialization files')
        case introduction#default-shell
            return
        case introduction#examples
            return
        case introduction#installation
            return
        case introduction#other-help-pages
            return
        case introduction#resources
            echo (_ 'Further help and development')
        case introduction#setup
            return
        case introduction#shebang-line
            return
        case introduction#starting-and-exiting
            return
        case introduction#uninstalling
            return
        case introduction#where-to-go
            return
        case interactive
            return
        case interactive#abbreviations
            return
        case interactive#autosuggestions
            echo (_ Autosuggestions)
        case interactive#command-line-editor
            echo (_ 'Command line editor')
        case interactive#command-mode
            echo (_ 'Command mode')
        case interactive#configurable-greeting
            echo (_ 'Configurable greeting')
        case interactive#copy-and-paste-kill-ring
            echo (_ 'Copy and paste (Kill Ring)')
        case interactive#custom-bindings
            return
        case interactive#directory-stack
            return
        case interactive#emacs-mode-commands
            echo (_ 'Emacs mode commands')
        case interactive#help
            return
        case interactive#insert-mode
            echo (_ 'Insert mode')
        case interactive#key-sequences
            return
        case interactive#multiline-editing
            echo (_ 'Multiline editing')
        case interactive#navigating-directories
            return
        case interactive#pager-color-variables
            echo (_ "How to color the pager")
        case interactive#private-mode
            return
        case interactive#programmable-prompt
            return
        case interactive#programmable-title
            echo (_ 'Programmable title')
        case interactive#searchable-command-history
            echo (_ 'Searchable history')
        case interactive#shared-bindings
            echo (_ 'Shared bindings')
        case interactive#syntax-highlighting
            echo (_ 'Setting syntax highlighting colors')
        case interactive#syntax-highlighting-variables
            echo (_ 'Variables for changing highlighting colors')
        case interactive#tab-completion
            echo (_ 'How tab-completion works')
        case interactive#vi-mode-commands
            return
        case interactive#visual-mode
            echo (_ 'Visual mode')
        case language
            return
        case language#argument-handling
            echo (_ "How to handle arguments")
        case language#autoloading-functions
            echo (_ "How functions are loaded")
        case language#brace-expansion
            echo (_ 'Brace expansion {a,b,c}')
        case language#builtin-commands
            echo (_ "An overview of fish's builtins")
        case language#combiners-and-or
            return
        case language#combining-different-expansions
            echo (_ "How different expansions work together")
        case language#combining-lists
            echo (_ 'Cartesian Products')
        case language#combining-pipes-and-redirections
            return
        case language#command-lookup
            return
        case language#command-substitution
            echo (_ "Command substitution")
        case language#comments
            echo (_ "# comments")
        case language#conditions
            echo (_ 'Conditional execution of code and flow control')
        case language#configuration-files
            return
        case language#debugging-fish-scripts
            echo (_ 'Debugging fish scripts')
        case language#defining-aliases
            echo (_ "How to define an alias")
        case language#dereferencing-variables
            return
        case language#escaping-characters
            echo (_ "How \\\\ escaping works")
        case language#event-handlers
            echo (_ 'Event handlers')
        case language#exporting-variables
            echo (_ 'Exporting variables')
        case language#functions
            echo (_ "How to define functions")
        case language#future-feature-flags
            return
        case language#home-directory-expansion
            echo (_ 'Home directory expansion ~USER')
        case language#input-output-redirection
            echo (_ 'Input/Output (IO) redirection')
        case language#job-control
            echo (_ 'Running multiple programs')
        case language#lists
            echo (_ "Variables with multiple elements")
        case language#locale-variables
            echo (_ 'Locale variables')
        case language#loops-and-blocks
            echo (_ "while, for and begin")
        case language#overriding-variables-for-a-single-command
            echo (_ "foo=bar variable overrides")
        case language#parameter-expansion
            echo (_ 'Parameter expansion (Globbing)')
        case language#path-variables
            echo (_ 'Why $PATH is special')
        case language#piping
            echo (_ Piping)
        case language#profiling-fish-scripts
            return
        case language#querying-for-user-input
            return
        case language#quotes
            echo (_ Quotes)
        case language#quoting-variables
            echo (_ 'How "" and \'\' work')
        case language#shell-variable-and-function-names
            echo (_ 'What characters are allowed in names')
        case language#shell-variables
            echo (_ 'Shell variables')
        case language#slices
            echo (_ 'Index range expansion')
        case language#special-variables
            echo (_ 'Special variables')
        case language#syntax-overview
            echo (_ 'Introduction to the fish syntax')
        case language#table-of-operators
            return
        case language#terminology
            echo (_ Terminology)
        case language#the-if-statement
            return
        case language#the-status-variable
            echo (_ "$status, the return code")
        case language#the-switch-statement
            return
        case language#universal-variables
            echo (_ 'More on universal variables')
        case language#variable-expansion
            echo (_ 'Variable expansion $VARNAME')
        case language#variable-scope
            echo (_ 'Variable scope')
        case language#variables-as-command
            return
        case language#wildcards-globbing
            echo (_ 'Wildcard expansion *.*')
        case license
            return
        case license#gnu-library-general-public-license
            return
        case license#license-for-fish
            return
        case license#license-for-the-python-docs-theme
            return
        case license#mit-license
            return
        case prompt
            echo (_ 'How to write your own prompt')
        case prompt#adding-color
            return
        case prompt#formatting
            return
        case prompt#our-first-prompt
            return
        case prompt#save-the-prompt
            return
        case prompt#shortening-the-working-directory
            return
        case prompt#status
            return
        case prompt#transient-prompt
            return
        case prompt#where-to-go-from-here
            return
        case relnotes
            echo (_ "Fish's release notes")
        case terminal-compatibility
            echo (_ "Terminal features used by fish")
        case terminal-compatibility#dcs-commands-and-gnu-screen
            return
        case terminal-compatibility#optional-commands
            return
        case terminal-compatibility#required-commands
            return
        case terminal-compatibility#unicode-codepoints
            return
        case tutorial
            echo (_ Tutorial)
        case tutorial#autoloading-functions
            echo (_ 'Autoloading Functions')
        case tutorial#autosuggestions
            echo (_ Autosuggestions)
        case tutorial#combiners-and-or-not
            echo (_ 'Combiners (And, Or, Not)')
        case tutorial#command-substitutions
            echo (_ 'Command Substitutions')
        case tutorial#conditionals-if-else-switch
            echo (_ 'Conditionals (If, Else, Switch)')
        case tutorial#exit-status
            echo (_ 'Exit Status')
        case tutorial#exports-shell-variables
            echo (_ 'Exports (Shell Variables)')
        case tutorial#functions
            echo (_ Functions)
        case tutorial#getting-help
            echo (_ 'Getting Help')
        case tutorial#getting-started
            return
        case tutorial#learning-fish
            echo (_ 'Learning fish')
        case tutorial#lists
            echo (_ Lists)
        case tutorial#loops
            echo (_ Loops)
        case tutorial#path
            echo (_ "$PATH")
        case tutorial#pipes-and-redirections
            echo (_ 'Pipes and Redirections')
        case tutorial#prompt
            echo (_ Prompt)
        case tutorial#ready-for-more
            echo (_ 'Ready for more?')
        case tutorial#running-commands
            echo (_ 'Running Commands')
        case tutorial#separating-commands-semicolon
            echo (_ 'Separating Commands (Semicolon)')
        case tutorial#startup-where-s-bashrc
            echo (_ "Startup (Where's .bashrc?)")
        case tutorial#syntax-highlighting
            echo (_ 'Syntax Highlighting')
        case tutorial#tab-completions
            echo (_ 'Tab Completions')
        case tutorial#universal-variables
            echo (_ 'Universal Variables')
        case tutorial#variables
            echo (_ Variables)
        case tutorial#why-fish
            echo (_ 'Why fish?')
        case tutorial#wildcards
            echo (_ Wildcards)
    end
end
