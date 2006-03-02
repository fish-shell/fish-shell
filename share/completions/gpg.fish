#
# Completions for the gpg command. 
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

#
# Various functions used for dynamic completions
#

function __fish_complete_gpg_user_id -d "Complete using gpg user ids"
	# gpg doesn't seem to like it when you use the whole key name as a
	# completion, so we skip the <EMAIL> part and use it a s a
	# description.	
	gpg --list-keys --with-colon|cut -d : -f 10|sed -ne 's/\(.*\) <\(.*\)>/\1'\t'\2/p'
end

function __fish_complete_gpg_key_id -d 'Complete using gpg key ids'
	# Use user_id as the description
	gpg --list-keys --with-colons|cut -d : -f 5,10|sed -ne "s/\(.*\):\(.*\)/\1'\t'\2/p"
end

function __fish_print_gpg_algo -d "Complete using all algorithms of the type specified in argv[1] supported by gpg. argv[1] is a regexp"
	# Set a known locale, so that the output format of 'gpg --version'
	# is at least somewhat predictable. The locale will automatically
	# expire when the function goes out of scope, and the original locale
	# will take effect again.
	set -lx LC_ALL C
	gpg --version | grep "$argv:"| grep -v "Home:"|cut -d : -f 2 |tr , \n|tr -d " "
end


#
# gpg subcommands
#

complete -c gpg -s s -l sign -d (N_ "Make a signature")
complete -c gpg -l clearsign -d (N_ "Make a clear text signature")
complete -c gpg -s b -l detach-sign -d (N_ "Make a detached signature")
complete -c gpg -s e -l encrypt -d (N_ "Encrypt data")
complete -c gpg -s c -l symmetric -d (N_ "Encrypt with a symmetric cipher using a passphrase") 
complete -c gpg -l store -d (N_ "Store only (make a simple RFC1991 packet)")
complete -c gpg -l decrypt -d (N_ "Decrypt specified file or stdin")
complete -c gpg -l verify -d (N_ "Assume specified file or stdin is sigfile and verify it")
complete -c gpg -l multifile -d (N_ "Modify certain other commands to accept multiple files for processing")
complete -c gpg -l verify-files -d (N_ "Identical to '--multifile --verify'")
complete -c gpg -l encrypt-files -d (N_ "Identical to '--multifile --encrypt'")
complete -c gpg -l decrypt-files -d (N_ "Identical to --multifile --decrypt")

complete -c gpg -l list-keys -xa "(__fish_append , (__fish_complete_gpg_user_id) )" -d (N_ "List all keys from the public keyrings, or just the ones given on the command line")
complete -c gpg -l list-public-keys -xa "(__fish_append , (__fish_complete_gpg_user_id) )" -d (N_ "List all keys from the public keyrings, or just the ones given on the command line")
complete -c gpg -s K -l list-secret-keys -xa "(__fish_append , (__fish_complete_gpg_user_id) )" -d (N_ "List all keys from the secret keyrings, or just the ones given on the command line")
complete -c gpg -l list-sigs -xa "(__fish_append , (__fish_complete_gpg_user_id))" -d (N_ "Same as --list-keys, but the signatures are listed too")

complete -c gpg -l check-sigs -xa "(__fish_append , (__fish_complete_gpg_user_id))" -d (N_ "Same as --list-keys, but the signatures are listed and verified")
complete -c gpg -l fingerprint -xa "(__fish_append , (__fish_complete_gpg_user_id))" -d (N_ "List all keys with their fingerprints")
complete -c gpg -l gen-key -d (N_ "Generate a new key pair")

complete -c gpg -l edit-key -d (N_ "Present a menu which enables you to do all key related tasks") -xa "(__fish_complete_gpg_user_id)"

complete -c gpg -l sign-key -xa "(__fish_complete_gpg_user_id)" -d (N_ "Sign a public key with your secret key")
complete -c gpg -l lsign-key -xa "(__fish_complete_gpg_user_id)" -d (N_ "Sign a public key with your secret key but mark it as non exportable")

