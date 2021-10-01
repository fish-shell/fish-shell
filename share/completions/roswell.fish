### subcommands
complete -f -c ros -n '__fish_use_subcommand' -xa run -d "Run repl"
complete -f -c ros -n '__fish_use_subcommand' -xa install -d "Install a given implementation or a system for roswell environment"
complete -f -c ros -n '__fish_use_subcommand' -xa update -d "Update installed systems."
complete -f -c ros -n '__fish_use_subcommand' -xa build -d "Make executable from script."
complete -f -c ros -n '__fish_use_subcommand' -xa use -d "Change default implementation."
complete -f -c ros -n '__fish_use_subcommand' -xa init -d "Creates a new ros script, optionally based on a template."
complete -f -c ros -n '__fish_use_subcommand' -xa fmt -d "Indent lisp source."
complete -f -c ros -n '__fish_use_subcommand' -xa list -d "List Information"
complete -f -c ros -n '__fish_use_subcommand' -xa template -d "Manage templates"
complete -f -c ros -n '__fish_use_subcommand' -xa delete -d "Delete installed implementations"
complete -f -c ros -n '__fish_use_subcommand' -xa config -d "Get and set options"
complete -f -c ros -n '__fish_use_subcommand' -xa version -d "Show the roswell version information"
complete -f -c ros -n '__fish_use_subcommand' -xa help -d "Use \"ros help [command]\" for more information about a command."

### help
set -l __roswell_helps "run install update build use init fmt list template delete config version"
complete -c ros -n "__fish_seen_subcommand_from help" -xa $__roswell_helps

### run
complete -c ros -n '__fish_seen_subcommand_from run' -s l -d 'load lisp FILE while building'
complete -c ros -n '__fish_seen_subcommand_from run' -l load -d 'load lisp FILE while building'
complete -c ros -n '__fish_seen_subcommand_from run' -s l -d 'load lisp FILE while building'
complete -c ros -n '__fish_seen_subcommand_from run' -l load -d 'load lisp FILE while building'
complete -c ros -n '__fish_seen_subcommand_from run' -s S -d 'override source registry of asdf systems'
complete -c ros -n '__fish_seen_subcommand_from run' -l source-registry -d 'override source registry of asdf systems'
complete -c ros -n '__fish_seen_subcommand_from run' -s s -d 'load asdf SYSTEM while building'
complete -c ros -n '__fish_seen_subcommand_from run' -l system -d 'load asdf SYSTEM while building'
complete -c ros -n '__fish_seen_subcommand_from run' -l load-system -d 'same as above (buildapp compatibility)'
complete -c ros -n '__fish_seen_subcommand_from run' -s p -d 'change current package to PACKAGE'
complete -c ros -n '__fish_seen_subcommand_from run' -l package -d 'change current package to PACKAGE'
complete -c ros -n '__fish_seen_subcommand_from run' -o sp -d 'combination of -s SP and -p SP'
complete -c ros -n '__fish_seen_subcommand_from run' -l system-package -d ' combination of -s SP and -p SP'
complete -c ros -n '__fish_seen_subcommand_from run' -s e -d 'evaluate FORM while building'
complete -c ros -n '__fish_seen_subcommand_from run' -l eval -d 'evaluate FORM while building'
complete -c ros -n '__fish_seen_subcommand_from run' -l require -d 'require MODULE while building'
complete -c ros -n '__fish_seen_subcommand_from run' -s q -d 'quit lisp here'
complete -c ros -n '__fish_seen_subcommand_from run' -l quit -d 'quit lisp here'
complete -c ros -n '__fish_seen_subcommand_from run' -s r -d 'restart from build by calling (FUNC)'
complete -c ros -n '__fish_seen_subcommand_from run' -l restart -d 'restart from build by calling (FUNC)'
complete -c ros -n '__fish_seen_subcommand_from run' -s E -d 'restart from build by calling (FUNC argv)'
complete -c ros -n '__fish_seen_subcommand_from run' -l entry -d 'restart from build by calling (FUNC argv)'
complete -c ros -n '__fish_seen_subcommand_from run' -s i -d 'evaluate FORM after restart'
complete -c ros -n '__fish_seen_subcommand_from run' -l init -d 'evaluate FORM after restart'
complete -c ros -n '__fish_seen_subcommand_from run' -o ip -d 'evaluate and princ FORM after restart'
complete -c ros -n '__fish_seen_subcommand_from run' -l print -d 'evaluate and princ FORM after restart'
complete -c ros -n '__fish_seen_subcommand_from run' -o iw -d 'evaluate and write FORM after restart'
complete -c ros -n '__fish_seen_subcommand_from run' -l write -d 'evaluate and write FORM after restart'
complete -c ros -n '__fish_seen_subcommand_from run' -s F -d 'evaluate FORM before dumping IMAGE'
complete -c ros -n '__fish_seen_subcommand_from run' -l final -d 'evaluate FORM before dumping IMAGE'
complete -c ros -n '__fish_seen_subcommand_from run' -a +R -d 'skip /etc/rosrc, ~/.roswell/init.lisp'
complete -c ros -n '__fish_seen_subcommand_from run' -l "no-rc" -d 'skip /etc/rosrc, ~/.roswell/init.lisp'
complete -c ros -n '__fish_seen_subcommand_from run' -s A -d 'use new asdf'
complete -c ros -n '__fish_seen_subcommand_from run' -l asdf -d 'use new asdf'
complete -c ros -n '__fish_seen_subcommand_from run' -a +Q -d 'do not use quicklisp'
complete -c ros -n '__fish_seen_subcommand_from run' -l noquicklisp -d 'do not use quicklisp'
complete -c ros -n '__fish_seen_subcommand_from run' -s v -d 'be quite noisy while building'
complete -c ros -n '__fish_seen_subcommand_from run' -l verbose -d 'be quite noisy while building'
complete -c ros -n '__fish_seen_subcommand_from run' -l quiet -d 'be quite quiet while building (default)'
complete -c ros -n '__fish_seen_subcommand_from run' -l test -d 'for test purpose'

