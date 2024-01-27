# Completion for SOPS (Secrets OPerationS)

function __fish_sops_no_subcommand --description "Test if there is a subcommand given"
    not __fish_sops_print_remaining_args
end

function __fish_sops_print_remaining_args --description "Print remaining argument given"
    set -l cmd (commandline -pxc) (commandline -ct)
    set -e cmd[1]
    set -l opts d/decrypt e/encrypt r/rotate i/in-place s/show-master-keys v/version
    set -a opts extract ignore-mac verbose enable-local-keyservice
    set -a opts k/kms= p/pgp= a/age=
    set -a opts gcp-kms= aws-profile= azure-kv= hc-vault-transit= input-type= output-type=
    set -a opts add-gcp-kms= rm-gcp-kms= add-azure-kv= rm-azure-kv= add-kms= rm-kms=
    set -a opts add-hc-vault-transit= rm-hc-vault-transit= add-age= rm-age= add-pgp= rm-pgp=
    set -a opts unencrypted-suffix= encrypted-suffix= unencrypted-regex= encrypted-regex=
    set -a opts config= encryption-context= set= shamir-secret-sharing-threshold= output= keyservice=
    argparse -s $opts -- $cmd 2>/dev/null
    if test -n "$argv"
        and not string match -qr '^-' $argv[1]
        string join0 -- $argv
        return 0
    else
        return 1
    end
end

function __fish_sops_commands --description "Test if argument(s) match a sops command"
    set -l args (__fish_sops_print_remaining_args | string split0)
    if string match -qr $argv $args[1]
        return 0
    else
        return 1
    end
end

# Primary commands
complete -F -c sops -n __fish_is_first_token -a exec-env -d "Execute a command with decrypted values inserted into the environment"
complete -F -c sops -n __fish_is_first_token -a exec-file -d "Execute a command with decrypted contents as a temporary file"
complete -F -c sops -n __fish_is_first_token -a publish -d "Publish sops file or directory to a configured destination"
complete -F -c sops -n __fish_is_first_token -a keyservice -d "Start a SOPS key kervice server"
complete -F -c sops -n __fish_is_first_token -a groups -d "Modify the groups on a SOPS file"
complete -F -c sops -n __fish_is_first_token -a updatekeys -d "Update the keys of a SOPS file using the config file"
complete -F -c sops -n __fish_is_first_token -a help -d "Shows a list of commands or help for one command"
complete -F -c sops -n __fish_is_first_token -a h -d "Shows a list of commands or help for one command"

# Primary flags without parameters
complete -F -c sops -n __fish_sops_no_subcommand -s d -l decrypt -d "Decrypt a file and output the result to stdout"
complete -F -c sops -n __fish_sops_no_subcommand -s e -l encrypt -d "Encrypt a file and output the result to stdout"
complete -F -c sops -n __fish_sops_no_subcommand -s r -l rotate -d "Generate new encryption key & reencrypt with the new key"
complete -F -c sops -n __fish_sops_no_subcommand -s i -l in-place -d "Write output back to the same file instead of stdout"
complete -F -c sops -n __fish_sops_no_subcommand -s s -l show-master-keys -d "Display master encryption keys in the file during editing"
complete -F -c sops -n __fish_sops_no_subcommand -l extract -d "Extract a specific key or branch from decrypted input document"
complete -F -c sops -n __fish_sops_no_subcommand -l ignore-mac -d "Ignore Message Authentication Code during decryption"
complete -F -c sops -n __fish_sops_no_subcommand -l verbose -d "Enable verbose logging output"
complete -F -c sops -n __fish_sops_no_subcommand -l enable-local-keyservice -d "Use local key service"
complete -F -c sops -n __fish_sops_no_subcommand -s v -l version -d "Print the version"

