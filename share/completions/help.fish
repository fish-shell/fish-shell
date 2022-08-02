if test -d "$__fish_data_dir/man/man1/"
    complete -c help -x -a '(__fish_print_commands)' -d 'Help for this command'
end

# Help topics in index.html
# This was semi-automated with `grep 'class="anchor"' -A1 /usr/share/doc/fish/index.html
# It's not fully automated since that requires parsing html with regex,
# and since this is by definition in sync - we ship the html, and we ship these completions.
complete -c help -x -a autosuggestions -d Autosuggestions
complete -c help -x -a builtin-overview -d 'Builtin commands'
complete -c help -x -a cartesian-product -d 'Cartesian Products'
complete -c help -x -a color -d 'Setting syntax highlighting colors'
complete -c help -x -a combine -d 'Combining different expansions'
complete -c help -x -a debugging -d 'Debugging fish scripts'
complete -c help -x -a editor -d 'Command line editor'
complete -c help -x -a emacs-mode -d 'Emacs mode commands'
complete -c help -x -a escapes -d 'Escaping characters'
complete -c help -x -a event -d 'Event handlers'
complete -c help -x -a expand -d 'Parameter expansion (Globbing)'
complete -c help -x -a expand-brace -d 'Brace expansion {a,b,c}'
complete -c help -x -a expand-command-substitution -d 'Command substitution'
complete -c help -x -a expand-command-substitution -d 'Command substitution (SUBCOMMAND)'
complete -c help -x -a expand-home -d 'Home directory expansion ~USER'
complete -c help -x -a expand-index-range -d 'Index range expansion'
complete -c help -x -a expand-variable -d 'Variable expansion $VARNAME'
complete -c help -x -a expand-wildcard -d 'Wildcard expansion *.*'
# Note: This is hard-coded in help.fish - it's not an anchor in the html.
complete -c help -x -a globbing -d 'Parameter expansion (Globbing)'
complete -c help -x -a greeting -d 'Configurable greeting'
complete -c help -x -a history -d 'Help on how to reuse previously entered commands'
complete -c help -x -a history-search -d 'Searchable history'
complete -c help -x -a identifiers -d 'Shell variable and function names'
complete -c help -x -a initialization -d 'Initialization files'
complete -c help -x -a introduction -d Introduction
complete -c help -x -a job-control -d 'Running multiple programs'
complete -c help -x -a killring -d 'Copy and paste (Kill Ring)'
complete -c help -x -a more-help -d 'Further help and development'
complete -c help -x -a multiline -d 'Multiline editing'
complete -c help -x -a other -d 'Other features'
complete -c help -x -a piping -d Piping
complete -c help -x -a prompt -d 'Programmable prompt'
complete -c help -x -a quotes -d Quotes
complete -c help -x -a redirects -d 'Input/Output (IO) redirection'
complete -c help -x -a shared-binds -d 'Shared bindings'
complete -c help -x -a syntax -d 'Introduction to the fish syntax'
complete -c help -x -a syntax-background -d 'Background jobs'
complete -c help -x -a syntax-conditional -d 'Conditional execution of code and flow control'
complete -c help -x -a syntax-function -d Functions
complete -c help -x -a syntax-function-autoloading -d 'Autoloading functions'
complete -c help -x -a syntax-function-wrappers -d 'Defining aliases'
complete -c help -x -a syntax-job-control -d 'Job control'
complete -c help -x -a syntax-words -d 'Some common words'
complete -c help -x -a tab-completion -d 'How tab-completion works'
complete -c help -x -a title -d 'Programmable title'
complete -c help -x -a variables -d 'Shell variables'
complete -c help -x -a variables-lists -d Lists
complete -c help -x -a variables-color -d 'Variables for changing highlighting colors'
complete -c help -x -a variables-export -d 'Exporting variables'
complete -c help -x -a variables-functions -d 'Variable scope for functions'
complete -c help -x -a variables-locale -d 'Locale variables'
complete -c help -x -a variables-scope -d 'Variable scope'
complete -c help -x -a variables-special -d 'Special variables'
complete -c help -x -a variables-status -d 'The status variable'
complete -c help -x -a variables-universal -d 'More on universal variables'
complete -c help -x -a vi-mode -d 'Vi mode commands'
complete -c help -x -a vi-mode-command -d 'Command mode'
complete -c help -x -a vi-mode-insert -d 'Insert mode'
complete -c help -x -a vi-mode-visual -d 'Visual mode'

