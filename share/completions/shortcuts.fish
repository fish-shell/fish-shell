# apple shortcuts.app CLI tool completion sucker downer by Aaron Gyes
# https://en.wikipedia.org/wiki/Shortcuts_(app)

# Apple added fish completion output to Swift's ArgumentParser.
# imagine my surprise when I found fish function stirngs in binaries in /usr/bin!

# checking the path is as expected is about as far as we're going with validation
if test "$(command -s shortcuts)" = /usr/bin/shortcuts
    command shortcuts --generate-completion-script=fish | source
end

# output is like: 

# function __fish_shortcuts_using_command
#     set cmd (commandline -xpc)
#     if test  (count $cmd) -eq (count $argv) 
#         for i in (seq (count $argv))
#             if test  $cmd[$i] != $argv[$i] 
#                 return 1
#             end
#         end
#         return 0
#     end
#     return 1
# end
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts' -f -a 'run' -d 'Run a shortcut.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts' -f -a 'list' -d 'List your shortcuts.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts' -f -a 'view' -d 'View a shortcut in Shortcuts.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts' -f -a 'sign' -d 'Sign a shortcut file.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts' -f -a 'help' -d 'Show subcommand help information.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts run' -f -r -s i -l input-path -d 'The input to provide to the shortcut.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts run -i' -f -a '(for i in *.{}; echo $i;end)'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts run --input-path' -f -a '(for i in *.{}; echo $i;end)'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts run' -f -r -s o -l output-path -d 'Where to write the shortcut output, if applicable.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts run -o' -f -a '(for i in *.{}; echo $i;end)'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts run --output-path' -f -a '(for i in *.{}; echo $i;end)'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts run' -f -r -l output-type -d 'What type to output data in, in Universal Type Identifier format.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts list' -f -r -s f -l folder-name -d 'The folder to list.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts list -f' -f -a '(command shortcuts ---completion list -- --custom (commandline -xpc)[1..-1])'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts list --folder-name' -f -a '(command shortcuts ---completion list -- --custom (commandline -xpc)[1..-1])'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts list' -f -l folders -d 'List folders instead of shortcuts.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts sign' -f -r -s m -l mode -d 'The signing mode.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts sign' -f -r -s i -l input -d 'The shortcut file to sign.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts sign -i' -f -a '(for i in *.{shortcut}; echo $i;end)'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts sign --input' -f -a '(for i in *.{shortcut}; echo $i;end)'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts sign' -f -r -s o -l output -d 'Output path for the signed shortcut file.'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts sign -o' -f -a '(for i in *.{shortcut,wflow}; echo $i;end)'
# complete -c shortcuts -n '__fish_shortcuts_using_command shortcuts sign --output' -f -a '(for i in *.{shortcut,wflow}; echo $i;end)'
