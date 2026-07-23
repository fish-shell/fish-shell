# security - command line interface to keychains and Security framework (man 1 security)
#
# security has a true subcommand grammar: `security [-hilqv] [-p prompt] <command>
# [command_options] [command_args]`. The first non-option token is the command;
# its options and operands follow. Keychain operands are enumerated live from the
# fast, unprivileged `security list-keychains`.

# ── live enumerators ─────────────────────────────────────────────────
# Keychains in the current search list. `list-keychains` prints quoted,
# indented paths; strip the quotes and surrounding whitespace.
function __fish_security_keychains
    security list-keychains 2>/dev/null \
        | string trim \
        | string trim -c '"' \
        | string match -v ''
end

# ── top-level options (before any subcommand) ────────────────────────
complete -c security -n __fish_use_subcommand -s h -d 'Show list of all commands, or usage for a command'
complete -c security -n __fish_use_subcommand -s i -d 'Run security in interactive mode'
complete -c security -n __fish_use_subcommand -s l -d 'Run /usr/bin/leaks on itself before exiting'
complete -c security -n __fish_use_subcommand -s p -x -d 'Set the interactive prompt (implies -i)'
complete -c security -n __fish_use_subcommand -s q -d 'Make security less verbose'
complete -c security -n __fish_use_subcommand -s v -d 'Make security more verbose'

# ── subcommands ──────────────────────────────────────────────────────
complete -c security -f -n __fish_use_subcommand -a help -d 'Show all commands, or usage for a command'
complete -c security -f -n __fish_use_subcommand -a list-keychains -d 'Display or manipulate the keychain search list'
complete -c security -f -n __fish_use_subcommand -a default-keychain -d 'Display or set the default keychain'
complete -c security -f -n __fish_use_subcommand -a login-keychain -d 'Display or set the login keychain'
complete -c security -f -n __fish_use_subcommand -a create-keychain -d 'Create keychains'
complete -c security -f -n __fish_use_subcommand -a delete-keychain -d 'Delete keychains and remove from the search list'
complete -c security -f -n __fish_use_subcommand -a lock-keychain -d 'Lock the specified keychain'
complete -c security -f -n __fish_use_subcommand -a unlock-keychain -d 'Unlock the specified keychain'
complete -c security -f -n __fish_use_subcommand -a set-keychain-settings -d 'Set settings for a keychain'
complete -c security -f -n __fish_use_subcommand -a set-keychain-password -d 'Set password for a keychain'
complete -c security -f -n __fish_use_subcommand -a show-keychain-info -d 'Show the settings for a keychain'
complete -c security -f -n __fish_use_subcommand -a dump-keychain -d 'Dump the contents of one or more keychains'
complete -c security -f -n __fish_use_subcommand -a create-keypair -d 'Create an asymmetric key pair'
complete -c security -f -n __fish_use_subcommand -a add-generic-password -d 'Add a generic password item'
complete -c security -f -n __fish_use_subcommand -a add-internet-password -d 'Add an internet password item'
complete -c security -f -n __fish_use_subcommand -a add-certificates -d 'Add certificates to a keychain'
complete -c security -f -n __fish_use_subcommand -a find-generic-password -d 'Find a generic password item'
complete -c security -f -n __fish_use_subcommand -a delete-generic-password -d 'Delete a generic password item'
complete -c security -f -n __fish_use_subcommand -a set-generic-password-partition-list -d 'Set the partition list of a generic password item'
complete -c security -f -n __fish_use_subcommand -a find-internet-password -d 'Find an internet password item'
complete -c security -f -n __fish_use_subcommand -a delete-internet-password -d 'Delete an internet password item'
complete -c security -f -n __fish_use_subcommand -a set-internet-password-partition-list -d 'Set the partition list of an internet password item'
complete -c security -f -n __fish_use_subcommand -a find-key -d 'Find keys in the keychain'
complete -c security -f -n __fish_use_subcommand -a set-key-partition-list -d 'Set the partition list of a key'
complete -c security -f -n __fish_use_subcommand -a find-certificate -d 'Find a certificate item'
complete -c security -f -n __fish_use_subcommand -a find-identity -d 'Find an identity (certificate + private key)'
complete -c security -f -n __fish_use_subcommand -a delete-certificate -d 'Delete a certificate from a keychain'
complete -c security -f -n __fish_use_subcommand -a delete-identity -d 'Delete a certificate and its private key from a keychain'
complete -c security -f -n __fish_use_subcommand -a set-identity-preference -d 'Set the preferred identity to use for a service'
complete -c security -f -n __fish_use_subcommand -a get-identity-preference -d 'Get the preferred identity to use for a service'
complete -c security -f -n __fish_use_subcommand -a create-db -d 'Create a db using the DL'
complete -c security -n __fish_use_subcommand -a export -d 'Export items from a keychain'
complete -c security -n __fish_use_subcommand -a import -d 'Import items into a keychain'
complete -c security -f -n __fish_use_subcommand -a cms -d 'Encode or decode CMS messages'
complete -c security -f -n __fish_use_subcommand -a install-mds -d 'Install (or re-install) the MDS database'
complete -c security -n __fish_use_subcommand -a add-trusted-cert -d 'Add trusted certificate(s)'
complete -c security -n __fish_use_subcommand -a remove-trusted-cert -d 'Remove trusted certificate(s)'
complete -c security -f -n __fish_use_subcommand -a dump-trust-settings -d 'Display contents of trust settings'
complete -c security -f -n __fish_use_subcommand -a user-trust-settings-enable -d 'Display or manipulate user-level trust settings'
complete -c security -n __fish_use_subcommand -a trust-settings-export -d 'Export trust settings'
complete -c security -n __fish_use_subcommand -a trust-settings-import -d 'Import trust settings'
complete -c security -n __fish_use_subcommand -a verify-cert -d 'Verify certificate(s)'
complete -c security -f -n __fish_use_subcommand -a authorize -d 'Perform authorization operations'
complete -c security -f -n __fish_use_subcommand -a authorizationdb -d 'Make changes to the authorization policy database'
complete -c security -n __fish_use_subcommand -a execute-with-privileges -d 'Execute tool with privileges'
complete -c security -f -n __fish_use_subcommand -a leaks -d 'Run /usr/bin/leaks on this process'
complete -c security -f -n __fish_use_subcommand -a smartcards -d 'Enable, disable or list disabled smartcard tokens'
complete -c security -f -n __fish_use_subcommand -a list-smartcards -d 'Display available smartcards'
complete -c security -n __fish_use_subcommand -a export-smartcard -d 'Export/display items from a smartcard'
complete -c security -f -n __fish_use_subcommand -a error -d 'Display a descriptive message for the given error code(s)'
complete -c security -f -n __fish_use_subcommand -a requirement-evaluate -d 'Evaluate a code requirement against a certificate chain'