# Primary flags with required parameters
complete -x -c sops -n __fish_sops_no_subcommand -s k -l kms -d "Comma separated list of KMS ARNs"
complete -x -c sops -n __fish_sops_no_subcommand -s p -l pgp -d "Comma separated list of PGP fingerprints"
complete -x -c sops -n __fish_sops_no_subcommand -s a -l age -d "Comma separated list of age recipients"
complete -x -c sops -n __fish_sops_no_subcommand -l gcp-kms -d "Comma separated list of GCP KMS resource IDs"
complete -x -c sops -n __fish_sops_no_subcommand -l aws-profile -d "The AWS profile to use for requests to AWS"
complete -x -c sops -n __fish_sops_no_subcommand -l azure-kv -d "Comma separated list of Azure Key Vault URLs"
complete -x -c sops -n __fish_sops_no_subcommand -l hc-vault-transit -d "Comma separated list of Vault's key URI"
complete -x -c sops -n __fish_sops_no_subcommand -l input-type -d "Currently json, yaml, dotenv and binary are supported."
complete -x -c sops -n __fish_sops_no_subcommand -l output-type -d "Currently json, yaml, dotenv and binary are supported."
complete -x -c sops -n __fish_sops_no_subcommand -l add-gcp-kms -d "Comma-separated list of GCP KMS key resource IDs"
complete -x -c sops -n __fish_sops_no_subcommand -l rm-gcp-kms -d "Remove comma-separated list of GCP KMS key resource IDs"
complete -x -c sops -n __fish_sops_no_subcommand -l add-azure-kv -x -d "Add comma-separated list of Azure Key Vault key URLs"
complete -x -c sops -n __fish_sops_no_subcommand -l rm-azure-kv -x -d "Remove comma-separated list of Azure Key Vault key URLs"
complete -x -c sops -n __fish_sops_no_subcommand -l add-kms -x -d "Add comma-separated list of KMS ARNs"
complete -x -c sops -n __fish_sops_no_subcommand -l rm-kms -x -d "Remove comma-separated list of KMS ARNs"
complete -x -c sops -n __fish_sops_no_subcommand -l add-hc-vault-transit -x -d "Add comma-separated list of Vault's URI key"
complete -x -c sops -n __fish_sops_no_subcommand -l rm-hc-vault-transit -x -d "Remove comma-separated list of Vault's URI key"
complete -x -c sops -n __fish_sops_no_subcommand -l add-age -x -d "Add comma-separated list of age recipients fingerprints"
complete -x -c sops -n __fish_sops_no_subcommand -l rm-age -x -d "Remove comma-separated list of age recipients fingerprints"
complete -x -c sops -n __fish_sops_no_subcommand -l add-pgp -x -d "Add comma-separated list of PGP fingerprints"
complete -x -c sops -n __fish_sops_no_subcommand -l rm-pgp -x -d "Remove comma-separated list of PGP fingerprints"
complete -x -c sops -n __fish_sops_no_subcommand -l unencrypted-suffix -d "Override the unencrypted key suffix"
complete -x -c sops -n __fish_sops_no_subcommand -l encrypted-suffix -d "Override the encrypted key suffix"
complete -x -c sops -n __fish_sops_no_subcommand -l unencrypted-regex -d "Set the unencrypted key suffix"
complete -x -c sops -n __fish_sops_no_subcommand -l encrypted-regex -d "Set the encrypted key suffix"
complete -r -c sops -n __fish_sops_no_subcommand -l config -d "Path to sops' config file"
complete -x -c sops -n __fish_sops_no_subcommand -l encryption-context -d "Comma separated list of KMS encryption context key:value pairs"
complete -x -c sops -n __fish_sops_no_subcommand -l set -d "Set a specific key or branch in the input document (edit mode)"
complete -x -c sops -n __fish_sops_no_subcommand -l shamir-secret-sharing-threshold -x -d "Number of master keys required to retrieve the data key with shamir"
complete -r -c sops -n __fish_sops_no_subcommand -l output -d "Save the output after encryption or decryption to file"
complete -x -c sops -n __fish_sops_no_subcommand -l keyservice -d "Specify key services to use in addition to the local one"

# Global flags
complete -F -c sops -s h -l help -d "Show help"

# exec-env flags
complete -F -c sops -n "__fish_sops_commands exec-env" -l background -d "Background the process and don\'t wait for it to complete"
complete -x -c sops -n "__fish_sops_commands exec-env" -l user -a "(__fish_print_users)" -d "The user to run the command as"
complete -F -c sops -n "__fish_sops_commands exec-env" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands exec-env" -l keyservice -d "Specify key services to use in addition to the local one"