complete -c gpg -l delete-key -xa "(__fish_complete_gpg_user_id)" -d (N_ "Remove key from the public keyring")
complete -c gpg -l delete-secret-key -xa "(__fish_complete_gpg_user_id)" -d (N_ "Remove key from the secret and public keyring")
complete -c gpg -l delete-secret-and-public-key -xa "(__fish_complete_gpg_user_id)" -d (N_ "Same as --delete-key, but if a secret key exists, it will be removed first")

complete -c gpg -l gen-revoke -xa "(__fish_complete_gpg_user_id)" -d (N_ "Generate a revocation certificate for the complete key") 
complete -c gpg -l desig-revoke -xa "(__fish_complete_gpg_user_id)" -d (N_ "Generate a designated revocation certificate for a key")

complete -c gpg -l export -xa "(__fish_append , (__fish_complete_gpg_user_id))" -d (N_ "Export all or the given keys from all keyrings" )
complete -c gpg -l send-keys -xa "(__fish_append , (__fish_complete_gpg_user_id))" -d (N_ "Same as --export but sends the keys to a keyserver")
complete -c gpg -l export-secret-keys -xa "(__fish_complete_gpg_user_id)" -d (N_ "Same as --export, but exports the secret keys instead")
complete -c gpg -l export-secret-subkeys -xa "(__fish_complete_gpg_user_id)" -d (N_ "Same as --export, but exports the secret keys instead")

complete -c gpg -l import -xa "(__fish_complete_gpg_user_id)" -d (N_ "Import/merge keys" )
complete -c gpg -l fast-import -xa "(__fish_complete_gpg_user_id)" -d (N_ "Import/merge keys" )

complete -c gpg -l recv-keys -xa "(__fish_complete_gpg_key_id)" -d (N_ "Import the keys with the given key IDs from a keyserver")
complete -c gpg -l refresh-keys -xa "(__fish_complete_gpg_key_id)" -d (N_ "Request updates from a keyserver for keys that already exist on the local keyring")
complete -c gpg -l search-keys -xa "(__fish_append , (__fish_complete_gpg_user_id) )" -d (N_ "Search the keyserver for the given names")
complete -c gpg -l update-trustdb -d (N_ "Do trust database maintenance")
complete -c gpg -l check-trustdb -d (N_ "Do trust database maintenance without user interaction") 

complete -c gpg -l export-ownertrust -d (N_ "Send the ownertrust values to stdout")
complete -c gpg -l import-ownertrust -d (N_ "Update the trustdb with the ownertrust values stored in specified files or stdin")

complete -c gpg -l rebuild-keydb-caches -d (N_ "Create signature caches in the keyring")

complete -c gpg -l print-md -xa "(__fish_print_gpg_algo Hash)" -d (N_ "Print message digest of specified algorithm for all given files or stdin")
complete -c gpg -l print-mds -d (N_ "Print message digest of all algorithms for all given files or stdin")

complete -c gpg -l gen-random -xa "0 1 2" -d (N_ "Emit specified number of random bytes of the given quality level")

complete -c gpg -l version -d (N_ "Display version and supported algorithms, and exit")
complete -c gpg -l warranty -d (N_ "Display warranty and exit")
complete -c gpg -s h -l help -d (N_ "Display help and exit")


#
# gpg options
#

complete -c gpg -s a -l armor -d (N_ "Create ASCII armored output")
complete -c gpg -s o -l output -r -d (N_ "Write output to specified file")

complete -c gpg -l max-output -d (N_ "Sets a limit on the number of bytes that will be generated when processing a file") -x

complete -c gpg -s u -l local-user -xa "(__fish_complete_gpg_user_id)" -d (N_ "Use specified key as the key to sign with")
complete -c gpg -l default-key -xa "(__fish_complete_gpg_user_id)" -d (N_ "Use specified key as the default key to sign with")