# Completions
complete -c help -x -a completion-own -d 'Writing your own completions'
complete -c help -x -a completion-func -d 'Useful functions for writing completions'
complete -c help -x -a completion-path -d 'Where to put completions'

# Tutorial
complete -c help -x -a tutorial -d Tutorial
complete -c help -x -a tut-autoload -d 'Autoloading Functions'
complete -c help -x -a tut-autosuggestions -d Autosuggestions
complete -c help -x -a tut-combiners -d 'Combiners (And, Or, Not)'
complete -c help -x -a tut-command_substitutions -d 'Command Substitutions'
complete -c help -x -a tut-conditionals -d 'Conditionals (If, Else, Switch)'
complete -c help -x -a tut-exit_status -d 'Exit Status'
complete -c help -x -a tut-exports -d 'Exports (Shell Variables)'
complete -c help -x -a tut-functions -d Functions
complete -c help -x -a tut-getting_help -d 'Getting Help'
complete -c help -x -a tut-learning_Fish -d 'Learning fish'
complete -c help -x -a tut-lists -d Lists
complete -c help -x -a tut-loops -d Loops
complete -c help -x -a tut-more -d 'Ready for more?'
complete -c help -x -a tut-path -d '$PATH'
complete -c help -x -a tut-pipes_and_redirections -d 'Pipes and Redirections'
complete -c help -x -a tut-prompt -d Prompt
complete -c help -x -a tut-running_commands -d 'Running Commands'
complete -c help -x -a tut-semicolon -d 'Separating Commands (Semicolon)'
complete -c help -x -a tut-startup -d "Startup (Where's .bashrc?)"
complete -c help -x -a tut-syntax_highlighting -d 'Syntax Highlighting'
complete -c help -x -a tut-tab_completions -d 'Tab Completions'
complete -c help -x -a tut-universal -d 'Universal Variables'
complete -c help -x -a tut-variables -d Variables
complete -c help -x -a tut-why_fish -d 'Why fish?'
complete -c help -x -a tut-wildcards -d Wildcards

