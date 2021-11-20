function __fish_seen_tokens --description 'Check whether there are already specified tokens in command line buffer'
  set --local cmd (commandline --tokenize --cut-at-cursor)
  set --erase cmd[1]
  
  for t in $cmd
    contains -- "$t" $argv && return 0
  end

  return 1
end

set --global COMMANDS -i -il -u -ul -us -l

complete --command gacutil --short-option '?' --description 'Show help'

complete --command gacutil --short-option i --condition "! __fish_seen_tokens $COMMANDS" --description 'Install an assembly into the global assembly cache'
complete --command gacutil --old-option il --condition "! __fish_seen_tokens $COMMANDS" --description 'Install one or more assemblies into the global assembly cache'
complete --command gacutil --short-option u --condition "! __fish_seen_tokens $COMMANDS" --description 'Uninstall an assembly from the global assembly cache'
complete --command gacutil --old-option ul --condition "! __fish_seen_tokens $COMMANDS" --description 'Uninstall one or more assemblies from the global assembly cache'
complete --command gacutil --old-option us --condition "! __fish_seen_tokens $COMMANDS" --description 'Uninstall an assembly using the specifed assemblies full name'
complete --command gacutil --short-option l --condition "! __fish_seen_tokens $COMMANDS" --description 'List the contents of the global assembly cache'

complete --command gacutil --old-option package --condition '__fish_seen_tokens -i -il -u -ul -us' --description 'Create a directory in prefix/lib/mono'
complete --command gacutil --old-option gacdir --condition "__fish_seen_tokens $COMMANDS" --description 'Use the GACs base directory'
complete --command gacutil --old-option root --condition "__fish_seen_tokens $COMMANDS" --description 'Integrate with automake tools or packaging tools that require a prefix directory to be specified'
complete --command gacutil --old-option check_refs --condition '__fish_seen_tokens -i -il' --description 'Check the assembly being installed into the GAC does not reference any non strong named assemblies'
