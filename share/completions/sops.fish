# Completion for SOPS (Secrets OPerationS)

function __fish_sops_commands --description "Test if argument(s) match a sops command"
  if not set -q argv[1]
    return 1
  end
  set -l cmd (commandline -cp)
  if set -q argv[2]
    if string match -qr -- "sops\s+$argv[1]\s+$argv[2]\s+\S*" $cmd
      return 0
    end
    return 1
  end
  if string match -qr -- "sops\s+$argv[1]\s+\S*" $cmd
    return 0
  end
  return 1
end

# Primary commands
complete -F -c sops -n __fish_is_first_arg -a exec-env -d "Execute a command with decrypted values inserted into the environment"
complete -F -c sops -n __fish_is_first_arg -a exec-file -d "Execute a command with decrypted contents as a temporary file"
complete -F -c sops -n __fish_is_first_arg -a publish -d "Publish sops file or directory to a configured destination"
complete -F -c sops -n __fish_is_first_arg -a keyservice -d "Start a SOPS key kervice server"
complete -F -c sops -n __fish_is_first_arg -a groups -d "Modify the groups on a SOPS file"
complete -F -c sops -n __fish_is_first_arg -a updatekeys -d "Update the keys of a SOPS file using the config file"
complete -F -c sops -n __fish_is_first_arg -a help -d "Shows a list of commands or help for one command"
complete -F -c sops -n __fish_is_first_arg -a h -d "Shows a list of commands or help for one command"

# Primary flags without parameters
complete -F -c sops -n __fish_is_first_token -s d -l decrypt -d "Decrypt a file and output the result to stdout"
complete -F -c sops -n __fish_is_first_token -s e -l encrypt -d "Encrypt a file and output the result to stdout"
complete -F -c sops -n __fish_is_first_token -s r -l rotate -d "Generate a new data encryption key and reencrypt all values with the new key"
complete -F -c sops -n __fish_is_first_token -s i -l in-place -d "Write output back to the same file instead of stdout"
complete -F -c sops -n __fish_is_first_token -s s -l show-master-keys -d "Display master encryption keys in the file during editing"
complete -F -c sops -n __fish_is_first_token -l extract -d "Extract a specific key or branch from the input document. Decrypt mode only. Example: --extract '[\"somekey\"][0]'"
complete -F -c sops -n __fish_is_first_token -l ignore-mac -d "Ignore Message Authentication Code during decryption"
complete -F -c sops -n __fish_is_first_token -l verbose -d "Enable verbose logging output"
complete -F -c sops -n __fish_is_first_token -l enable-local-keyservice -d "Use local key service"
complete -F -c sops -n __fish_is_first_token -s v -l version -d "Print the version"

# Primary flags with required parameters
complete -x -c sops -n __fish_is_first_token -s k -l kms -d "Comma separated list of KMS ARNs [\$SOPS_KMS_ARN]"
complete -x -c sops -n __fish_is_first_token -s p -l pgp -d "Comma separated list of PGP fingerprints [\$SOPS_PGP_FP]"
complete -x -c sops -n __fish_is_first_token -s a -l age -d "Comma separated list of age recipients [\$SOPS_AGE_RECIPIENTS]"
complete -x -c sops -n __fish_is_first_token -l gcp-kms -d "Comma separated list of GCP KMS resource IDs [\$SOPS_GCP_KMS_IDS]"
complete -x -c sops -n __fish_is_first_token -l aws-profile -d "The AWS profile to use for requests to AWS"
complete -x -c sops -n __fish_is_first_token -l azure-kv -d "Comma separated list of Azure Key Vault URLs [\$SOPS_AZURE_KEYVAULT_URLS]"
complete -x -c sops -n __fish_is_first_token -l hc-vault-transit -d "Comma separated list of Vault's key URI (e.g. 'https://vault.example.org:8200/v1/transit/keys/dev') [\$SOPS_VAULT_URIS]"
complete -x -c sops -n __fish_is_first_token -l input-type -d "Currently json, yaml, dotenv and binary are supported. If not set, sop will use the file's extension to determine the type"
complete -x -c sops -n __fish_is_first_token -l output-type -d "Currently json, yaml, dotenv and binary are supported. If not set, sop will use the input file's extension to determine the output format"
complete -x -c sops -n __fish_is_first_token -l add-gcp-kms -d "Add the provided comma-separated list of GCP KMS key resource IDs to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l rm-gcp-kms -d "Remove the provided comma-separated list of GCP KMS key resource IDs to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l add-azure-kv -x -d "Add the provided comma-separated list of Azure Key Vault key URLs to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l rm-azure-kv -x -d "Remove the provided comma-separated list of Azure Key Vault key URLs to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l add-kms -x -d "Add the provided comma-separated list of KMS ARNs to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l rm-kms -x -d "Remove the provided comma-separated list of KMS ARNs to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l add-hc-vault-transit -x -d "Add the provided comma-separated list of Vault's URI key to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l rm-hc-vault-transit -x -d "Remove the provided comma-separated list of Vault's URI key to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l add-age -x -d "Add the provided comma-separated list of age recipients fingerprints to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l rm-age -x -d "Remove the provided comma-separated list of age recipients fingerprints to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l add-pgp -x -d "Add the provided comma-separated list of PGP fingerprints to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l rm-pgp -x -d "Remove the provided comma-separated list of PGP fingerprints to the list of master keys on the given file"
complete -x -c sops -n __fish_is_first_token -l unencrypted-suffix -d "Override the unencrypted key suffix"
complete -x -c sops -n __fish_is_first_token -l encrypted-suffix -d "Override the encrypted key suffix. When empty, all keys will be encrypted, unless otherwise marked with unencrypted-suffix"
complete -x -c sops -n __fish_is_first_token -l unencrypted-regex -d "Set the unencrypted key suffix. When specified, only keys matching the regex will be left unencrypted"
complete -x -c sops -n __fish_is_first_token -l encrypted-regex -d "Set the encrypted key suffix. When specified, only keys matching the regex will be encrypted"
complete -r -c sops -n __fish_is_first_token -l config -d "Path to sops' config file. If set, sops will not search for the config file recursively"
complete -x -c sops -n __fish_is_first_token -l encryption-context -d "Comma separated list of KMS encryption context key:value pairs"
complete -x -c sops -n __fish_is_first_token -l set -d "Set a specific key or branch in the input document. Value must be a json encoded string. (edit mode only). eg. --set '[\"somekey\"][0] {\"somevalue\":true}'"
complete -x -c sops -n __fish_is_first_token -l shamir-secret-sharing-threshold -x -d "The number of master keys required to retrieve the data key with shamir (default: 0)"
complete -r -c sops -n __fish_is_first_token -l output -d "Save the output after encryption or decryption to the file specified"
complete -x -c sops -n __fish_is_first_token -l keyservice -d "Specify the key services to use in addition to the local one. Can be specified more than once. Syntax: protocol://address. Example: tcp://myserver.com:5000"

