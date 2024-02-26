function __fish_ansible_vault_no_subcommand -d 'Test if ansible-vault has yet to be given the subcommand'
    commandline -pc | not string match --regex '[^-]\b(?:create|decrypt|edit|encrypt|encrypt_string|rekey|view)\b'
end

function __fish_ansible_vault_using_command
    set -l cmd (string trim (__fish_ansible_vault_no_subcommand))
    test -z "$cmd"
    and return 1
    contains -- "$cmd" $argv
end

# generic options
complete -c ansible-vault -l version -d "Display version and exit"
complete -c ansible-vault -s h -l help -f -d "Show help message and exit"
complete -c ansible-vault -s v -l verbose -d "Verbose mode (-vvv for more, -vvvv to enable connection debugging)"
complete -c ansible-vault -l ask-vault-pass -f -d "Ask for vault password"
complete -c ansible-vault -l new-vault-id -r -d "the new vault identity to use for rekey"
complete -c ansible-vault -l new-vault-password-file -r -d "New vault password file for rekey"
complete -c ansible-vault -l vault-id -r -d "the vault identity to use"
complete -c ansible-vault -l vault-password-file -r -d "Vault password file"

# subcommands
complete -c ansible-vault -n __fish_ansible_vault_no_subcommand -a decrypt -d 'Decrypt encrypted file or stdin'
complete -c ansible-vault -n __fish_ansible_vault_no_subcommand -a encrypt -d 'Encrypt a file or stdin'
complete -c ansible-vault -n __fish_ansible_vault_no_subcommand -ra create -d 'Create encrypted file'
complete -c ansible-vault -n __fish_ansible_vault_no_subcommand -ra edit -d 'Edit encrypted file'
complete -c ansible-vault -n __fish_ansible_vault_no_subcommand -ra rekey -d 'Rekey encrypted file'
complete -c ansible-vault -n __fish_ansible_vault_no_subcommand -ra view -d 'View contents of something encrypted'
complete -f -c ansible-vault -n __fish_ansible_vault_no_subcommand -a encrypt_string -d 'Encrypt string'

# encrypt_string options
complete -c ansible-vault -n '__fish_ansible_vault_using_command encrypt encrypt_string' -r -l stdin-name -f -d 'Specify the variable name for stdin'
complete -c ansible-vault -n '__fish_ansible_vault_using_command encrypt encrypt_string' -r -s n -l name -f -d 'Specify the variable name'
complete -c ansible-vault -n '__fish_ansible_vault_using_command encrypt encrypt_string' -s p -l prompt -d 'Prompt for the string to encrypt'

# shared options
complete -c ansible-vault -n '__fish_ansible_vault_using_command encrypt encrypt_string rekey edit create' -l encrypt-vault-id -r -d "the vault id used to encrypt (required if more than vault-id is provided)"
complete -c ansible-vault -n '__fish_ansible_vault_using_command encrypt encrypt_string decrypt' -l output -r -d "Output file name for encrypt or decrypt; use - for stdout"