# exec-file flags
complete -F -c sops -n "__fish_sops_commands exec-file" -l background -d "Background the process and don\'t wait for it to complete"
complete -F -c sops -n "__fish_sops_commands exec-file" -l no-fifo -d "Use a regular file instead of a fifo to temporarily hold the decrypted contents"
complete -x -c sops -n "__fish_sops_commands exec-file" -l user -a "(__fish_print_users)" -d "The user to run the command as"
complete -x -c sops -n "__fish_sops_commands exec-file" -l input-type -a "json yaml dotenv binary" -d "Currently json, yaml, dotenv and binary are supported"
complete -x -c sops -n "__fish_sops_commands exec-file" -l output-type -a "json yaml dotenv binary" -d "Currently json, yaml, dotenv and binary are supported"
complete -F -c sops -n "__fish_sops_commands exec-file" -l filename -d "Filename for the temporarily file (default: tmp-file)"
complete -F -c sops -n "__fish_sops_commands exec-file" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands exec-file" -l keyservice -d "Specify key services to use in addition to the local one"

# publish flags
complete -F -c sops -n "__fish_sops_commands publish" -s y -l yes -d "Pre-approve all changes and run non-interactively"
complete -F -c sops -n "__fish_sops_commands publish" -l omit-extensions -d "Omit file extensions in destination path when publishing sops file"
complete -F -c sops -n "__fish_sops_commands publish" -l recursive -d "If source path is a directory, publish all its content recursively"
complete -F -c sops -n "__fish_sops_commands publish" -l verbose -d "Enable verbose logging output"
complete -F -c sops -n "__fish_sops_commands publish" -l enable-local-keyservice -d "use local key service"
complete -x -c sops -n "__fish_sops_commands publish" -l keyservice -d "Specify key services to use in addition to the local one"

# keyservice flags
complete -x -c sops -n "__fish_sops_commands keyservice" -l network -l net -a "tcp;unix" -d "Network to listen on (default: \"tcp\")"
complete -x -c sops -n "__fish_sops_commands keyservice" -l address -l addr -a "(__fish_print_addresses | cut -f1):" -d "Address to listen on (default: \"127.0.0.1:5000\")"
complete -F -c sops -n "__fish_sops_commands keyservice" -l prompt -d "Prompt user to confirm every incoming request"
complete -F -c sops -n "__fish_sops_commands keyservice" -l verbose -d "Enable verbose logging output"

# groups subcommands
complete -x -c sops -n "__fish_sops_commands groups; and __fish_prev_arg_in groups" -a add -d "Add a new group to a SOPS file"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_prev_arg_in groups" -a delete -d "Delete a key group from a SOPS file"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_prev_arg_in add" -a "(__fish_print_users)"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_prev_arg_in delete" -a "(__fish_print_users)"

# groups add flags
complete -F -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -s f -l file -d "The file to add the group to"
complete -F -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -s i -l in-place -d "Write output back to the same file instead of stdout"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l pgp -d "The PGP fingerprints the new group should contain"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l kms -d "The KMS ARNs the new group should contain"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l aws-profile -d "The AWS profile to use for requests to AWS"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l gcp-kms -d "The GCP KMS Resource ID the new group should contain"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l azure-kv -d "The Azure Key Vault key URL the new group should contain"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l hc-vault-transit -d "The full vault path to the key used to encrypt/decrypt"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l age -d "The age recipient the new group should contain. Can be specified more than once"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l shamir-secret-sharing-threshold -d "Number of master keys required to retrieve data key with shamir"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l encryption-context -d "Comma separated list of KMS encryption context key:value pairs"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from add" -l keyservice -d "Specify key services to use in addition to the local one"

# groups delete flags
complete -F -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from delete" -s f -l file -d "The file to add the group to"
complete -F -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from delete" -s i -l in-place -d "Write output back to the same file instead of stdout"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from delete" -l shamir-secret-sharing-threshold -d "Number of master keys required to retrieve data key with shamir"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from delete" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_seen_subcommand_from delete" -l keyservice -d "Specify key services to use in addition to the local one"

# updatekeys flags
complete -F -c sops -n "__fish_sops_commands updatekeys" -s y -l yes -d "Pre-approve all changes and run non-interactively"
complete -F -c sops -n "__fish_sops_commands updatekeys" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands updatekeys" -l keyservice -d "Specify key services to use in addition to the local one"

# help options
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a exec-env -d "Execute a command with decrypted values inserted into the environment"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a exec-file -d "Execute a command with decrypted contents as a temporary file"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a publish -d "Publish sops file or directory to a configured destination"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a keyservice -d "Start a SOPS key kervice server"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a groups -d "Modify the groups on a SOPS file"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a updatekeys -d "Update the keys of a SOPS file using the config file"