# Commands
complete -c help -x -a abbr -d 'Manage fish abbreviations'
complete -c help -x -a alias -d 'Create a function'
complete -c help -x -a and -d 'Conditionally execute a command'
complete -c help -x -a argparse -d 'Parse options passed to a fish script or function'
complete -c help -x -a begin -d 'Start a new block of code'
complete -c help -x -a bg -d 'Send jobs to background'
complete -c help -x -a bind -d 'Handle fish key bindings'
complete -c help -x -a block -d 'Temporarily block delivery of events'
complete -c help -x -a breakpoint -d 'Launch debug mode'
complete -c help -x -a break -d 'Stop the current inner loop'
complete -c help -x -a builtin -d 'Run a builtin command'
complete -c help -x -a case -d 'Conditionally execute a block of commands'
complete -c help -x -a cdh -d 'Change to a recently visited directory'
complete -c help -x -a cd -d 'Change directory'
complete -c help -x -a commandline -d 'Set or get the current command line buffer'
complete -c help -x -a command -d 'Run a program'
complete -c help -x -a complete -d 'Edit command specific tab-completions'
complete -c help -x -a contains -d 'Test if a word is present in a list'
complete -c help -x -a continue -d 'Skip the remainder of the current iteration of the current inner loop'
complete -c help -x -a count -d 'Count the number of elements of a list'
complete -c help -x -a dirh -d 'Print directory history'
complete -c help -x -a dirs -d 'Print directory stack'
complete -c help -x -a disown -d 'Remove a process from the list of jobs'
complete -c help -x -a echo -d 'Display a line of text'
complete -c help -x -a else -d 'Execute command if a condition is not met'
complete -c help -x -a emit -d 'Emit a generic event'
complete -c help -x -a end -d 'End a block of commands'
complete -c help -x -a eval -d 'Evaluate the specified commands'
complete -c help -x -a exec -d 'Execute command in current process'
complete -c help -x -a exit -d 'Exit the shell'
complete -c help -x -a false -d 'Return an unsuccessful result'
complete -c help -x -a fg -d 'Bring job to foreground'
complete -c help -x -a fish_add_path -d 'Add to the path'
complete -c help -x -a fish_breakpoint_prompt -d 'Define the prompt when stopped at a breakpoint'
complete -c help -x -a fish_command_not_found -d 'What to do when a command wasn\'t found'
complete -c help -x -a fish_config -d 'Start the web-based configuration interface'
complete -c help -x -a fish_git_prompt -d 'Output git information for use in a prompt'
complete -c help -x -a fish_greeting -d 'Display a welcome message in interactive shells'
complete -c help -x -a fish_hg_prompt -d 'Output Mercurial information for use in a prompt'
complete -c help -x -a fish_indent -d 'Indenter and prettifier'
complete -c help -x -a fish_is_root_user -d 'Check if the current user is root'
complete -c help -x -a fish_key_reader -d 'Explore what characters keyboard keys send'
complete -c help -x -a fish_mode_prompt -d 'Define the appearance of the mode indicator'
complete -c help -x -a fish_opt -d 'Create an option specification for the argparse command'
complete -c help -x -a fish_prompt -d 'Define the appearance of the command line prompt'
complete -c help -x -a fish_right_prompt -d 'Define the appearance of the right-side command line prompt'
complete -c help -x -a fish -d 'The friendly interactive shell'
complete -c help -x -a fish_status_to_signal -d 'Convert exit codes to human-friendly signals'
complete -c help -x -a fish_svn_prompt -d 'Output Subversion information for use in a prompt'
complete -c help -x -a fish_title -d 'Define the terminal\'s title'
complete -c help -x -a fish_update_completions -d 'Update completions using manual pages'
complete -c help -x -a fish_vcs_prompt -d 'Output version control system information for use in a prompt'
complete -c help -x -a for -d 'Perform a set of commands multiple times'
complete -c help -x -a funced -d 'Edit a function interactively'
complete -c help -x -a funcsave -d 'Save the definition of a function to the user\'s autoload directory'
complete -c help -x -a function -d 'Create a function'
complete -c help -x -a functions -d 'Print or erase functions'
complete -c help -x -a help -d 'Display fish documentation'
complete -c help -x -a history -d 'Show and manipulate command history'
complete -c help -x -a if -d 'Conditionally execute a command'
complete -c help -x -a isatty -d 'Test if a file descriptor is a terminal'
complete -c help -x -a jobs -d 'Print currently running jobs'
complete -c help -x -a math -d 'Perform mathematics calculations'
complete -c help -x -a nextd -d 'Move forward through directory history'
complete -c help -x -a not -d 'Negate the exit status of a job'
complete -c help -x -a open -d 'Open file in its default application'
complete -c help -x -a or -d 'Conditionally execute a command'
complete -c help -x -a path -d 'Manipulate and check paths'
complete -c help -x -a popd -d 'Move through directory stack'
complete -c help -x -a prevd -d 'Move backward through directory history'
complete -c help -x -a printf -d 'Display text according to a format string'
complete -c help -x -a prompt_hostname -d 'Print the hostname, shortened for use in the prompt'
complete -c help -x -a prompt_login -d 'Describe the login suitable for prompt'
complete -c help -x -a prompt_pwd -d 'Print pwd suitable for prompt'
complete -c help -x -a psub -d 'Perform process substitution'
complete -c help -x -a pushd -d 'Push directory to directory stack'
complete -c help -x -a pwd -d 'Output the current working directory'
complete -c help -x -a random -d 'Generate random number'
complete -c help -x -a read -d 'Read line of input into variables'
complete -c help -x -a realpath -d 'Convert a path to an absolute path without symlinks'
complete -c help -x -a return -d 'Stop the current inner function'
complete -c help -x -a _ -d 'Call fish\'s translations'
complete -c help -x -a set_color -d 'Set the terminal color'
complete -c help -x -a set -d 'Display and change shell variables'
complete -c help -x -a source -d 'Evaluate contents of file'
complete -c help -x -a status -d 'Query fish runtime information'
complete -c help -x -a string-collect -d 'Join strings into one'
complete -c help -x -a string-escape -d 'Escape special characters'
complete -c help -x -a string-join0 -d 'Join strings with zero bytes'
complete -c help -x -a string-join -d 'Join strings with delimiter'
complete -c help -x -a string-length -d 'Print string lengths'
complete -c help -x -a string-lower -d 'Convert strings to lowercase'
complete -c help -x -a string-match -d 'Match substrings'
complete -c help -x -a string-pad -d 'Pad strings to a fixed width'
complete -c help -x -a string-repeat -d 'Multiply a string'
complete -c help -x -a string-replace -d 'Replace substrings'
complete -c help -x -a string -d 'Manipulate strings'
complete -c help -x -a string-split0 -d 'Split on zero bytes'
complete -c help -x -a string-split -d 'Split strings by delimiter'
complete -c help -x -a string-sub -d 'Extract substrings'
complete -c help -x -a string-trim -d 'Remove trailing whitespace'
complete -c help -x -a string-unescape -d 'Expand escape sequences'
complete -c help -x -a string-upper -d 'Convert strings to uppercase'
complete -c help -x -a suspend -d 'Suspend the current shell'
complete -c help -x -a switch -d 'Conditionally execute a block of commands'
complete -c help -x -a test -d 'Perform tests on files and text'
complete -c help -x -a time -d 'Measure how long a command or block takes'
complete -c help -x -a trap -d 'Perform an action when the shell receives a signal'
complete -c help -x -a true -d 'Return a successful result'
complete -c help -x -a type -d 'Locate a command and describe its type'
complete -c help -x -a ulimit -d 'Set or get resource usage limits'
complete -c help -x -a umask -d 'Set or get the file creation mode mask'
complete -c help -x -a vared -d 'Interactively edit the value of an environment variable'
complete -c help -x -a wait -d 'Wait for jobs to complete'
complete -c help -x -a while -d 'Perform a set of commands multiple times'

