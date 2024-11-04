#
# Completions for the gpg program.
#
# This program accepts an rather large number of switches. It allows
# you to do things like changing what file descriptor errors should be
# written to, to make gpg use a different locale than the one
# specified in the environment or to specify an alternative home
# directory.

# Switches related to debugging, switches whose use is not
# recommended, switches whose behaviour is as of yet undefined,
# switches for experimental features, switches to make gpg compliant
# to legacy pgp-versions, dos-specific switches, switches meant for
# the options file and deprecated or obsolete switches have all been
# removed. The remaining list of completions is still quite
# impressive.

function __fish_complete_gpg -d "Internal function for gpg completion code deduplication" -a __fish_complete_gpg_command
    if string match -q 'gpg (GnuPG) 1.*' ($__fish_complete_gpg_command --version 2>/dev/null)
        complete -c $__fish_complete_gpg_command -l simple-sk-checksum -d 'Integrity protect secret keys by using a SHA-1 checksum'
        complete -c $__fish_complete_gpg_command -l no-sig-create-check -d "Do not verify each signature right after creation"
        complete -c $__fish_complete_gpg_command -l pgp2 -d "Set up all options to be as PGP 2.x compliant as possible"
        complete -c $__fish_complete_gpg_command -l rfc1991 -d "Try to be more RFC-1991 compliant"
    else
        complete -c $__fish_complete_gpg_command -l no-keyring -d "Do not use any keyring at all"
        complete -c $__fish_complete_gpg_command -l no-skip-hidden-recipients -d "During decryption, do not skip all anonymous recipients"
        complete -c $__fish_complete_gpg_command -l only-sign-text-ids -d "Exclude any non-text-based user ids from selection for signing"
        complete -c $__fish_complete_gpg_command -l override-session-key-fd -x -d "Don't use the public key but the specified session key"
        complete -c $__fish_complete_gpg_command -l passwd -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Change the passphrase of the secret key belonging to the given user id"
        complete -c $__fish_complete_gpg_command -l pinentry-mode -xa "default ask cancel error loopback" -d "Set the pinentry mode"
        complete -c $__fish_complete_gpg_command -l quick-add-key -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Directly add a subkey to a key"
        complete -c $__fish_complete_gpg_command -l quick-add-uid -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Adds a new user id to an existing key"
        complete -c $__fish_complete_gpg_command -l quick-gen-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Generate a standard key without needing to answer prompts"
        complete -c $__fish_complete_gpg_command -l quick-generate-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Generate a standard key without needing to answer prompts"
        complete -c $__fish_complete_gpg_command -l quick-lsign-key -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Directly sign a key from the passphrase; marks signatures as non-exportable"
        complete -c $__fish_complete_gpg_command -l quick-revoke-uid -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Revokes a user id on an existing key"
        complete -c $__fish_complete_gpg_command -l quick-revuid -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Revokes a user id on an existing key"
        complete -c $__fish_complete_gpg_command -l quick-set-expire -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Set the expiration time of the specified key"
        complete -c $__fish_complete_gpg_command -l quick-set-primary-uid -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Sets or updates the priary uid flag for the specified key"
        complete -c $__fish_complete_gpg_command -l quick-sign-key -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Directly sign a key from the passphrase"
        complete -c $__fish_complete_gpg_command -l receive-keys -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Import the keys with the given key IDs from a keyserver"
        complete -c $__fish_complete_gpg_command -s f -l recipient-file -r -d "Similar to --recipient, but encrypts to key stored in file instead"
        complete -c $__fish_complete_gpg_command -l request-origin -r -d "Tell gpg to assume that the operation ultimately originated at a particular origin"
        complete -c $__fish_complete_gpg_command -l sender -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "When creating a signature, tells gpg the user id of a key; when verifying, used to restrict information printed"
        complete -c $__fish_complete_gpg_command -l show-key -d "Take OpenPGP keys as input and print information about them"
        complete -c $__fish_complete_gpg_command -l show-keys -d "Take OpenPGP keys as input and print information about them"
        complete -c $__fish_complete_gpg_command -l skip-hidden-recipients -d "During decryption, skip all anonymous recipients"
        complete -c $__fish_complete_gpg_command -l tofu-default-policy -xa "auto good unknown bad ask" -d "Set the default TOFU policy"
        complete -c $__fish_complete_gpg_command -l tofu-policy -xa "auto good unknown bad ask" -d "Set the default TOFU policy for the specified keys"
        complete -c $__fish_complete_gpg_command -l try-secret-key -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command --list-secret-keys)" -d "Specify keys to be used for trial decryption"

        complete -c $__fish_complete_gpg_command -l with-icao-spelling -d "Print the ICAO spelling of the fingerprint in addition to the hex digits"
        complete -c $__fish_complete_gpg_command -l with-key-origin -d "Include the locally held information on the origin and last update of a key in a key listing"
        complete -c $__fish_complete_gpg_command -l with-keygrip -d "Include the keygrip in the key listings"
        complete -c $__fish_complete_gpg_command -l with-secret -d "Include info about the presence of a secret key in public key listings done with --with-colons"
        complete -c $__fish_complete_gpg_command -l no-symkey-cache -d "Disable the passphrase cache used for symmetrical en- and decryption"
        complete -c $__fish_complete_gpg_command -l no-autostart -d "Do not start the gpg-agent or the dirmngr if it has not been started and its service is required"
        complete -c $__fish_complete_gpg_command -l log-file -r -d "Write log output to the specified file"
        complete -c $__fish_complete_gpg_command -l locate-keys -d "Locate the keys given as arguments"
        complete -c $__fish_complete_gpg_command -l locate-external-keys -d "Locate they keys given as arguments; do not consider local keys"
        complete -c $__fish_complete_gpg_command -l list-signatures -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Same as --list-keys, but the signatures are listed too"
        complete -c $__fish_complete_gpg_command -l list-gcrypt-config -d "Display various internal configuration parameters of libgcrypt"
        complete -c $__fish_complete_gpg_command -l known-notation -r -d "Tell GnuPG about a critical signature notation"
        complete -c $__fish_complete_gpg_command -l key-origin -d "Track the origin of a key"
        complete -c $__fish_complete_gpg_command -l input-size-hint -r -d "Specify input size in bytes"
        complete -c $__fish_complete_gpg_command -s F -l hidden-recipient-file -r -d "Similar to --hidden-recipient, but encrypts to key stored in file instead"
        complete -c $__fish_complete_gpg_command -l generate-revocation -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Generate a revocation certificate for the complete key"
        complete -c $__fish_complete_gpg_command -l generate-key -d "Generate a new key pair"
        complete -c $__fish_complete_gpg_command -l generate-designated-revocation -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Generate a designated revocation certificate for a key"
        complete -c $__fish_complete_gpg_command -l full-gen-key -d "Generate a new key pair with dialogs for all options"
        complete -c $__fish_complete_gpg_command -l full-generate-key -d "Generate a new key pair with dialogs for all options"
        complete -c $__fish_complete_gpg_command -l export-filter -d "Define an import/export filter to apply to an imported/exported keyblock before it is written"
        complete -c $__fish_complete_gpg_command -l import-filter -d "Define an import/export filter to apply to an imported/exported keyblock before it is written"
        complete -c $__fish_complete_gpg_command -l edit-card -d "Present a menu to work with a smartcard"
        complete -c $__fish_complete_gpg_command -l disable-signer-uid -d "Don't embed the uid of the signing key in the data signature"
        complete -c $__fish_complete_gpg_command -l disable-dirmngr -d "Entirely disable the use of the Dirmngr"
        complete -c $__fish_complete_gpg_command -l dirmngr-program -r -d "Specify a dirmngr program to be used for keyserver access"
        complete -c $__fish_complete_gpg_command -l default-new-key-algo -x -d "Change the default algorothms for key generation"
        complete -c $__fish_complete_gpg_command -l compliance -xa "gnupg openpgp rfc4880 rfc4880bis rfc2440 pgp6 pgp7 pgp8" -d "Set a compliance standard for GnuPG"
        complete -c $__fish_complete_gpg_command -l change-passphrase -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Change the passphrase of the secret key belonging to the given user id"
        complete -c $__fish_complete_gpg_command -l agent-program -d "Specify an agent program to be used for secret key operations"
        complete -c $__fish_complete_gpg_command -l clear-sign -d "Make a clear text signature"
    end

    #
    # gpg subcommands
    #

    complete -c $__fish_complete_gpg_command -s s -l sign -d "Make a signature"
    complete -c $__fish_complete_gpg_command -l clearsign -d "Make a clear text signature"
    complete -c $__fish_complete_gpg_command -s b -l detach-sign -d "Make a detached signature"
    complete -c $__fish_complete_gpg_command -s e -l encrypt -d "Encrypt data"
    complete -c $__fish_complete_gpg_command -s c -l symmetric -d "Encrypt with a symmetric cipher using a passphrase"
    complete -c $__fish_complete_gpg_command -l store -d "Store only (make a simple literal data packet)"
    complete -c $__fish_complete_gpg_command -l decrypt -d "Decrypt specified file or stdin"
    complete -c $__fish_complete_gpg_command -l verify -d "Assume specified file or stdin is sigfile and verify it"
    complete -c $__fish_complete_gpg_command -l multifile -d "Modify certain other commands to accept multiple files for processing"
    complete -c $__fish_complete_gpg_command -l verify-files -d "Identical to '--multifile --verify'"
    complete -c $__fish_complete_gpg_command -l encrypt-files -d "Identical to '--multifile --encrypt'"
    complete -c $__fish_complete_gpg_command -l decrypt-files -d "Identical to --multifile --decrypt"

    complete -c $__fish_complete_gpg_command -s k -l list-keys -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "List all keys from the public keyrings, or just the ones given on the command line"
    complete -c $__fish_complete_gpg_command -l list-public-keys -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "List all keys from the public keyrings, or just the ones given on the command line"
    complete -c $__fish_complete_gpg_command -s K -l list-secret-keys -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command --list-secret-keys)" -d "List all keys from the secret keyrings, or just the ones given on the command line"
    complete -c $__fish_complete_gpg_command -l list-sigs -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Same as --list-keys, but the signatures are listed too"

    complete -c $__fish_complete_gpg_command -l check-sigs -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Same as --list-keys, but the signatures are listed and verified"
    complete -c $__fish_complete_gpg_command -l check-signatures -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Same as --list-keys, but the signatures are listed and verified"
    complete -c $__fish_complete_gpg_command -l fingerprint -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "List all keys with their fingerprints"

    complete -c $__fish_complete_gpg_command -l gen-key -d "Generate a new key pair"

    complete -c $__fish_complete_gpg_command -l edit-key -d "Present a menu which enables you to do all key related tasks" -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)"

    complete -c $__fish_complete_gpg_command -l sign-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Sign a public key with your secret key"
    complete -c $__fish_complete_gpg_command -l lsign-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Sign a public key with your secret key but mark it as non exportable"

    complete -c $__fish_complete_gpg_command -l delete-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Remove key from the public keyring"
    complete -c $__fish_complete_gpg_command -l delete-secret-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command --list-secret-keys)" -d "Remove key from the secret and public keyring"
    complete -c $__fish_complete_gpg_command -l delete-keys -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Remove key from the public keyring"
    complete -c $__fish_complete_gpg_command -l delete-secret-keys -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command --list-secret-keys)" -d "Remove key from the secret and public keyring"
    complete -c $__fish_complete_gpg_command -l delete-secret-and-public-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Same as --delete-key, but if a secret key exists, it will be removed first"

    complete -c $__fish_complete_gpg_command -l gen-revoke -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Generate a revocation certificate for the complete key"
    complete -c $__fish_complete_gpg_command -l desig-revoke -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Generate a designated revocation certificate for a key"

    complete -c $__fish_complete_gpg_command -l export -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d 'Export all or the given keys from all keyrings'
    complete -c $__fish_complete_gpg_command -l export-ssh-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d 'Export all or the given keys in OpenSSH format'
    complete -c $__fish_complete_gpg_command -l send-keys -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Same as --export but sends the keys to a keyserver"
    complete -c $__fish_complete_gpg_command -l export-secret-keys -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command --list-secret-keys)" -d "Same as --export, but exports the secret keys instead"
    complete -c $__fish_complete_gpg_command -l export-secret-subkeys -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Same as --export, but exports the secret keys instead"

    complete -c $__fish_complete_gpg_command -l import -d 'Import/merge keys'
    complete -c $__fish_complete_gpg_command -l fast-import -d 'Import/merge keys'

    complete -c $__fish_complete_gpg_command -l recv-keys -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Import the keys with the given key IDs from a keyserver"
    complete -c $__fish_complete_gpg_command -l refresh-keys -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Request updates from a keyserver for keys that already exist on the local keyring"
    complete -c $__fish_complete_gpg_command -l search-keys -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Search the keyserver for the given names"

    complete -c $__fish_complete_gpg_command -l update-trustdb -d "Do trust database maintenance"
    complete -c $__fish_complete_gpg_command -l check-trustdb -d "Do trust database maintenance without user interaction"

    complete -c $__fish_complete_gpg_command -l export-ownertrust -d "Send the ownertrust values to stdout"
    complete -c $__fish_complete_gpg_command -l import-ownertrust -d "Update the trustdb with the ownertrust values stored in specified files or stdin"

    complete -c $__fish_complete_gpg_command -l rebuild-keydb-caches -d "Create signature caches in the keyring"

    complete -c $__fish_complete_gpg_command -l print-md -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Hash)" -d "Print message digest of specified algorithm for all given files or stdin"
    complete -c $__fish_complete_gpg_command -l print-mds -d "Print message digest of all algorithms for all given files or stdin"

    complete -c $__fish_complete_gpg_command -l gen-random -xa "0 1 2" -d "Emit specified number of random bytes of the given quality level"

    complete -c $__fish_complete_gpg_command -l card-edit -d "Present a menu to work with a smartcard"
    complete -c $__fish_complete_gpg_command -l card-status -x -d "Print smartcard status"
    complete -c $__fish_complete_gpg_command -l change-pin -x -d "Change smartcard PIN"

    complete -c $__fish_complete_gpg_command -l version -d "Display version and supported algorithms, and exit"
    complete -c $__fish_complete_gpg_command -l warranty -d "Display warranty and exit"
    complete -c $__fish_complete_gpg_command -s h -l help -d "Display help and exit"

    #
    # gpg options
    #

    complete -c $__fish_complete_gpg_command -s a -l armor -d "Create ASCII armored output"
    complete -c $__fish_complete_gpg_command -s o -l output -r -d "Write output to specified file"

    complete -c $__fish_complete_gpg_command -l max-output -d "Sets a limit on the number of bytes that will be generated when processing a file" -x

    complete -c $__fish_complete_gpg_command -s u -l local-user -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Use specified key as the key to sign with"
    complete -c $__fish_complete_gpg_command -l default-key -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Use specified key as the default key to sign with"

    complete -c $__fish_complete_gpg_command -s r -l recipient -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Encrypt for specified user id"
    complete -c $__fish_complete_gpg_command -s R -l hidden-recipient -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Encrypt for specified user id, but hide the keyid of the key"
    complete -c $__fish_complete_gpg_command -l default-recipient -xa "(__fish_complete_gpg_user_id $__fish_complete_gpg_command)" -d "Use specified user id as default recipient"
    complete -c $__fish_complete_gpg_command -l default-recipient-self -d "Use the default key as default recipient"
    complete -c $__fish_complete_gpg_command -l no-default-recipient -d "Reset --default-recipient and --default-recipient-self"

    complete -c $__fish_complete_gpg_command -s v -l verbose -d "Give more information during processing"
    complete -c $__fish_complete_gpg_command -s q -l quiet -d "Quiet mode"

    complete -c $__fish_complete_gpg_command -s z -d "Compression level" -xa "(seq 1 9)"
    complete -c $__fish_complete_gpg_command -l compress-level -d "Compression level" -xa "(seq 1 9)"
    complete -c $__fish_complete_gpg_command -l bzip2-compress-level -d "Compression level" -xa "(seq 1 9)"
    complete -c $__fish_complete_gpg_command -l bzip2-decompress-lowmem -d "Use a different decompression method for BZIP2 compressed files"

    complete -c $__fish_complete_gpg_command -s t -l textmode -d "Treat input files as text and store them in the OpenPGP canonical text form with standard 'CRLF' line endings"
    complete -c $__fish_complete_gpg_command -l no-textmode -d "Don't treat input files as text and store them in the OpenPGP canonical text form with standard 'CRLF' line endings"

    complete -c $__fish_complete_gpg_command -s n -l dry-run -d "Don't make any changes (this is not completely implemented)"

    complete -c $__fish_complete_gpg_command -s i -l interactive -d "Prompt before overwrite"

    complete -c $__fish_complete_gpg_command -l batch -d "Batch mode"
    complete -c $__fish_complete_gpg_command -l no-batch -d "Don't use batch mode"
    complete -c $__fish_complete_gpg_command -l no-tty -d "Never write output to terminal"

    complete -c $__fish_complete_gpg_command -l yes -d "Assume yes on most questions"
    complete -c $__fish_complete_gpg_command -l no -d "Assume no on most questions"

    complete -c $__fish_complete_gpg_command -l ask-cert-level -d "Prompt for a certification level when making a key signature"
    complete -c $__fish_complete_gpg_command -l no-ask-cert-level -d "Don't prompt for a certification level when making a key signature"
    complete -c $__fish_complete_gpg_command -l default-cert-level -xa "0\t'Not verified' 1\t'Not verified' 2\t'Caual verification' 3\t'Extensive verification'" -d "The default certification level to use for the level check when signing a key"
    complete -c $__fish_complete_gpg_command -l min-cert-level -xa "0 1 2 3" -d "Disregard any signatures with a certification level below specified level when building the trust database"

    complete -c $__fish_complete_gpg_command -l trusted-key -xa "(__fish_complete_gpg_key_id $__fish_complete_gpg_command)" -d "Assume that the specified key is as trustworthy as one of your own secret keys"
    complete -c $__fish_complete_gpg_command -l trust-model -xa "pgp classic direct always" -d "Specify trust model"

    complete -c $__fish_complete_gpg_command -l keyid-format -xa "short 0xshort long 0xlong" -d "Select how to display key IDs"

    complete -c $__fish_complete_gpg_command -l keyserver -x -d "Use specified keyserver"
    complete -c $__fish_complete_gpg_command -l keyserver-options -xa "(__fish_append , include-revoked include-disabled honor-keyserver-url include-subkeys use-temp-files keep-temp-files verbose timeout http-proxy auto-key-retrieve)" -d "Options for the keyserver"

    complete -c $__fish_complete_gpg_command -l import-options -xa "(__fish_append , import-local-sigs repair-pks-subkey-bug merge-only)" -d "Options for importing keys"
    complete -c $__fish_complete_gpg_command -l export-options -xa "(__fish_append , export-local-sigs export-attributes export-sensitive-revkeys export-minimal)" -d "Options for exporting keys"
    complete -c $__fish_complete_gpg_command -l list-options -xa "(__fish_append , show-photos show-policy-urls show-notations show-std-notations show-user-notations show-keyserver-urls show-uid-validity show-unusable-uids show-unusable-subkeys show-keyring show-sig-expire show-sig-subpackets )" -d "Options for listing keys and signatures"
    complete -c $__fish_complete_gpg_command -l verify-options -xa "(__fish_append , show-photos show-policy-urls show-notations show-std-notations show-user-notations show-keyserver-urls show-uid-validity show-unusable-uids)" -d "Options for verifying signatures"

    complete -c $__fish_complete_gpg_command -l photo-viewer -r -d "The command line that should be run to view a photo ID"
    complete -c $__fish_complete_gpg_command -l exec-path -r -d "Sets a list of directories to search for photo viewers and keyserver helpers"

    complete -c $__fish_complete_gpg_command -l show-keyring -d "Display the keyring name at the head of key listings to show which keyring a given key resides on"
    complete -c $__fish_complete_gpg_command -l keyring -r -d "Add specified file to the current list of keyrings"

    complete -c $__fish_complete_gpg_command -l secret-keyring -r -d "Add specified file to the current list of secret keyrings"
    complete -c $__fish_complete_gpg_command -l primary-keyring -r -d "Designate specified file as the primary public keyring"

    complete -c $__fish_complete_gpg_command -l trustdb-name -r -d "Use specified file instead of the default trustdb"
    complete -c $__fish_complete_gpg_command -l homedir -xa "(__fish_complete_directories (commandline -ct))" -d "Set the home directory"
    complete -c $__fish_complete_gpg_command -l display-charset -xa " iso-8859-1 iso-8859-2 iso-8859-15 koi8-r utf-8 " -d "Set the native character set"

    complete -c $__fish_complete_gpg_command -l utf8-strings -d "Assume that following command line arguments are given in UTF8"
    complete -c $__fish_complete_gpg_command -l no-utf8-strings -d "Assume that following arguments are encoded in the character set specified by --display-charset"
    complete -c $__fish_complete_gpg_command -l options -r -d "Read options from specified file, do not read the default options file"
    complete -c $__fish_complete_gpg_command -l no-options -d "Shortcut for '--options /dev/null'"
    complete -c $__fish_complete_gpg_command -l load-extension -x -d "Load an extension module"

    complete -c $__fish_complete_gpg_command -l status-fd -x -d "Write special status strings to the specified file descriptor"
    complete -c $__fish_complete_gpg_command -l logger-fd -x -d "Write log output to the specified file descriptor"
    complete -c $__fish_complete_gpg_command -l logger-file -r -d "Write log output to the specified file"
    complete -c $__fish_complete_gpg_command -l attribute-fd -d "Write attribute subpackets to the specified file descriptor"

    complete -c $__fish_complete_gpg_command -l sk-comments -d "Include secret key comment packets when exporting secret keys"
    complete -c $__fish_complete_gpg_command -l no-sk-comments -d "Don't include secret key comment packets when exporting secret keys"

    complete -c $__fish_complete_gpg_command -l comment -x -d "Use specified string as comment string"
    complete -c $__fish_complete_gpg_command -l no-comments -d "Don't use a comment string"

    complete -c $__fish_complete_gpg_command -l emit-version -d "Include the version string in ASCII armored output"
    complete -c $__fish_complete_gpg_command -l no-emit-version -d "Don't include the version string in ASCII armored output"

    complete -c $__fish_complete_gpg_command -l sig-notation -x
    complete -c $__fish_complete_gpg_command -l cert-notation -x

    complete -c $__fish_complete_gpg_command -s N -l set-notation -x -d "Put the specified name value pair into the signature as notation data"
    complete -c $__fish_complete_gpg_command -l sig-policy-url -x -d "Set signature policy"
    complete -c $__fish_complete_gpg_command -l cert-policy-url -x -d "Set certificate policy"
    complete -c $__fish_complete_gpg_command -l set-policy-url -x -d "Set signature and certificate policy"
    complete -c $__fish_complete_gpg_command -l sig-keyserver-url -x -d "Use specified URL as a preferred keyserver for data signatures"

    complete -c $__fish_complete_gpg_command -l set-filename -x -d "Use specified string as the filename which is stored inside messages"

    complete -c $__fish_complete_gpg_command -l for-your-eyes-only -d "Set the 'for your eyes only' flag in the message"
    complete -c $__fish_complete_gpg_command -l no-for-your-eyes-only -d "Clear the 'for your eyes only' flag in the message"

    complete -c $__fish_complete_gpg_command -l completes-needed -x -d "Number of completely trusted users to introduce a new key signer (defaults to 1)"
    complete -c $__fish_complete_gpg_command -l marginals-needed -x -d "Number of marginally trusted users to introduce a new key signer (defaults to 3)"

    complete -c $__fish_complete_gpg_command -l max-cert-depth -x -d "Maximum depth of a certification chain (default is 5)"

    complete -c $__fish_complete_gpg_command -l cipher-algo -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Cipher)" -d "Use specified cipher algorithm"
    complete -c $__fish_complete_gpg_command -l digest-algo -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Hash)" -d "Use specified message digest algorithm"
    complete -c $__fish_complete_gpg_command -l compress-algo -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Compression)" -d "Use specified compression algorithm"
    complete -c $__fish_complete_gpg_command -l cert-digest-algo -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Hash)" -d "Use specified message digest algorithm when signing a key"
    complete -c $__fish_complete_gpg_command -l s2k-cipher-algo -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Cipher)" -d "Use specified cipher algorithm to protect secret keys"
    complete -c $__fish_complete_gpg_command -l s2k-digest-algo -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Hash)" -d "Use specified digest algorithm to mangle the passphrases"
    complete -c $__fish_complete_gpg_command -l s2k-mode -xa "0\t'Plain passphrase' 1\t'Salted passphrase' 3\t'Repeated salted mangling'" -d "Selects how passphrases are mangled"

    complete -c $__fish_complete_gpg_command -l disable-cipher-algo -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Cipher)" -d "Never allow the use of specified cipher algorithm"
    complete -c $__fish_complete_gpg_command -l disable-pubkey-algo -xa "(__fish_print_gpg_algo $__fish_complete_gpg_command Pubkey)" -d "Never allow the use of specified public key algorithm"

    complete -c $__fish_complete_gpg_command -l no-sig-cache -d "Do not cache the verification status of key signatures"

    complete -c $__fish_complete_gpg_command -l auto-check-trustdb -d "Automatically run the --check-trustdb command internally when needed"
    complete -c $__fish_complete_gpg_command -l no-auto-check-trustdb -d "Never automatically run the --check-trustdb"

    complete -c $__fish_complete_gpg_command -l throw-keyids -d "Do not put the recipient keyid into encrypted packets"
    complete -c $__fish_complete_gpg_command -l no-throw-keyids -d "Put the recipient keyid into encrypted packets"
    complete -c $__fish_complete_gpg_command -l not-dash-escaped -d "Change the behavior of cleartext signatures so that they can be used for patch files"

    complete -c $__fish_complete_gpg_command -l escape-from-lines -d "Mangle From-field of email headers (default)"
    complete -c $__fish_complete_gpg_command -l no-escape-from-lines -d "Do not mangle From-field of email headers"

    complete -c $__fish_complete_gpg_command -l passphrase-fd -x -d "Read passphrase from specified file descriptor"
    complete -c $__fish_complete_gpg_command -l command-fd -x -d "Read user input from specified file descriptor"

    complete -c $__fish_complete_gpg_command -l use-agent -d "Try to use the GnuPG-Agent"
    complete -c $__fish_complete_gpg_command -l no-use-agent -d "Do not try to use the GnuPG-Agent"
    complete -c $__fish_complete_gpg_command -l gpg-agent-info -x -d "Override value of GPG_AGENT_INFO environment variable"

    complete -c $__fish_complete_gpg_command -l force-v3-sigs -d "Force v3 signatures for signatures on data"
    complete -c $__fish_complete_gpg_command -l no-force-v3-sigs -d "Do not force v3 signatures for signatures on data"

    complete -c $__fish_complete_gpg_command -l force-v4-certs -d "Always use v4 key signatures even on v3 keys"
    complete -c $__fish_complete_gpg_command -l no-force-v4-certs -d "Don't use v4 key signatures on v3 keys"

    complete -c $__fish_complete_gpg_command -l force-mdc -d "Force the use of encryption with a modification detection code"
    complete -c $__fish_complete_gpg_command -l disable-mdc -d "Disable the use of the modification detection code"

    complete -c $__fish_complete_gpg_command -l allow-non-selfsigned-uid -d "Allow the import and use of keys with user IDs which are not self-signed"
    complete -c $__fish_complete_gpg_command -l no-allow-non-selfsigned-uid -d "Do not allow the import and use of keys with user IDs which are not self-signed"

    complete -c $__fish_complete_gpg_command -l allow-freeform-uid -d "Disable all checks on the form of the user ID while generating a new one"

    complete -c $__fish_complete_gpg_command -l ignore-time-conflict -d "Do not fail if signature is older than key"
    complete -c $__fish_complete_gpg_command -l ignore-valid-from -d "Allow subkeys that have a timestamp from the future"
    complete -c $__fish_complete_gpg_command -l ignore-crc-error -d "Ignore CRC errors"
    complete -c $__fish_complete_gpg_command -l ignore-mdc-error -d "Do not fail on MDC integrity protection failure"

    complete -c $__fish_complete_gpg_command -l lock-once -d "Lock the databases the first time a lock is requested and do not release the lock until the process terminates"
    complete -c $__fish_complete_gpg_command -l lock-multiple -d "Release the locks every time a lock is no longer needed"

    complete -c $__fish_complete_gpg_command -l no-random-seed-file -d "Do not create an internal pool file for quicker generation of random numbers"
    complete -c $__fish_complete_gpg_command -l no-verbose -d "Reset verbose level to 0"
    complete -c $__fish_complete_gpg_command -l no-greeting -d "Suppress the initial copyright message"
    complete -c $__fish_complete_gpg_command -l no-secmem-warning -d "Suppress the warning about 'using insecure memory'"
    complete -c $__fish_complete_gpg_command -l no-permission-warning -d "Suppress the warning about unsafe file and home directory (--homedir) permissions"
    complete -c $__fish_complete_gpg_command -l no-mdc-warning -d "Suppress the warning about missing MDC integrity protection"

    complete -c $__fish_complete_gpg_command -l require-secmem -d "Refuse to run if GnuPG cannot get secure memory"

    complete -c $__fish_complete_gpg_command -l no-require-secmem -d "Do not refuse to run if GnuPG cannot get secure memory (default)"
    complete -c $__fish_complete_gpg_command -l no-armor -d "Assume the input data is not in ASCII armored format"

    complete -c $__fish_complete_gpg_command -l no-default-keyring -d "Do not add the default keyrings to the list of keyrings"

    complete -c $__fish_complete_gpg_command -l skip-verify -d "Skip the signature verification step"

    complete -c $__fish_complete_gpg_command -l with-colons -d "Print key listings delimited by colons"
    complete -c $__fish_complete_gpg_command -l with-key-data -d "Print key listings delimited by colons (like --with-colons) and print the public key data"
    complete -c $__fish_complete_gpg_command -l with-fingerprint -d "Same as the command --fingerprint but changes only the format of the output and may be used together with another command"
    complete -c $__fish_complete_gpg_command -l with-subkey-fingerprint -d "Force printing of all subkeys"

    complete -c $__fish_complete_gpg_command -l fast-list-mode -d "Changes the output of the list commands to work faster"
    complete -c $__fish_complete_gpg_command -l fixed-list-mode -d "Do not merge primary user ID and primary key in --with-colon listing mode and print all timestamps as UNIX timestamps"

    complete -c $__fish_complete_gpg_command -l list-only -d "Changes the behaviour of some commands. This is like --dry-run but different"

    complete -c $__fish_complete_gpg_command -l show-session-key -d "Display the session key used for one message"
    complete -c $__fish_complete_gpg_command -l ask-sig-expire -d "Prompt for an expiration time"
    complete -c $__fish_complete_gpg_command -l no-ask-sig-expire -d "Do not prompt for an expiration time"

    complete -c $__fish_complete_gpg_command -l ask-cert-expire -d "Prompt for an expiration time"
    complete -c $__fish_complete_gpg_command -l no-ask-cert-expire -d "Do not prompt for an expiration time"

    complete -c $__fish_complete_gpg_command -l try-all-secrets -d "Don't look at the key ID as stored in the message but try all secret keys in turn to find the right decryption key"
    complete -c $__fish_complete_gpg_command -l enable-special-filenames -d "Enable a mode in which filenames of the form -&n, where n is a non-negative decimal number, refer to the file descriptor n and not to a file with that name"

    complete -c $__fish_complete_gpg_command -l group -x -d "Sets up a named group, which is similar to aliases in email programs"
    complete -c $__fish_complete_gpg_command -l ungroup -d "Remove a given entry from the --group list"
    complete -c $__fish_complete_gpg_command -l no-groups -d "Remove all entries from the --group list"

    complete -c $__fish_complete_gpg_command -l preserve-permissions -d "Don't change the permissions of a secret keyring back to user read/write only"

    complete -c $__fish_complete_gpg_command -l personal-cipher-preferences -x -d "Set the list of personal cipher preferences to the specified string"
    complete -c $__fish_complete_gpg_command -l personal-digest-preferences -x -d "Set the list of personal digest preferences to the specified string"
    complete -c $__fish_complete_gpg_command -l personal-compress-preferences -x -d "Set the list of personal compression preferences to the specified string"

    complete -c $__fish_complete_gpg_command -l default-preference-list -x -d "Set the list of default preferences to the specified string"

    complete -c $__fish_complete_gpg_command -l openpgp -x -d "Use strict OpenPGP behaviour"
end