complete -c gpg -s r -l recipient -xa "(__fish_complete_gpg_user_id)" -d (N_ "Encrypt for specified user id")
complete -c gpg -s R -l hidden-recipient -xa "(__fish_complete_gpg_user_id)" -d (N_ "Encrypt for specified user id, but hide the keyid of the key")
complete -c gpg -l default-recipient -xa "(__fish_complete_gpg_user_id)" -d (N_ "Use specified user id as default recipient")
complete -c gpg -l default-recipient-self -d (N_ "Use the default key as default recipient")
complete -c gpg -l no-default-recipient -d (N_ "Reset --default-recipient and --default-recipient-self")

complete -c gpg -s v -l verbose -d (N_ "Give more information during processing")
complete -c gpg -s q -l quiet -d (N_ "Quiet mode")

complete -c gpg -s z -d (N_ "Compression level") -xa "(seq 1 9)"
complete -c gpg -l compress-level -d (N_ "Compression level") -xa "(seq 1 9)"
complete -c gpg -l bzip2-compress-level -d (N_ "Compression level") -xa "(seq 1 9)"
complete -c gpg -l bzip2-decompress-lowmem -d (N_ "Use a different decompression method for BZIP2 compressed files")

complete -c gpg -s t -l textmode -d (N_ "Treat input files as text and store them in the OpenPGP canonical text form with standard 'CRLF' line endings")
complete -c gpg -l no-textmode -d (N_ "Don't treat input files as text and store them in the OpenPGP canonical text form with standard 'CRLF' line endings")

complete -c gpg -s n -l dry-run -d (N_ "Don't make any changes (this is not completely implemented)") 

complete -c gpg -s i -l interactive -d (N_ "Prompt before overwrite")

complete -c gpg -l batch -d (N_ "Batch mode")
complete -c gpg -l no-batch -d (N_ "Don't use batch mode")
complete -c gpg -l no-tty -d (N_ "Never write output to terminal")

complete -c gpg -l yes -d (N_ "Assume yes on most questions")
complete -c gpg -l no -d (N_ "Assume no on most questions")

complete -c gpg -l ask-cert-level -d (N_ "Prompt for a certification level when making a key signature")
complete -c gpg -l no-ask-cert-level -d (N_ "Don't prompt for a certification level when making a key signature")
complete -c gpg -l default-cert-level -xa "0\t'Not verified' 1\t'Not verified' 2\t'Caual verification' 3\t'Extensive verification'" -d (N_ "The default certification level to use for the level check when signing a key")
complete -c gpg -l min-cert-level -xa "0 1 2 3" -d (N_ "Disregard any signatures with a certification level below specified level when building the trust database")

complete -c gpg -l trusted-key -xa "(__fish_complete_gpg_key_id)" -d (N_ "Assume that the specified key is as trustworthy as one of your own secret keys")
complete -c gpg -l trust-model -xa "pgp classic direct always" -d (N_ "Specify trust model")

complete -c gpg -l keyid-format -xa "short 0xshort long 0xlong" -d (N_ "Select how to display key IDs")

complete -c gpg -l keyserver -x -d (N_ "Use specified keyserver")
complete -c gpg -l keyserver-options -xa "(__fish_append , include-revoked include-disabled honor-keyserver-url include-subkeys use-temp-files keep-temp-files verbose timeout http-proxy auto-key-retrieve)" -d (N_ "Options for the keyserver")

complete -c gpg -l import-options -xa "(__fish_append , import-local-sigs repair-pks-subkey-bug merge-only)" -d (N_ "Options for importing keys")
complete -c gpg -l export-options -xa "(__fish_append , export-local-sigs export-attributes export-sensitive-revkeys export-minimal)" -d (N_ "Options for exporting keys")
complete -c gpg -l list-options -xa "(__fish_append , show-photos show-policy-urls show-notations show-std-notations show-user-notations show-keyserver-urls show-uid-validity show-unusable-uids show-unusable-subkeys show-keyring show-sig-expire show-sig-subpackets )" -d (N_ "Options for listing keys and signatures")
complete -c gpg -l verify-options -xa "(__fish_append , show-photos show-policy-urls show-notations show-std-notations show-user-notations show-keyserver-urls show-uid-validity show-unusable-uids)" -d (N_ "Options for verifying signatures")

