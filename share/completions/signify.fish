# Tab completion for openbsd-signify

set -l subcommands -C -G -S -V
set -l subcommands_desc (echo -s \
    -C\t"Verify a signed checksum list."\n \
    -G\t"Generate a new key pair."\n \
    -S\t"Sign the specified message file."\n \
    -V\t"Verify the message and signature match."\n \
    | string escape)

complete -c signify -f

complete -c signify \
    -n "not __fish_seen_subcommand_from $subcommands" \
    -a "$subcommands_desc"

complete -c signify -F \
    -n '__fish_seen_subcommand_from -C'

complete -c signify -s c -f -r \
    -d 'Specify the comment to be added during key generation' \
    -n '__fish_seen_subcommand_from -G'

complete -c signify -s e -f \
    -d 'Use embedded signatures' \
    -n '__fish_seen_subcommand_from -S -V'

complete -c signify -s m -F -r \
    -d 'Message file to sign or verify' \
    -n '__fish_seen_subcommand_from -S -V'

# The -n option has two context-dependent meanings
complete -c signify -s n -f \
    -d 'When generating a key pair, do not ask for a passphrase' \
    -n '__fish_seen_argument -s G'
complete -c signify -s n -f \
    -d 'When signing with -z, store a zero timestamp in the gzip header' \
    -n '__fish_seen_subcommand_from -S && __fish_seen_argument -s z'

# This is deliberately split up to only add a description to the flag and not all its argument completions
complete -c signify -s p -f -k -r \
    -a '(__fish_complete_suffix .pub)' \
    -n '__fish_seen_subcommand_from -C -G -V'
complete -c signify -s p -f -k -r \
    -d 'Public key produced by -G, and used by -V to check a signature' \
    -n '__fish_seen_subcommand_from -C -G -V'

complete -c signify -s q -f \
    -d 'Quiet mode.  Suppress informational output' \
    -n '__fish_seen_subcommand_from -C -V'

complete -c signify -s s -f -k -r \
    -a '(__fish_complete_suffix .sec)' \
    -n '__fish_seen_subcommand_from -G -S'
complete -c signify -s s -f -k -r \
    -d 'Secret (private) key produced by -G, and used by -S to sign a message' \
    -n '__fish_seen_subcommand_from -G -S'

complete -c signify -s t -f -r \
    -a '(
        set -l files /etc/signify/*
        string replace -rf -- \'\\.pub$\' "" $files | string replace -r \'.*-\' ""
    )' \
    -n '__fish_seen_subcommand_from -C -V'
complete -c signify -s t -f -r \
    -d 'When inferring a key to verify with, only use keys with this keytype suffix' \
    -n '__fish_seen_subcommand_from -C -V'

complete -c signify -s x -f -k -r \
    -a '(__fish_complete_suffix .sig)' \
    -n '__fish_seen_subcommand_from -C -S -V'
complete -c signify -s x -f -k -r \
    -d 'The signature file to create or verify. The default is message.sig' \
    -n '__fish_seen_subcommand_from -C -S -V'

complete -c signify -s z -f \
    -d 'Sign and verify gzip (1) archives, embed signature in the header' \
    -n '__fish_seen_subcommand_from -S -V'