### list
set -l __roswell_lists "installed dump versions"
complete -c ros -n "__fish_seen_subcommand_from list" -xa $__roswell_lists

### template
complete -c ros -n "__fish_seen_subcommand_from template" -xa init -d "Create new template"
complete -c ros -n "__fish_seen_subcommand_from template" -xa deinit -d "Remove a template"
complete -c ros -n "__fish_seen_subcommand_from template" -xa list -d "List the installed templates"
complete -c ros -n "__fish_seen_subcommand_from template" -xa checkout -d "Checkout default template to edit."
complete -c ros -n "__fish_seen_subcommand_from template" -xa add -d "Add files to template."
complete -c ros -n "__fish_seen_subcommand_from template" -xa cat -d "Show file contents"
complete -c ros -n "__fish_seen_subcommand_from template" -xa edit -d "Edit file contents"
complete -c ros -n "__fish_seen_subcommand_from template" -xa rm -d "Remove (delete) files from template."
complete -c ros -n "__fish_seen_subcommand_from template" -xa delete -d "Remove (delete) files from template."
complete -c ros -n "__fish_seen_subcommand_from template" -xa type -d "Set template type for a file."
complete -c ros -n "__fish_seen_subcommand_from template" -xa chmod -d "Set mode for a file."
complete -c ros -n "__fish_seen_subcommand_from template" -xa rewrite -d "Set path rewrite rule for a file"
complete -c ros -n "__fish_seen_subcommand_from template" -xa export -d "Export template to directory"
complete -c ros -n "__fish_seen_subcommand_from template" -xa import -d "Import template"
complete -c ros -n "__fish_seen_subcommand_from template" -xa help -d "Print usage and subcommands description"

### config
complete -c ros -n "__fish_seen_subcommand_from config" -xa show -d "show TARGET."
complete -c ros -n "__fish_seen_subcommand_from config" -xa set -d "set TARGET VALUE."