complete -c gpg -l photo-viewer -r -d (N_ "The command line that should be run to view a photo ID")
complete -c gpg -l exec-path -r -d (N_ "Sets a list of directories to search for photo viewers and keyserver helpers")

complete -c gpg -l show-keyring -d (N_ "Display the keyring name at the head of key listings to show which keyring a given key resides on")
complete -c gpg -l keyring -r -d (N_ "Add specified file to the current list of keyrings")

complete -c gpg -l secret-keyring -r -d (N_ "Add specified file to the current list of secret keyrings")
complete -c gpg -l primary-keyring -r -d (N_ "Designate specified file as the primary public keyring")

complete -c gpg -l trustdb-name -r -d (N_ "Use specified file instead of the default trustdb")
complete -c gpg -l homedir -xa "(__fish_complete_directory (commandline -ct))" -d (N_ "Set the home directory")
complete -c gpg -l display-charset -xa " iso-8859-1 iso-8859-2 iso-8859-15 koi8-r utf-8 " -d (N_ "Set the native character set")

complete -c gpg -l utf8-strings -d (N_ "Assume that following command line arguments are given in UTF8")
complete -c gpg -l no-utf8-strings -d (N_ "Assume that following arguments are encoded in the character set specified by --display-charset")
complete -c gpg -l options -r -d (N_ "Read options from specified file, do not read the default options file")
complete -c gpg -l no-options -d (N_ "Shortcut for '--options /dev/null'")
complete -c gpg -l load-extension -x -d (N_ "Load an extension module")

complete -c gpg -l status-fd -x -d (N_ "Write special status strings to the specified file descriptor")
complete -c gpg -l logger-fd -x -d (N_ "Write log output to the specified file descriptor")
complete -c gpg -l attribute-fd -d (N_ "Write attribute subpackets to the specified file descriptor")

complete -c gpg -l sk-comments -d (N_ "Include secret key comment packets when exporting secret keys")
complete -c gpg -l no-sk-comments -d (N_ "Don't include secret key comment packets when exporting secret keys")

complete -c gpg -l comment -x -d (N_ "Use specified string as comment string")
complete -c gpg -l no-comments -d (N_ "Don't use a comment string")

complete -c gpg -l emit-version -d (N_ "Include the version string in ASCII armored output")
complete -c gpg -l no-emit-version -d (N_ "Don't include the version string in ASCII armored output")

complete -c gpg -l sig-notation -x
complete -c gpg -l cert-notation -x

complete -c gpg -s N -l set-notation -x -d (N_ "Put the specified name value pair into the signature as notation data")
complete -c gpg -l sig-policy-url -x -d (N_ "Set signature policy")
complete -c gpg -l cert-policy-url -x -d (N_ "Set certificate policy")
complete -c gpg -l set-policy-url -x -d (N_ "Set signature and certificate policy")
complete -c gpg -l sig-keyserver-url -x -d (N_ "Use specified URL as a preferred keyserver for data signatures")

complete -c gpg -l set-filename -x -d (N_ "Use specified string as the filename which is stored inside messages")

complete -c gpg -l for-your-eyes-only -d (N_ "Set the 'for your eyes only' flag in the message")
complete -c gpg -l no-for-your-eyes-only -d (N_ "Clear the 'for your eyes only' flag in the message") 

complete -c gpg -l use-embedded-filename -d (N_ "Create file with name as given in data")
complete -c gpg -l no-use-embedded-filename -d (N_ "Don't create file with name as given in data")

complete -c gpg -l completes-needed -x -d (N_ "Number of completely trusted users to introduce a new key signer (defaults to 1)")
complete -c gpg -l marginals-needed -x -d (N_ "Number of marginally trusted users to introduce a new key signer (defaults to 3)")

complete -c gpg -l max-cert-depth -x -d (N_ "Maximum depth of a certification chain (default is 5)")

