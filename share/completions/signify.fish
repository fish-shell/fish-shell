# Tab completion for openbsd-signify

set -l subcommands -C -G -S -V
set -l subcommands_desc (echo -s \
    -C\t"Verify a signed checksum list."\n \
    -G\t"Generate a new key pair."\n \
    -S\t"Sign the specified message file."\n \
    -V\t"Verify the message and signature match."\n \
    | string escape)

complete -c signify --no-files

complete -c signify \
    --condition "not __fish_seen_subcommand_from $subcommands" \
    --arguments "$subcommands_desc"

complete -c signify --force-files \
    --condition '__fish_seen_subcommand_from -C'

complete -c signify --short-option c --no-files --require-parameter \
    --description 'Specify the comment to be added during key generation' \
    --condition '__fish_seen_subcommand_from -G'

complete -c signify --short-option e --no-files \
    --description 'Use embedded signatures' \
    --condition '__fish_seen_subcommand_from -S -V'

complete -c signify --short-option m --force-files --require-parameter \
    --description 'Message file to sign or verify' \
    --condition '__fish_seen_subcommand_from -S -V'

# The -n option has two context-dependent meanings
complete -c signify --short-option n --no-files \
    --description 'When generating a key pair, do not ask for a passphrase' \
    --condition '__fish_seen_argument -s G'
complete -c signify --short-option n --no-files \
    --description 'When signing with -z, store a zero timestamp in the gzip header' \
    --condition '__fish_seen_subcommand_from -S && __fish_seen_argument -s z'

# This is deliberately split up to only add a description to the flag and not all its argument completions
complete -c signify --short-option p --no-files --keep-order --require-parameter \
    --arguments '(__fish_complete_suffix .pub)' \
    --condition '__fish_seen_subcommand_from -C -G -V'
complete -c signify --short-option p --no-files --keep-order --require-parameter \
    --description 'Public key produced by -G, and used by -V to check a signature' \
    --condition '__fish_seen_subcommand_from -C -G -V'

complete -c signify --short-option q --no-files \
    --description 'Quiet mode.  Suppress informational output' \
    --condition '__fish_seen_subcommand_from -C -V'

complete -c signify --short-option s --no-files --keep-order --require-parameter \
    --arguments '(__fish_complete_suffix .sec)' \
    --condition '__fish_seen_subcommand_from -G -S'
complete -c signify --short-option s --no-files --keep-order --require-parameter \
    --description 'Secret (private) key produced by -G, and used by -S to sign a message' \
    --condition '__fish_seen_subcommand_from -G -S'

complete -c signify --short-option t --no-files --require-parameter \
    --arguments '(ls /etc/signify 2>/dev/null | string match -e \'*-*.pub\' | string replace -r \'\\.pub$\' "" | string replace -r \'.*-\' "")' \
    --condition '__fish_seen_subcommand_from -C -V'
complete -c signify --short-option t --no-files --require-parameter \
    --description 'When inferring a key to verify with, only use keys with this keytype suffix' \
    --condition '__fish_seen_subcommand_from -C -V'

complete -c signify --short-option x --no-files --keep-order --require-parameter \
    --arguments '(__fish_complete_suffix .sig)' \
    --condition '__fish_seen_subcommand_from -C -S -V'
complete -c signify --short-option x --no-files --keep-order --require-parameter \
    --description 'The signature file to create or verify. The default is message.sig' \
    --condition '__fish_seen_subcommand_from -C -S -V'

complete -c signify --short-option z --no-files \
    --description 'Sign and verify gzip (1) archives, embed signature in the header' \
    --condition '__fish_seen_subcommand_from -S -V'