# Other pages
complete -c help -x -a releasenotes -d "Fish's release notes"
complete -c help -x -a completions -d "How to write completions"
complete -c help -x -a faq -d "Frequently Asked Questions"
complete -c help -x -a fish-for-bash-users -d "Differences from bash"

complete -c help -x -a argument-handling -d "How to handle arguments"
complete -c help -x -a autoloading-functions -d "How functions are loaded"
complete -c help -x -a brace-expansion -d "{a,b} brace expansion"
complete -c help -x -a builtin-commands -d "An overview of fish's builtins"
complete -c help -x -a combining-different-expansions -d "How different expansions work together"
complete -c help -x -a combining-lists-cartesian-product -d "How lists combine"
complete -c help -x -a command-substitution -d "(command) command substitution"
complete -c help -x -a comments -d "# comments"
complete -c help -x -a conditions -d "ifs and elses"
complete -c help -x -a defining-aliases -d "How to define an alias"
complete -c help -x -a escaping-characters -d "How \\\\ escaping works"
complete -c help -x -a exporting-variables -d "What set -x does"
complete -c help -x -a functions -d "How to define functions"
complete -c help -x -a home-directory-expansion -d "~ expansion"
complete -c help -x -a index-range-expansion -d "var[x..y] slices"
complete -c help -x -a input-output-redirection -d "< and > redirectoins"
complete -c help -x -a lists -d "Variables with multiple elements"
complete -c help -x -a loops-and-blocks -d "while, for and begin"
complete -c help -x -a more-on-universal-variables
complete -c help -x -a overriding-variables-for-a-single-command -d "foo=bar variable overrides"
complete -c help -x -a pager-color-variables -d "How to color the pager"
complete -c help -x -a path-variables -d 'Why $PATH is special'
complete -c help -x -a shell-variable-and-function-names -d 'What characters are allowed in names'
complete -c help -x -a shell-variables
complete -c help -x -a special-variables
complete -c help -x -a syntax-highlighting-variables
complete -c help -x -a syntax-overview
complete -c help -x -a terminology
complete -c help -x -a the-fish-language
complete -c help -x -a the-status-variable -d '$status, the return code'
complete -c help -x -a variable-expansion -d '$variable'
complete -c help -x -a variable-scope -d 'Local, global and universal scope'
complete -c help -x -a variable-scope-for-functions
complete -c help -x -a wildcards-globbing
complete -c help -x -a abbreviations
complete -c help -x -a command-line-editor
complete -c help -x -a configurable-greeting
complete -c help -x -a copy-and-paste-kill-ring
complete -c help -x -a custom-bindings
complete -c help -x -a directory-stack
complete -c help -x -a emacs-mode-commands
complete -c help -x -a help
complete -c help -x -a interactive-use
complete -c help -x -a multiline-editing
complete -c help -x -a navigating-directories
complete -c help -x -a private-mode
complete -c help -x -a programmable-prompt
complete -c help -x -a programmable-title
complete -c help -x -a searchable-command-history
complete -c help -x -a shared-bindings
complete -c help -x -a syntax-highlighting
complete -c help -x -a vi-mode-commands
complete -c help -x -a arithmetic-expansion
complete -c help -x -a blocks-and-loops
complete -c help -x -a builtins-and-other-commands
complete -c help -x -a command-substitutions
complete -c help -x -a heredocs
complete -c help -x -a process-substitution
complete -c help -x -a prompts -d 'How to make your own prompt'
complete -c help -x -a quoting -d 'How "" and \'\' work'
complete -c help -x -a special-variables
complete -c help -x -a string-manipulation
complete -c help -x -a test-test
complete -c help -x -a wildcards-globs
complete -c help -x -a frequently-asked-questions