# ── help ─────────────────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from help' -a '(security help 2>/dev/null | string match -r "^\s+\S+" | string trim)'

# ── list-keychains / default-keychain / login-keychain ───────────────
complete -c security -f -n '__fish_seen_subcommand_from list-keychains default-keychain login-keychain' -s d -x -a 'user system common dynamic' -d 'Use the specified preference domain'
complete -c security -n '__fish_seen_subcommand_from list-keychains default-keychain login-keychain' -s s -d 'Set the search list / keychain'
complete -c security -n '__fish_seen_subcommand_from list-keychains default-keychain login-keychain delete-keychain' -a '(__fish_security_keychains)' -d Keychain

# ── create-keychain ──────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from create-keychain' -s P -d 'Prompt for a password using the SecurityAgent'
complete -c security -f -n '__fish_seen_subcommand_from create-keychain' -s p -x -d 'Use the given password for the keychains (insecure)'

# ── lock-keychain ────────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from lock-keychain' -s a -d 'Lock all keychains'
complete -c security -n '__fish_seen_subcommand_from lock-keychain unlock-keychain' -a '(__fish_security_keychains)' -d Keychain

# ── unlock-keychain ──────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from unlock-keychain' -s u -d 'Do not use the supplied password to unlock'
complete -c security -f -n '__fish_seen_subcommand_from unlock-keychain' -s p -x -d 'Use the given password to unlock'

# ── set-keychain-settings ────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from set-keychain-settings' -s l -d 'Lock keychain when the system sleeps'
complete -c security -f -n '__fish_seen_subcommand_from set-keychain-settings' -s u -d 'Lock keychain after timeout interval'
complete -c security -f -n '__fish_seen_subcommand_from set-keychain-settings' -s t -x -d 'Specify timeout interval in seconds'
complete -c security -n '__fish_seen_subcommand_from set-keychain-settings set-keychain-password show-keychain-info' -a '(__fish_security_keychains)' -d Keychain

# ── set-keychain-password ────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from set-keychain-password' -s o -x -d 'Old keychain password'
complete -c security -f -n '__fish_seen_subcommand_from set-keychain-password' -s p -x -d 'New keychain password'