# Global flags
complete -F -c sops -s h -l help -d "Show help"

# exec-env flags
complete -F -c sops -n "__fish_sops_commands exec-env" -l background -d "Background the process and don\'t wait for it to complete"
complete -x -c sops -n "__fish_sops_commands exec-env" -l user -a "(__fish_print_users)" -d "The user to run the command as"
complete -F -c sops -n "__fish_sops_commands exec-env" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands exec-env" -l keyservice -d "Specify the key services to use in addition to the local one. Can be specified more than once. Syntax: protocol://address. Example: tcp://myserver.com:5000"

# exec-file flags
complete -F -c sops -n "__fish_sops_commands exec-file" -l background -d "Background the process and don\'t wait for it to complete"
complete -F -c sops -n "__fish_sops_commands exec-file" -l no-fifo -d "Use a regular file instead of a fifo to temporarily hold the decrypted contents"
complete -x -c sops -n "__fish_sops_commands exec-file" -l user -a "(__fish_print_users)" -d "The user to run the command as"
complete -x -c sops -n "__fish_sops_commands exec-file" -l input-type -a "json;yaml;dotenv;binary" -d "Currently json, yaml, dotenv and binary are supported. If not set, sops will use the file's extension to determine the output format"
complete -x -c sops -n "__fish_sops_commands exec-file" -l output-type -a "json;yaml;dotenv;binary" -d "Currently json, yaml, dotenv and binary are supported. If not set, sops will use the input file's extension to determine the output format"
complete -F -c sops -n "__fish_sops_commands exec-file" -l filename -d "Filename for the temporarily file (default: tmp-file)"
complete -F -c sops -n "__fish_sops_commands exec-file" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands exec-file" -l keyservice -d "Specify the key services to use in addition to the local one. Can be specified more than once. Syntax: protocol://address. Example: tcp://myserver.com:5000"

# publish flags
complete -F -c sops -n "__fish_sops_commands publish" -s y -l yes -d "Pre-approve all changes and run non-interactively"
complete -F -c sops -n "__fish_sops_commands publish" -l omit-extentions -d "Omit file extensions in destination path when publishing sops file to configured destination"
complete -F -c sops -n "__fish_sops_commands publish" -l recursive -d "If the source path is a directory, publish all its content recursively"
complete -F -c sops -n "__fish_sops_commands publish" -l verbose -d "Enable verbose logging output"
complete -F -c sops -n "__fish_sops_commands publish" -l enable-local-keyservice -d "use local key service"
complete -x -c sops -n "__fish_sops_commands publish" -l keyservice -d "Specify the key services to use in addition to the local one. Can be specified more than once. Syntax: protocol://address. Example: tcp://myserver.com:5000"

