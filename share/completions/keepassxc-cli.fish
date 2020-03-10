# Completions for keepassxc-cli

# General options
complete -c keepassxc-cli -l debug-info -d "Show debug info"
complete -r -c keepassxc-cli -s k -l key-file -d "Specify a key file"
complete -c keepassxc-cli -l no-password -d "Deactivate the password key"
complete -r -c keepassxc-cli -s y -l yubikey -d "Specify a yubikey slot"
complete -r -c keepassxc-cli -s q -l quiet -d "Be quiet"
complete -c keepassxc-cli -s h -l help -d "Show help info"
complete -c keepassxc-cli -s v -l version -d "Show version info"

# Commands
complete -f -c keepassxc-cli -n __fish_use_subcommand -a add -d "Add a new entry"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a analyze -d "Analyze password for weakness and problem"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a clip -d "Copy an entry's password to the clipboard"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a close -d "Close the currently opened database"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a create -d "Create new database"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a diceware -d "Generate a new random diceware passphrase"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a edit -d "Edit an entry"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a estimate -d "Estimate the entropy of a password"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a exit -d "Exit interactive mode"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a export -d "Export the contents to stdout"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a generate -d "Generate a new password"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a help -d "Show command help"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a import -d "Import the contents"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a locate -d "Find entries quickly"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a ls -d "List database entries"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a merge -d "Merge two databases"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a mkdir -d "Add a new group"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a mv -d "Move an entry"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a open -d "Open a database"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a quit -d "Exit interactive mode"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a rm -d "Remove an entry"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a rmdir -d "Remove a group"
complete -f -c keepassxc-cli -n __fish_use_subcommand -a show -d "Show an entry's info"

## Merge options
complete -r -c keepassxc-cli -n "__fish_seen_subcommand_from merge" -s d -l dry-run -d "Dry run"
complete -r -c keepassxc-cli -n "__fish_seen_subcommand_from merge" -l key-file-from -d "Key file to merge from"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from merge" -l no-password-from -d "Deactivate the password key to merge from"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from merge" -l yubikey-from -d "Yubikey slot for the second database"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from merge" -s s -l same-credentials -d "Use the same credentials for both databases"

## Add and edit options
complete -x -c keepassxc-cli -n "__fish_seen_subcommand_from add edit" -s u -l username -d "Username for the entry"
complete -f -c keepassxc-cli -n "__fish_seen_subcommand_from add edit" -l url -d "URL for the entry"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from add edit" -p u -l password-prompt -d "Prompt for the entry's password"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from add edit" -s g -l generate -d "Generate a password for the entry"

## Edit options
complete -x -c keepassxc-cli -n "__fish_seen_subcommand_from edit" -s t -l title -d "Title for the entry"

## Estimate options
complete -c keepassxc-cli -n "__fish_seen_subcommand_from estimate" -s a -l advanced -d "Perform advanced analysis on the password"

## Analyze options
complete -r -c keepassxc-cli -n "__fish_seen_subcommand_from analyze" -s H -l hibp -d "Check if any passwords have been publicly leaked"

## Clip options
complete -c keepassxc-cli -n "__fish_seen_subcommand_from clip" -s t -l totp -d "Copy the current TOTP to the clipboard"

## Show options
complete -x -c keepassxc-cli -n "__fish_seen_subcommand_from show" -s a -l attributes -d "Names of the attributes to show"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from show" -s s -l show-protected -d "Show the protected attributes in clear text"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from show" -s t -l totp -d "Show the entry's current TOTP"

## Diceware options
complete -x -c keepassxc-cli -n "__fish_seen_subcommand_from diceware" -s W -l words -d "Word count for the diceware passphrase"
complete -x -c keepassxc-cli -n "__fish_seen_subcommand_from diceware" -s w -l word-list -d "Wordlist for the diceware generator"

## Export options
complete -x -c keepassxc-cli -n "__fish_seen_subcommand_from export" -s f -l format -a "xml csv" -d "Format to use when exporting"

## List options
complete -c keepassxc-cli -n "__fish_seen_subcommand_from ls" -s R -l recursive -d "Recursively list the elements of the group"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from ls" -s f -l flatten -d "Flattens the output to single line"

## Generate options
complete -x -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -s L -l length -d "Length of the generated password"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -s l -l lower -d "Use lowercase chars"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -s U -l upper -d "Use uppercase chars"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -s n -l numeric -d "Use numbers"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -s s -l special -d "Use special chars"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -s e -l extended -d "Use extended ASCII"
complete -x -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -s x -l exclude -d "Exclude char set"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -l exclude-similar -d "Exclude similar looking chars"
complete -c keepassxc-cli -n "__fish_seen_subcommand_from generate" -l every-group -d "Include chars from every selected group"