# ── dump-keychain ────────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from dump-keychain' -s a -d 'Dump access control list of items'
complete -c security -f -n '__fish_seen_subcommand_from dump-keychain' -s d -d 'Dump (decrypted) data of items'
complete -c security -f -n '__fish_seen_subcommand_from dump-keychain' -s i -d 'Interactive access control list editing mode'
complete -c security -f -n '__fish_seen_subcommand_from dump-keychain' -s r -d 'Dump raw (encrypted) data of items'
complete -c security -n '__fish_seen_subcommand_from dump-keychain' -a '(__fish_security_keychains)' -d Keychain

# ── create-keypair ───────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from create-keypair' -s a -x -a 'rsa dh dsa fee' -d 'Algorithm to use (default rsa)'
complete -c security -f -n '__fish_seen_subcommand_from create-keypair' -s s -x -d 'Key size in bits (default 512)'
complete -c security -f -n '__fish_seen_subcommand_from create-keypair' -s f -x -d 'Make key valid from the specified date'
complete -c security -f -n '__fish_seen_subcommand_from create-keypair' -s t -x -d 'Make key valid to the specified date'
complete -c security -f -n '__fish_seen_subcommand_from create-keypair' -s d -x -d 'Make key valid for the number of days from today'
complete -c security -n '__fish_seen_subcommand_from create-keypair' -s k -x -a '(__fish_security_keychains)' -d 'Use the specified keychain'
complete -c security -f -n '__fish_seen_subcommand_from create-keypair' -s A -d 'Allow any application to access this key (insecure)'
complete -c security -n '__fish_seen_subcommand_from create-keypair' -s T -d 'Specify an application which may access this key'