# keyservice flags
complete -x -c sops -n "__fish_sops_commands keyservice" -l network -l net -a "tcp;unix" -d "Network to listen on, e.g. 'tcp' or 'unix' (default: \"tcp\")"
complete -x -c sops -n "__fish_sops_commands keyservice" -l address -l addr -a "(__fish_print_addresses | cut -f1):" -d "Address to listen on, e.g. '127.0.0.1:5000' or '/tmp/sops.sock' (default: \"127.0.0.1:5000\")"
complete -F -c sops -n "__fish_sops_commands keyservice" -l prompt -d "Prompt user to confirm every incoming request"
complete -F -c sops -n "__fish_sops_commands keyservice" -l verbose -d "Enable verbose logging output"

# groups subcommands
complete -x -c sops -n "__fish_sops_commands groups; and __fish_prev_arg_in groups" -a add -d "Add a new group to a SOPS file"
complete -x -c sops -n "__fish_sops_commands groups; and __fish_prev_arg_in groups" -a delete -d "Delete a key group from a SOPS file"
complete -x -c sops -n "__fish_sops_commands groups add; and __fish_prev_arg_in add" -a "(__fish_print_users)"
complete -x -c sops -n "__fish_sops_commands groups delete; and __fish_prev_arg_in delete" -a "(__fish_print_users)"

# groups add flags
complete -F -c sops -n "__fish_sops_commands groups add" -s f -l file -d "The file to add the group to"
complete -F -c sops -n "__fish_sops_commands groups add" -s i -l in-place -d "Write output back to the same file instead of stdout"
complete -x -c sops -n "__fish_sops_commands groups add" -l pgp -d "The PGP fingerprints the new group should contain. Can be specified more than once"
complete -x -c sops -n "__fish_sops_commands groups add" -l kms -d "The KMS ARNs the new group should contain. Can be specified more than once"
complete -x -c sops -n "__fish_sops_commands groups add" -l aws-profile -d "The AWS profile to use for requests to AWS"
complete -x -c sops -n "__fish_sops_commands groups add" -l gcp-kms -d "The GCP KMS Resource ID the new group should contain. Can be specified more than once"
complete -x -c sops -n "__fish_sops_commands groups add" -l azure-kv -d "The Azure Key Vault key URL the new group should contain. Can be specified more than once"
complete -x -c sops -n "__fish_sops_commands groups add" -l hc-vault-transit -d "The full vault path to the key used to encrypt/decrypt. Make you choose and configure a key with encrption/decryption enabled (e.g. 'https://vault.example.org:8200/v1/transit/keys/dev'). Can be specified more than once"
complete -x -c sops -n "__fish_sops_commands groups add" -l age -d "The age recipient the new group should contain. Can be specified more than once"
complete -x -c sops -n "__fish_sops_commands groups add" -l shamir-secret-sharing-threshold -d "The number of master keys required to retrieve the data key with shamir (default: 0)"
complete -x -c sops -n "__fish_sops_commands groups add" -l encryption-context -d "Comma separated list of KMS encryption context key:value pairs"
complete -x -c sops -n "__fish_sops_commands groups add" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands groups add" -l keyservice -d "Specify the key services to use in addition to the local one. Can be specified more than once. Syntax: protocol://address. Example: tcp://myserver.com:5000"

# groups delete flags
complete -F -c sops -n "__fish_sops_commands groups delete" -s f -l file -d "The file to add the group to"
complete -F -c sops -n "__fish_sops_commands groups delete" -s i -l in-place -d "Write output back to the same file instead of stdout"
complete -x -c sops -n "__fish_sops_commands groups delete" -l shamir-secret-sharing-threshold -d "The number of master keys required to retrieve the data key with shamir (default: 0)"
complete -x -c sops -n "__fish_sops_commands groups delete" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands groups delete" -l keyservice -d "Specify the key services to use in addition to the local one. Can be specified more than once. Syntax: protocol://address. Example: tcp://myserver.com:5000"

# updatekeys flags
complete -F -c sops -n "__fish_sops_commands updatekeys" -s y -l yes -d "Pre-approve all changes and run non-interactively"
complete -F -c sops -n "__fish_sops_commands updatekeys" -l enable-local-keyservice -d "Use local key service"
complete -x -c sops -n "__fish_sops_commands updatekeys" -l keyservice -d "Specify the key services to use in addition to the local one. Can be specified more than once. Syntax: protocol://address. Example: tcp://myserver.com:5000"

# help options
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a exec-env -d "Execute a command with decrypted values inserted into the environment"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a exec-file -d "Execute a command with decrypted contents as a temporary file"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a publish -d "Publish sops file or directory to a configured destination"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a keyservice -d "Start a SOPS key kervice server"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a groups -d "Modify the groups on a SOPS file"
complete -x -c sops -n "__fish_sops_commands help; or __fish_sops_commands h" -a updatekeys -d "Update the keys of a SOPS file using the config file"
