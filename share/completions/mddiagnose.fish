# completion for mddiagnose (macOS)

complete -c mddiagnose -s h -f -d 'Display help'
complete -c mddiagnose -s d -f -d 'Ignore unknown options'
complete -c mddiagnose -s n -f -d 'Do not reveal the resulting package in the Finder'
complete -c mddiagnose -s r -f -d 'Avoid restricted operations such as heap'
complete -c mddiagnose -s s -f -d 'Skip gathering system.log'
complete -c mddiagnose -s v -f -d 'Prints version of mddiagnose'
complete -c mddiagnose -s m -f -d 'Minimal report'
complete -c mddiagnose -s e -r -d 'Evaluate indexing information for path'
complete -c mddiagnose -s p -r -d 'Evaluate permissions information for path'
complete -c mddiagnose -s f -r -d 'Write the diagnostic to the specified path'