# ── add-generic-password / add-internet-password (shared flags) ──────
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s a -x -d 'Specify account name (required)'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s c -x -d 'Specify item creator (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s C -x -d 'Specify item type (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s D -x -d 'Specify kind (default "application password")'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s j -x -d 'Specify comment string'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s l -x -d 'Specify label'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s w -x -d 'Specify password to be added'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s A -d 'Allow any application to access this item (insecure)'
complete -c security -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s T -d 'Specify an application which may access this item'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s U -d 'Update item if it already exists'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password add-internet-password' -s X -x -d 'Specify password data as a hexadecimal string'
# add-generic-password only
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password' -s G -x -d 'Specify generic attribute value'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password' -s s -x -d 'Specify service name (required)'
complete -c security -f -n '__fish_seen_subcommand_from add-generic-password' -s p -x -d 'Specify password to be added (legacy, = -w)'
# add-internet-password only
complete -c security -f -n '__fish_seen_subcommand_from add-internet-password' -s d -x -d 'Specify security domain string'
complete -c security -f -n '__fish_seen_subcommand_from add-internet-password' -s p -x -d 'Specify path string'
complete -c security -f -n '__fish_seen_subcommand_from add-internet-password' -s P -x -d 'Specify port number'
complete -c security -f -n '__fish_seen_subcommand_from add-internet-password' -s r -x -d 'Specify protocol (four-character SecProtocolType)'
complete -c security -f -n '__fish_seen_subcommand_from add-internet-password' -s s -x -d 'Specify server name (required)'
complete -c security -f -n '__fish_seen_subcommand_from add-internet-password' -s t -x -d 'Specify authentication type (four-character)'

# ── add-certificates ─────────────────────────────────────────────────
complete -c security -n '__fish_seen_subcommand_from add-certificates' -s k -x -a '(__fish_security_keychains)' -d 'Use the specified keychain'

# ── find-generic-password / delete-generic-password (shared) ─────────
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -s a -x -d 'Match account string'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -s c -x -d 'Match creator (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -s C -x -d 'Match type (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -s D -x -d 'Match kind string'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -s G -x -d 'Match value string (generic attribute)'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -s j -x -d 'Match comment string'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -s l -x -d 'Match label string'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -s s -x -d 'Match service string'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password' -s g -d 'Display the password for the item found'
complete -c security -f -n '__fish_seen_subcommand_from find-generic-password' -s w -d 'Display only the password for the item found'
complete -c security -n '__fish_seen_subcommand_from find-generic-password delete-generic-password' -a '(__fish_security_keychains)' -d Keychain

# ── find-internet-password / delete-internet-password (shared) ───────
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s a -x -d 'Match account string'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s c -x -d 'Match creator (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s C -x -d 'Match type (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s d -x -d 'Match securityDomain string'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s D -x -d 'Match kind string'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s j -x -d 'Match comment string'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s l -x -d 'Match label string'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s p -x -d 'Match path string'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s P -x -d 'Match port number'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s r -x -d 'Match protocol (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s s -x -d 'Match server string'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -s t -x -d 'Match authenticationType (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password' -s g -d 'Display the password for the item found'
complete -c security -f -n '__fish_seen_subcommand_from find-internet-password' -s w -d 'Display only the password for the item found'
complete -c security -n '__fish_seen_subcommand_from find-internet-password delete-internet-password' -a '(__fish_security_keychains)' -d Keychain

# ── find-key ─────────────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s a -x -d 'Match application label string'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s c -x -d 'Match creator (four-character code)'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s d -d 'Match keys that can decrypt'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s D -x -d 'Match description string'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s e -d 'Match keys that can encrypt'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s j -x -d 'Match comment string'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s l -x -d 'Match label string'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s r -d 'Match keys that can derive'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s s -d 'Match keys that can sign'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s t -x -a 'symmetric public private' -d 'Type of key to find'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s u -d 'Match keys that can unwrap'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s v -d 'Match keys that can verify'
complete -c security -f -n '__fish_seen_subcommand_from find-key' -s w -d 'Match keys that can wrap'
complete -c security -n '__fish_seen_subcommand_from find-key' -a '(__fish_security_keychains)' -d Keychain

# ── partition-list setters (shared match flags) ──────────────────────
complete -c security -f -n '__fish_seen_subcommand_from set-generic-password-partition-list set-internet-password-partition-list' -s S -x -d 'Comma-separated partition list'
complete -c security -f -n '__fish_seen_subcommand_from set-generic-password-partition-list set-internet-password-partition-list' -s k -x -d 'Password for keychain (deprecated)'
complete -c security -f -n '__fish_seen_subcommand_from set-generic-password-partition-list set-internet-password-partition-list' -s a -x -d 'Match account string'
complete -c security -f -n '__fish_seen_subcommand_from set-generic-password-partition-list set-internet-password-partition-list' -s l -x -d 'Match label string'
complete -c security -f -n '__fish_seen_subcommand_from set-generic-password-partition-list' -s s -x -d 'Match service string'
complete -c security -f -n '__fish_seen_subcommand_from set-internet-password-partition-list' -s s -x -d 'Match server string'
complete -c security -n '__fish_seen_subcommand_from set-generic-password-partition-list set-internet-password-partition-list set-key-partition-list' -a '(__fish_security_keychains)' -d Keychain

# ── find-certificate ─────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from find-certificate' -s a -d 'Find all matching certificates, not just the first'
complete -c security -f -n '__fish_seen_subcommand_from find-certificate' -s c -x -d 'Match on name when searching'
complete -c security -f -n '__fish_seen_subcommand_from find-certificate' -s e -x -d 'Match on emailAddress when searching'
complete -c security -f -n '__fish_seen_subcommand_from find-certificate' -s m -d 'Show the email addresses in the certificate'
complete -c security -f -n '__fish_seen_subcommand_from find-certificate' -s p -d 'Output certificate in PEM format'
complete -c security -f -n '__fish_seen_subcommand_from find-certificate' -s Z -d 'Print SHA-256 (and SHA-1) hash of the certificate'
complete -c security -n '__fish_seen_subcommand_from find-certificate' -a '(__fish_security_keychains)' -d Keychain

# ── find-identity ────────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from find-identity' -s p -x -a 'basic ssl-client ssl-server smime eap ipsec ichat codesigning sys-default sys-kerberos-kdc' -d 'Specify policy'
complete -c security -f -n '__fish_seen_subcommand_from find-identity' -s s -x -d 'Specify policy string'
complete -c security -f -n '__fish_seen_subcommand_from find-identity' -s v -d 'Show valid identities only'
complete -c security -n '__fish_seen_subcommand_from find-identity' -a '(__fish_security_keychains)' -d Keychain

# ── delete-certificate / delete-identity ─────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from delete-certificate delete-identity' -s c -x -d 'Specify certificate to delete by its common name'
complete -c security -f -n '__fish_seen_subcommand_from delete-certificate delete-identity' -s Z -x -d 'Specify certificate to delete by its SHA-256/SHA-1 hash'
complete -c security -f -n '__fish_seen_subcommand_from delete-certificate delete-identity' -s t -d 'Also delete user trust settings for this certificate'
complete -c security -n '__fish_seen_subcommand_from delete-certificate delete-identity' -a '(__fish_security_keychains)' -d Keychain

# ── set-identity-preference / get-identity-preference ────────────────
complete -c security -f -n '__fish_seen_subcommand_from set-identity-preference get-identity-preference' -s s -x -d 'Specify service'
complete -c security -f -n '__fish_seen_subcommand_from set-identity-preference get-identity-preference' -s u -x -d 'Specify key usage'
complete -c security -f -n '__fish_seen_subcommand_from set-identity-preference' -s n -d 'No identity; delete the preference'
complete -c security -f -n '__fish_seen_subcommand_from set-identity-preference' -s c -x -d 'Specify certificate by common name'
complete -c security -f -n '__fish_seen_subcommand_from set-identity-preference' -s Z -x -d 'Specify certificate by SHA-1 hash'
complete -c security -f -n '__fish_seen_subcommand_from get-identity-preference' -s p -d 'Output identity certificate in PEM format'
complete -c security -f -n '__fish_seen_subcommand_from get-identity-preference' -s c -d 'Print common name of the preferred identity'
complete -c security -f -n '__fish_seen_subcommand_from get-identity-preference' -s Z -d 'Print SHA-1 hash of the preferred identity'

# ── export ───────────────────────────────────────────────────────────
complete -c security -n '__fish_seen_subcommand_from export' -s k -x -a '(__fish_security_keychains)' -d 'Keychain to export from'
complete -c security -f -n '__fish_seen_subcommand_from export' -s t -x -a 'certs allKeys pubKeys privKeys identities all' -d 'Type of items to export'
complete -c security -f -n '__fish_seen_subcommand_from export' -s f -x -a 'openssl bsafe pkcs7 pkcs8 pkcs12 x509 openssh1 openssh2 pemseq' -d 'Format of the exported data'
complete -c security -f -n '__fish_seen_subcommand_from export' -s w -d 'Wrap private keys on export'
complete -c security -f -n '__fish_seen_subcommand_from export' -s p -d 'Apply PEM armour to the output data'
complete -c security -f -n '__fish_seen_subcommand_from export' -s P -x -d 'Specify the wrapping passphrase'
complete -c security -n '__fish_seen_subcommand_from export' -s o -F -d 'Write the output data to outfile'

# ── import ───────────────────────────────────────────────────────────
complete -c security -n '__fish_seen_subcommand_from import' -s k -x -a '(__fish_security_keychains)' -d 'Keychain to import into'
complete -c security -f -n '__fish_seen_subcommand_from import' -s t -x -a 'cert pub priv session agg' -d 'Type of items to import'
complete -c security -f -n '__fish_seen_subcommand_from import' -s f -x -a 'openssl bsafe raw pkcs7 pkcs8 pkcs12 x509 openssh1 openssh2 pemseq' -d 'Format of the imported data'
complete -c security -f -n '__fish_seen_subcommand_from import' -s w -d 'Private keys are wrapped and must be unwrapped on import'
complete -c security -f -n '__fish_seen_subcommand_from import' -s x -d 'Private keys are non-extractable after import'
complete -c security -f -n '__fish_seen_subcommand_from import' -s P -x -d 'Specify the unwrapping passphrase'
complete -c security -f -n '__fish_seen_subcommand_from import' -s a -x -d 'Specify optional extended attribute name and value'
complete -c security -f -n '__fish_seen_subcommand_from import' -s A -d 'Allow any application to access the imported key (insecure)'
complete -c security -n '__fish_seen_subcommand_from import' -s T -d 'Specify an application which may access the imported key'

# ── cms ──────────────────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from cms' -s C -d 'Create a CMS encrypted message'
complete -c security -f -n '__fish_seen_subcommand_from cms' -s D -d 'Decode a CMS message'
complete -c security -f -n '__fish_seen_subcommand_from cms' -s E -d 'Create a CMS enveloped message'
complete -c security -f -n '__fish_seen_subcommand_from cms' -s S -d 'Create a CMS signed message'

# ── add-trusted-cert / remove-trusted-cert ───────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from add-trusted-cert' -s d -d 'Add to admin cert store (default is user)'
complete -c security -f -n '__fish_seen_subcommand_from add-trusted-cert' -s r -x -a 'trustRoot trustAsRoot deny unspecified' -d 'Result type (default trustRoot)'
complete -c security -f -n '__fish_seen_subcommand_from add-trusted-cert' -s p -x -a 'ssl smime codeSign IPSec basic swUpdate pkgSign eap macappstore appleID timestamping' -d 'Specify policy constraint'
complete -c security -n '__fish_seen_subcommand_from add-trusted-cert' -s a -d 'Specify application constraint'
complete -c security -f -n '__fish_seen_subcommand_from add-trusted-cert' -s s -x -d 'Specify policy-specific string'
complete -c security -f -n '__fish_seen_subcommand_from add-trusted-cert' -s e -x -d 'Specify allowed error'
complete -c security -f -n '__fish_seen_subcommand_from add-trusted-cert' -s u -x -d 'Specify key usage (an integer)'
complete -c security -n '__fish_seen_subcommand_from add-trusted-cert' -s k -x -a '(__fish_security_keychains)' -d 'Keychain to which cert is added'
complete -c security -n '__fish_seen_subcommand_from add-trusted-cert' -s i -F -d 'Input trust settings file'
complete -c security -n '__fish_seen_subcommand_from add-trusted-cert' -s o -F -d 'Output trust settings file'
complete -c security -f -n '__fish_seen_subcommand_from remove-trusted-cert' -s d -d 'Remove from admin cert store (default is user)'

# ── trust-settings-export / trust-settings-import ────────────────────
complete -c security -f -n '__fish_seen_subcommand_from trust-settings-export' -s s -d 'Export system trust settings'
complete -c security -f -n '__fish_seen_subcommand_from trust-settings-export trust-settings-import' -s d -d 'Operate on admin (system) trust settings'

# ── verify-cert ──────────────────────────────────────────────────────
complete -c security -n '__fish_seen_subcommand_from verify-cert' -s c -F -d 'Certificate to verify (DER or PEM)'
complete -c security -n '__fish_seen_subcommand_from verify-cert' -s r -F -d 'Root certificate (DER or PEM)'
complete -c security -f -n '__fish_seen_subcommand_from verify-cert' -s p -x -a 'ssl smime codeSign IPSec basic swUpdate pkgSign eap appleID macappstore timestamping' -d 'Verification policy (default basic)'
complete -c security -f -n '__fish_seen_subcommand_from verify-cert' -s C -d 'Evaluation is for client usage'
complete -c security -f -n '__fish_seen_subcommand_from verify-cert' -s d -x -d 'Date to set for verification (YYYY-MM-DD-hh:mm:ss)'
complete -c security -n '__fish_seen_subcommand_from verify-cert' -s k -x -a '(__fish_security_keychains)' -d 'Keychain to search for intermediate CAs'
complete -c security -f -n '__fish_seen_subcommand_from verify-cert' -s n -x -d 'Name to be verified (SSL host or RFC822 email)'

# ── authorize ────────────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from authorize' -s u -d 'Allow user interaction'
complete -c security -f -n '__fish_seen_subcommand_from authorize' -s p -d 'Allow returning partial rights'
complete -c security -f -n '__fish_seen_subcommand_from authorize' -s d -d 'Destroy acquired rights'
complete -c security -f -n '__fish_seen_subcommand_from authorize' -s P -d 'Pre-authorize rights only'
complete -c security -f -n '__fish_seen_subcommand_from authorize' -s l -d 'Operate authorization in least privileged mode'
complete -c security -f -n '__fish_seen_subcommand_from authorize' -s i -d 'Internalize authref passed on stdin'
complete -c security -f -n '__fish_seen_subcommand_from authorize' -s e -d 'Externalize authref to stdout'
complete -c security -f -n '__fish_seen_subcommand_from authorize' -s w -d 'Wait while holding AuthorizationRef until stdout closes'

# ── authorizationdb ──────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from authorizationdb' -a 'read write remove' -d 'Authorization policy database operation'

# ── smartcards ───────────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from smartcards' -s l -d 'List disabled smartcard tokens'
complete -c security -f -n '__fish_seen_subcommand_from smartcards' -s e -x -d 'Enable smartcard token'
complete -c security -f -n '__fish_seen_subcommand_from smartcards' -s d -x -d 'Disable smartcard token'

# ── export-smartcard ─────────────────────────────────────────────────
complete -c security -f -n '__fish_seen_subcommand_from export-smartcard' -s i -x -d 'Token id (see list-smartcards)'
complete -c security -f -n '__fish_seen_subcommand_from export-smartcard' -s t -x -a 'certs privKeys identities all' -d 'Type of items to display'
complete -c security -n '__fish_seen_subcommand_from export-smartcard' -s e -F -d 'Path to export certificates and public keys'
