complete --command powershell --short-option '?' --old-option Help --description 'Show help'

complete --command powershell --old-option PSConsoleFile --require-parameter \
    --description 'Load the specified PowerShell console file'
complete --command powershell --old-option Version --require-parameter --arguments '2.0 3.0' \
    --description 'Start the specified version of PowerShell'
complete --command powershell --old-option NoLogo --description 'Hides the copyright banner at startup'
complete --command powershell --old-option NoExit \
    --description 'Do not exit after running startup commands'
complete --command powershell --old-option Sta \
    --description 'Start PowerShell using a single-threaded apartment'
complete --command powershell --old-option Mta \
    --description 'Start PowerShell using a multi-threaded apartment'
complete --command powershell --old-option NoProfile --description 'Does not load the PowerShell profile'
complete --command powershell --old-option NonInteractive \
    --description 'Do not present an interactive prompt to the user'
complete --command powershell --old-option InputFormat --no-files --require-parameter \
    --arguments 'Text XML' --description 'Describe the format of data sent to PowerShell'
complete --command powershell --old-option OutputFormat --no-files --require-parameter \
    --arguments 'Text XML' --description 'Determine how output is formatted'
complete --command powershell --old-option WindowStyle --no-files --require-parameter \
    --arguments 'Normal Minimized Maximized Hidden' --description 'Set the window style for the session'
complete --command powershell --old-option EncodedCommand --require-parameter \
    --description 'Accept a base-64-encoded string version of a command'
complete --command powershell --old-option ConfigurationName --no-files --require-parameter \
    --description 'Specify a configuration endpoint in which PowerShell is run'
complete --command powershell --old-option File --require-parameter
complete --command powershell --old-option ExecutionPolicy --no-files --require-parameter \
    --arguments 'AllSigned Bypass Default RemoteSigned Restricted Undefined Unrestricted' \
    --description 'Sets the default execution policy for the current session'
complete --command powershell --old-option Command --no-files --require-parameter \
    --description 'Execute the specified commands at the PowerShell command prompt'