complete -c gpg -l cipher-algo -xa "(__fish_print_gpg_algo Cipher)" -d (N_ "Use specified cipher algorithm")
complete -c gpg -l digest-algo -xa "(__fish_print_gpg_algo Hash)" -d (N_ "Use specified message digest algorithm")
complete -c gpg -l compress-algo -xa "(__fish_print_gpg_algo Compression)" -d (N_ "Use specified compression algorithm")
complete -c gpg -l cert-digest-algo -xa "(__fish_print_gpg_algo Hash)" -d (N_ "Use specified message digest algorithm when signing a key")
complete -c gpg -l s2k-cipher-algo -xa "(__fish_print_gpg_algo Cipher)" -d (N_ "Use specified cipher algorithm to protect secret keys")
complete -c gpg -l s2k-digest-algo -xa "(__fish_print_gpg_algo Hash)" -d (N_ "Use specified digest algorithm to mangle the passphrases")
complete -c gpg -l s2k-mode -xa "0\t'Plain passphrase' 1\t'Salted passphrase' 3\t'Repeated salted mangling'" -d (N_ "Selects how passphrases are mangled")

complete -c gpg -l simple-sk-checksum -d (N_ "Integrity protect secret keys by using a SHA-1 checksum" )

complete -c gpg -l disable-cipher-algo -xa "(__fish_print_gpg_algo Cipher)" -d (N_ "Never allow the use of specified cipher algorithm")
complete -c gpg -l disable-pubkey-algo -xa "(__fish_print_gpg_algo Pubkey)" -d (N_ "Never allow the use of specified public key algorithm")

complete -c gpg -l no-sig-cache -d (N_ "Do not cache the verification status of key signatures")
complete -c gpg -l no-sig-create-check -d (N_ "Do not verify each signature right after creation")

complete -c gpg -l auto-check-trustdb -d (N_ "Automatically run the --check-trustdb command internally when needed")
complete -c gpg -l no-auto-check-trustdb -d (N_ "Never automatically run the --check-trustdb")

complete -c gpg -l throw-keyids -d (N_ "Do not put the recipient keyid into encrypted packets")
complete -c gpg -l no-throw-keyids -d (N_ "Put the recipient keyid into encrypted packets")
complete -c gpg -l not-dash-escaped -d (N_ "Change the behavior of cleartext signatures so that they can be used for patch files")

complete -c gpg -l escape-from-lines -d (N_ "Mangle From-field of email headers (default)")
complete -c gpg -l no-escape-from-lines -d (N_ "Do not mangle From-field of email headers")

complete -c gpg -l passphrase-fd -x -d (N_ "Read passphrase from specified file descriptor")
complete -c gpg -l command-fd -x -d (N_ "Read user input from specified file descriptor")

complete -c gpg -l use-agent -d (N_ "Try to use the GnuPG-Agent")
complete -c gpg -l no-use-agent -d (N_ "Do not try to use the GnuPG-Agent")
complete -c gpg -l gpg-agent-info -x -d (N_ "Override value of GPG_AGENT_INFO environment variable")

complete -c gpg -l force-v3-sigs -d (N_ "Force v3 signatures for signatures on data")
complete -c gpg -l no-force-v3-sigs -d (N_ "Do not force v3 signatures for signatures on data")

complete -c gpg -l force-v4-certs -d (N_ "Always use v4 key signatures even on v3 keys")
complete -c gpg -l no-force-v4-certs -d (N_ "Don't use v4 key signatures on v3 keys")

complete -c gpg -l force-mdc -d (N_ "Force the use of encryption with a modification detection code")
complete -c gpg -l disable-mdc -d (N_ "Disable the use of the modification detection code")

complete -c gpg -l allow-non-selfsigned-uid -d (N_ "Allow the import and use of keys with user IDs which are not self-signed")
complete -c gpg -l no-allow-non-selfsigned-uid -d (N_ "Do not allow the import and use of keys with user IDs which are not self-signed")

complete -c gpg -l allow-freeform-uid -d (N_ "Disable all checks on the form of the user ID while generating a new one")

