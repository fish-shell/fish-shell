if test -d "$__fish_data_dir/man/man1/"
    complete -c help -x -a '(__fish_print_commands)' -d 'Help for this command'
end

# Help topics
# This was semi-automated with `grep 'class="anchor"' -A1 /usr/share/doc/fish/index.html
# It's not fully automated since that requires parsing html with regex,
# and since this is by definition in sync - we ship the html, and we ship these completions.
complete -c help -x -a autosuggestions -d Autosuggestions
complete -c help -x -a builtin-overview -d 'Builtin commands'
complete -c help -x -a cartesian-product -d 'Cartesian Products'
complete -c help -x -a color -d 'Setting syntax highlighting colors'
complete -c help -x -a combine -d 'Combining different expansions'
complete -c help -x -a completion -d 'How tab-completion works'
complete -c help -x -a completion-func -d 'Useful functions for writing completions'
complete -c help -x -a completion-own -d 'Writing your own completions'
complete -c help -x -a completion-path -d 'Where to put completions'
complete -c help -x -a debugging -d 'Debugging fish scripts'
complete -c help -x -a docs -d Help
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
complete -c help -x -a expand-process -d 'Process expansion %JOB'
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
complete -c help -x -a title -d 'Programmable title'
complete -c help -x -a variables -d 'Shell variables'
complete -c help -x -a variables-arrays -d Arrays
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

# Tutorial
complete -c help -x -a tutorial -d Tutorial
complete -c help -x -a tut_autoload -d 'Autoloading Functions'
complete -c help -x -a tut_autosuggestions -d Autosuggestions
complete -c help -x -a tut_combiners -d 'Combiners (And, Or, Not)'
complete -c help -x -a tut_command_substitutions -d 'Command Substitutions'
complete -c help -x -a tut_conditionals -d 'Conditionals (If, Else, Switch)'
complete -c help -x -a tut_exit_status -d 'Exit Status'
complete -c help -x -a tut_exports -d 'Exports (Shell Variables)'
complete -c help -x -a tut_functions -d Functions
complete -c help -x -a tut_getting_help -d 'Getting Help'
complete -c help -x -a tut_learning_Fish -d 'Learning fish'
complete -c help -x -a tut_lists -d Lists
complete -c help -x -a tut_loops -d Loops
complete -c help -x -a tut_more -d 'Ready for more?'
complete -c help -x -a tut_path -d '$PATH'
complete -c help -x -a tut_pipes_and_redirections -d 'Pipes and Redirections'
complete -c help -x -a tut_prompt -d Prompt
complete -c help -x -a tut_running_commands -d 'Running Commands'
complete -c help -x -a tut_semicolon -d 'Separating Commands (Semicolon)'
complete -c help -x -a tut_startup -d "Startup (Where's .bashrc?)"
complete -c help -x -a tut_syntax_highlighting -d 'Syntax Highlighting'
complete -c help -x -a tut_tab_completions -d 'Tab Completions'
complete -c help -x -a tut_universal -d 'Universal Variables'
complete -c help -x -a tut_variables -d Variables
complete -c help -x -a tut_why_fish -d 'Why fish?'
complete -c help -x -a tut_wildcards -d Wildcards