complete -c gpg -l ignore-time-conflict -d (N_ "Do not fail if signature is older than key")
complete -c gpg -l ignore-valid-from -d (N_ "Allow subkeys that have a timestamp from the future")
complete -c gpg -l ignore-crc-error -d (N_ "Ignore CRC errors")
complete -c gpg -l ignore-mdc-error -d (N_ "Do not fail on MDC integrity protection failure")

complete -c gpg -l lock-once -d (N_ "Lock the databases the first time a lock is requested and do not release the lock until the process terminates")
complete -c gpg -l lock-multiple -d (N_ "Release the locks every time a lock is no longer needed")

complete -c gpg -l no-random-seed-file -d (N_ "Do not create an internal pool file for quicker generation of random numbers")
complete -c gpg -l no-verbose -d (N_ "Reset verbose level to 0")
complete -c gpg -l no-greeting -d (N_ "Suppress the initial copyright message")
complete -c gpg -l no-secmem-warning -d (N_ "Suppress the warning about 'using insecure memory'")
complete -c gpg -l no-permission-warning -d (N_ "Suppress the warning about unsafe file and home directory (--homedir) permissions")
complete -c gpg -l no-mdc-warning -d (N_ "Suppress the warning about missing MDC integrity protection")

complete -c gpg -l require-secmem -d (N_ "Refuse to run if GnuPG cannot get secure memory")

complete -c gpg -l no-require-secmem -d (N_ "Do not refuse to run if GnuPG cannot get secure memory (default)")
complete -c gpg -l no-armor -d (N_ "Assume the input data is not in ASCII armored format")

complete -c gpg -l no-default-keyring -d (N_ "Do not add the default keyrings to the list of keyrings")

complete -c gpg -l skip-verify -d (N_ "Skip the signature verification step")

complete -c gpg -l with-colons -d (N_ "Print key listings delimited by colons")
complete -c gpg -l with-key-data -d (N_ "Print key listings delimited by colons (like --with-colons) and print the public key data")
complete -c gpg -l with-fingerprint -d (N_ "Same as the command --fingerprint but changes only the format of the output and may be used together with another command")

complete -c gpg -l fast-list-mode -d (N_ "Changes the output of the list commands to work faster")
complete -c gpg -l fixed-list-mode -d (N_ "Do not merge primary user ID and primary key in --with-colon listing mode and print all timestamps as UNIX timestamps")

complete -c gpg -l list-only -d (N_ "Changes the behaviour of some commands. This is like --dry-run but different")

complete -c gpg -l show-session-key -d (N_ "Display the session key used for one message")
complete -c gpg -l override-session-key -x -d (N_ "Don't use the public key but the specified session key")

complete -c gpg -l ask-sig-expire -d (N_ "Prompt for an expiration time")
complete -c gpg -l no-ask-sig-expire -d (N_ "Do not prompt for an expiration time")

complete -c gpg -l ask-cert-expire -d (N_ "Prompt for an expiration time")
complete -c gpg -l no-ask-cert-expire -d (N_ "Do not prompt for an expiration time")

complete -c gpg -l try-all-secrets -d (N_ "Don't look at the key ID as stored in the message but try all secret keys in turn to find the right decryption key")
complete -c gpg -l enable-special-filenames -d (N_ "Enable a mode in which filenames of the form -&n, where n is a non-negative decimal number, refer to the file descriptor n and not to a file with that name")

complete -c gpg -l group -x -d (N_ "Sets up a named group, which is similar to aliases in email programs")
complete -c gpg -l ungroup -d (N_ "Remove a given entry from the --group list")
complete -c gpg -l no-groups -d (N_ "Remove all entries from the --group list")

complete -c gpg -l preserve-permissions -d (N_ "Don't change the permissions of a secret keyring back to user read/write only")

complete -c gpg -l personal-cipher-preferences -x -d (N_ "Set the list of personal cipher preferences to the specified string")
complete -c gpg -l personal-digest-preferences -x -d (N_ "Set the list of personal digest preferences to the specified string")
complete -c gpg -l personal-compress-preferences -x -d (N_ "Set the list of personal compression preferences to the specified string")
complete -c gpg -l default-preference-list -x -d (N_ "Set the list of default preferences to the specified string")
