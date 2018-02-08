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
	# It also replaces colons with \x3a
	gpg --list-keys --with-colon|cut -d : -f 10 | sed -ne 's/\\\x3a/:/g' -e 's/\(.*\) <\(.*\)>/\1'\t'\2/p'
end

function __fish_complete_gpg_key_id -d 'Complete using gpg key ids'
	# Use user_id as the description
	set -l lastid
	gpg --list-keys --with-colons | while read garbage
		switch $garbage
			case "uid*"
				echo $garbage | cut -d ":" -f 10 | sed -e "s/\\\x3a/:/g" | read lastid
			case "*"
				echo $garbage | cut -d ":" -f 5 | read fingerprint
		end
		printf "%s\t%s\n" $fingerprint $lastid
	end
end

function __fish_print_gpg_algo -d "Complete using all algorithms of the type specified in argv[1] supported by gpg. argv[1] is a regexp"
	# Set a known locale, so that the output format of 'gpg --version'
	# is at least somewhat predictable. The locale will automatically
	# expire when the function goes out of scope, and the original locale
	# will take effect again.
	set -lx LC_ALL C

	# sed script explained:
	# in the line that matches "$argv:"
	# 	define label 'loop'
	# 	if the line ends with a ','
	# 		add next line to buffer
	#		transliterate '\n' with ' '
	#		goto loop
	# 	remove everything until the first ':' of the line
	# 	remove all blanks
	# 	transliterate ',' with '\n' (OSX apparently doesn't like '\n' on RHS of the s-command)
	# 	print result
	gpg --version | sed -ne "/$argv:/"'{:loop; /,$/{N; y!\n! !; b loop}; s!^[^:]*:!!; s![ ]*!!g; y!,!\n!; p}'
end


#
# gpg subcommands
#

complete -c gpg -s s -l sign -d "Make a signature"
complete -c gpg -l clearsign -d "Make a clear text signature"
complete -c gpg -s b -l detach-sign -d "Make a detached signature"
complete -c gpg -s e -l encrypt -d "Encrypt data"
complete -c gpg -s c -l symmetric -d "Encrypt with a symmetric cipher using a passphrase"
complete -c gpg -l store -d "Store only (make a simple RFC1991 packet)"
complete -c gpg -l decrypt -d "Decrypt specified file or stdin"
complete -c gpg -l verify -d "Assume specified file or stdin is sigfile and verify it"
complete -c gpg -l multifile -d "Modify certain other commands to accept multiple files for processing"
complete -c gpg -l verify-files -d "Identical to '--multifile --verify'"
complete -c gpg -l encrypt-files -d "Identical to '--multifile --encrypt'"
complete -c gpg -l decrypt-files -d "Identical to --multifile --decrypt"

complete -c gpg -s k -l list-keys -xa "(__fish_complete_gpg_user_id)" -d "List all keys from the public keyrings, or just the ones given on the command line"
complete -c gpg -l list-public-keys -xa "(__fish_complete_gpg_user_id)" -d "List all keys from the public keyrings, or just the ones given on the command line"
complete -c gpg -s K -l list-secret-keys -xa "(__fish_complete_gpg_user_id)" -d "List all keys from the secret keyrings, or just the ones given on the command line"
complete -c gpg -l list-sigs -xa "(__fish_complete_gpg_user_id)" -d "Same as --list-keys, but the signatures are listed too"

complete -c gpg -l check-sigs -xa "(__fish_complete_gpg_user_id)" -d "Same as --list-keys, but the signatures are listed and verified"
complete -c gpg -l fingerprint -xa "(__fish_complete_gpg_user_id)" -d "List all keys with their fingerprints"
complete -c gpg -l gen-key -d "Generate a new key pair"

complete -c gpg -l edit-key -d "Present a menu which enables you to do all key related tasks" -xa "(__fish_complete_gpg_user_id)"

complete -c gpg -l sign-key -xa "(__fish_complete_gpg_user_id)" -d "Sign a public key with your secret key"
complete -c gpg -l lsign-key -xa "(__fish_complete_gpg_user_id)" -d "Sign a public key with your secret key but mark it as non exportable"

complete -c gpg -l delete-key -xa "(__fish_complete_gpg_user_id)" -d "Remove key from the public keyring"
complete -c gpg -l delete-secret-key -xa "(__fish_complete_gpg_user_id)" -d "Remove key from the secret and public keyring"
complete -c gpg -l delete-secret-and-public-key -xa "(__fish_complete_gpg_user_id)" -d "Same as --delete-key, but if a secret key exists, it will be removed first"

complete -c gpg -l gen-revoke -xa "(__fish_complete_gpg_user_id)" -d "Generate a revocation certificate for the complete key"
complete -c gpg -l desig-revoke -xa "(__fish_complete_gpg_user_id)" -d "Generate a designated revocation certificate for a key"

complete -c gpg -l export -xa "(__fish_complete_gpg_user_id)" -d 'Export all or the given keys from all keyrings'
complete -c gpg -l send-keys -xa "(__fish_complete_gpg_key_id)" -d "Same as --export but sends the keys to a keyserver"
complete -c gpg -l export-secret-keys -xa "(__fish_complete_gpg_user_id)" -d "Same as --export, but exports the secret keys instead"
complete -c gpg -l export-secret-subkeys -xa "(__fish_complete_gpg_user_id)" -d "Same as --export, but exports the secret keys instead"

complete -c gpg -l import -d 'Import/merge keys'
complete -c gpg -l fast-import -d 'Import/merge keys'

complete -c gpg -l recv-keys -xa "(__fish_complete_gpg_key_id)" -d "Import the keys with the given key IDs from a keyserver"
complete -c gpg -l refresh-keys -xa "(__fish_complete_gpg_key_id)" -d "Request updates from a keyserver for keys that already exist on the local keyring"
complete -c gpg -l search-keys -xa "(__fish_complete_gpg_user_id)" -d "Search the keyserver for the given names"
complete -c gpg -l update-trustdb -d "Do trust database maintenance"
complete -c gpg -l check-trustdb -d "Do trust database maintenance without user interaction"

complete -c gpg -l export-ownertrust -d "Send the ownertrust values to stdout"
complete -c gpg -l import-ownertrust -d "Update the trustdb with the ownertrust values stored in specified files or stdin"

complete -c gpg -l rebuild-keydb-caches -d "Create signature caches in the keyring"

complete -c gpg -l print-md -xa "(__fish_print_gpg_algo Hash)" -d "Print message digest of specified algorithm for all given files or stdin"
complete -c gpg -l print-mds -d "Print message digest of all algorithms for all given files or stdin"

complete -c gpg -l gen-random -xa "0 1 2" -d "Emit specified number of random bytes of the given quality level"

complete -c gpg -l version -d "Display version and supported algorithms, and exit"
complete -c gpg -l warranty -d "Display warranty and exit"
complete -c gpg -s h -l help -d "Display help and exit"


#
# gpg options
#

complete -c gpg -s a -l armor -d "Create ASCII armored output"
complete -c gpg -s o -l output -r -d "Write output to specified file"

complete -c gpg -l max-output -d "Sets a limit on the number of bytes that will be generated when processing a file" -x

complete -c gpg -s u -l local-user -xa "(__fish_complete_gpg_user_id)" -d "Use specified key as the key to sign with"
complete -c gpg -l default-key -xa "(__fish_complete_gpg_user_id)" -d "Use specified key as the default key to sign with"

complete -c gpg -s r -l recipient -xa "(__fish_complete_gpg_user_id)" -d "Encrypt for specified user id"
complete -c gpg -s R -l hidden-recipient -xa "(__fish_complete_gpg_user_id)" -d "Encrypt for specified user id, but hide the keyid of the key"
complete -c gpg -l default-recipient -xa "(__fish_complete_gpg_user_id)" -d "Use specified user id as default recipient"
complete -c gpg -l default-recipient-self -d "Use the default key as default recipient"
complete -c gpg -l no-default-recipient -d "Reset --default-recipient and --default-recipient-self"

complete -c gpg -s v -l verbose -d "Give more information during processing"
complete -c gpg -s q -l quiet -d "Quiet mode"

complete -c gpg -s z -d "Compression level" -xa "(seq 1 9)"
complete -c gpg -l compress-level -d "Compression level" -xa "(seq 1 9)"
complete -c gpg -l bzip2-compress-level -d "Compression level" -xa "(seq 1 9)"
complete -c gpg -l bzip2-decompress-lowmem -d "Use a different decompression method for BZIP2 compressed files"

complete -c gpg -s t -l textmode -d "Treat input files as text and store them in the OpenPGP canonical text form with standard 'CRLF' line endings"
complete -c gpg -l no-textmode -d "Don't treat input files as text and store them in the OpenPGP canonical text form with standard 'CRLF' line endings"

complete -c gpg -s n -l dry-run -d "Don't make any changes (this is not completely implemented)"

complete -c gpg -s i -l interactive -d "Prompt before overwrite"

complete -c gpg -l batch -d "Batch mode"
complete -c gpg -l no-batch -d "Don't use batch mode"
complete -c gpg -l no-tty -d "Never write output to terminal"

complete -c gpg -l yes -d "Assume yes on most questions"
complete -c gpg -l no -d "Assume no on most questions"

complete -c gpg -l ask-cert-level -d "Prompt for a certification level when making a key signature"
complete -c gpg -l no-ask-cert-level -d "Don't prompt for a certification level when making a key signature"
complete -c gpg -l default-cert-level -xa "0\t'Not verified' 1\t'Not verified' 2\t'Caual verification' 3\t'Extensive verification'" -d "The default certification level to use for the level check when signing a key"
complete -c gpg -l min-cert-level -xa "0 1 2 3" -d "Disregard any signatures with a certification level below specified level when building the trust database"

complete -c gpg -l trusted-key -xa "(__fish_complete_gpg_key_id)" -d "Assume that the specified key is as trustworthy as one of your own secret keys"
complete -c gpg -l trust-model -xa "pgp classic direct always" -d "Specify trust model"

complete -c gpg -l keyid-format -xa "short 0xshort long 0xlong" -d "Select how to display key IDs"

complete -c gpg -l keyserver -x -d "Use specified keyserver"
complete -c gpg -l keyserver-options -xa "(__fish_append , include-revoked include-disabled honor-keyserver-url include-subkeys use-temp-files keep-temp-files verbose timeout http-proxy auto-key-retrieve)" -d "Options for the keyserver"

complete -c gpg -l import-options -xa "(__fish_append , import-local-sigs repair-pks-subkey-bug merge-only)" -d "Options for importing keys"
complete -c gpg -l export-options -xa "(__fish_append , export-local-sigs export-attributes export-sensitive-revkeys export-minimal)" -d "Options for exporting keys"
complete -c gpg -l list-options -xa "(__fish_append , show-photos show-policy-urls show-notations show-std-notations show-user-notations show-keyserver-urls show-uid-validity show-unusable-uids show-unusable-subkeys show-keyring show-sig-expire show-sig-subpackets )" -d "Options for listing keys and signatures"
complete -c gpg -l verify-options -xa "(__fish_append , show-photos show-policy-urls show-notations show-std-notations show-user-notations show-keyserver-urls show-uid-validity show-unusable-uids)" -d "Options for verifying signatures"

complete -c gpg -l photo-viewer -r -d "The command line that should be run to view a photo ID"
complete -c gpg -l exec-path -r -d "Sets a list of directories to search for photo viewers and keyserver helpers"

complete -c gpg -l show-keyring -d "Display the keyring name at the head of key listings to show which keyring a given key resides on"
complete -c gpg -l keyring -r -d "Add specified file to the current list of keyrings"

complete -c gpg -l secret-keyring -r -d "Add specified file to the current list of secret keyrings"
complete -c gpg -l primary-keyring -r -d "Designate specified file as the primary public keyring"

complete -c gpg -l trustdb-name -r -d "Use specified file instead of the default trustdb"
complete -c gpg -l homedir -xa "(__fish_complete_directories (commandline -ct))" -d "Set the home directory"
complete -c gpg -l display-charset -xa " iso-8859-1 iso-8859-2 iso-8859-15 koi8-r utf-8 " -d "Set the native character set"

complete -c gpg -l utf8-strings -d "Assume that following command line arguments are given in UTF8"
complete -c gpg -l no-utf8-strings -d "Assume that following arguments are encoded in the character set specified by --display-charset"
complete -c gpg -l options -r -d "Read options from specified file, do not read the default options file"
complete -c gpg -l no-options -d "Shortcut for '--options /dev/null'"
complete -c gpg -l load-extension -x -d "Load an extension module"

complete -c gpg -l status-fd -x -d "Write special status strings to the specified file descriptor"
complete -c gpg -l logger-fd -x -d "Write log output to the specified file descriptor"
complete -c gpg -l attribute-fd -d "Write attribute subpackets to the specified file descriptor"

complete -c gpg -l sk-comments -d "Include secret key comment packets when exporting secret keys"
complete -c gpg -l no-sk-comments -d "Don't include secret key comment packets when exporting secret keys"

complete -c gpg -l comment -x -d "Use specified string as comment string"
complete -c gpg -l no-comments -d "Don't use a comment string"

complete -c gpg -l emit-version -d "Include the version string in ASCII armored output"
complete -c gpg -l no-emit-version -d "Don't include the version string in ASCII armored output"

complete -c gpg -l sig-notation -x
complete -c gpg -l cert-notation -x

complete -c gpg -s N -l set-notation -x -d "Put the specified name value pair into the signature as notation data"
complete -c gpg -l sig-policy-url -x -d "Set signature policy"
complete -c gpg -l cert-policy-url -x -d "Set certificate policy"
complete -c gpg -l set-policy-url -x -d "Set signature and certificate policy"
complete -c gpg -l sig-keyserver-url -x -d "Use specified URL as a preferred keyserver for data signatures"

complete -c gpg -l set-filename -x -d "Use specified string as the filename which is stored inside messages"

complete -c gpg -l for-your-eyes-only -d "Set the 'for your eyes only' flag in the message"
complete -c gpg -l no-for-your-eyes-only -d "Clear the 'for your eyes only' flag in the message"

complete -c gpg -l use-embedded-filename -d "Create file with name as given in data"
complete -c gpg -l no-use-embedded-filename -d "Don't create file with name as given in data"

complete -c gpg -l completes-needed -x -d "Number of completely trusted users to introduce a new key signer (defaults to 1)"
complete -c gpg -l marginals-needed -x -d "Number of marginally trusted users to introduce a new key signer (defaults to 3)"

complete -c gpg -l max-cert-depth -x -d "Maximum depth of a certification chain (default is 5)"

complete -c gpg -l cipher-algo -xa "(__fish_print_gpg_algo Cipher)" -d "Use specified cipher algorithm"
complete -c gpg -l digest-algo -xa "(__fish_print_gpg_algo Hash)" -d "Use specified message digest algorithm"
complete -c gpg -l compress-algo -xa "(__fish_print_gpg_algo Compression)" -d "Use specified compression algorithm"
complete -c gpg -l cert-digest-algo -xa "(__fish_print_gpg_algo Hash)" -d "Use specified message digest algorithm when signing a key"
complete -c gpg -l s2k-cipher-algo -xa "(__fish_print_gpg_algo Cipher)" -d "Use specified cipher algorithm to protect secret keys"
complete -c gpg -l s2k-digest-algo -xa "(__fish_print_gpg_algo Hash)" -d "Use specified digest algorithm to mangle the passphrases"
complete -c gpg -l s2k-mode -xa "0\t'Plain passphrase' 1\t'Salted passphrase' 3\t'Repeated salted mangling'" -d "Selects how passphrases are mangled"

complete -c gpg -l simple-sk-checksum -d 'Integrity protect secret keys by using a SHA-1 checksum'

complete -c gpg -l disable-cipher-algo -xa "(__fish_print_gpg_algo Cipher)" -d "Never allow the use of specified cipher algorithm"
complete -c gpg -l disable-pubkey-algo -xa "(__fish_print_gpg_algo Pubkey)" -d "Never allow the use of specified public key algorithm"

complete -c gpg -l no-sig-cache -d "Do not cache the verification status of key signatures"
complete -c gpg -l no-sig-create-check -d "Do not verify each signature right after creation"

complete -c gpg -l auto-check-trustdb -d "Automatically run the --check-trustdb command internally when needed"
complete -c gpg -l no-auto-check-trustdb -d "Never automatically run the --check-trustdb"

complete -c gpg -l throw-keyids -d "Do not put the recipient keyid into encrypted packets"
complete -c gpg -l no-throw-keyids -d "Put the recipient keyid into encrypted packets"
complete -c gpg -l not-dash-escaped -d "Change the behavior of cleartext signatures so that they can be used for patch files"

complete -c gpg -l escape-from-lines -d "Mangle From-field of email headers (default)"
complete -c gpg -l no-escape-from-lines -d "Do not mangle From-field of email headers"

complete -c gpg -l passphrase-fd -x -d "Read passphrase from specified file descriptor"
complete -c gpg -l command-fd -x -d "Read user input from specified file descriptor"

complete -c gpg -l use-agent -d "Try to use the GnuPG-Agent"
complete -c gpg -l no-use-agent -d "Do not try to use the GnuPG-Agent"
complete -c gpg -l gpg-agent-info -x -d "Override value of GPG_AGENT_INFO environment variable"

complete -c gpg -l force-v3-sigs -d "Force v3 signatures for signatures on data"
complete -c gpg -l no-force-v3-sigs -d "Do not force v3 signatures for signatures on data"

complete -c gpg -l force-v4-certs -d "Always use v4 key signatures even on v3 keys"
complete -c gpg -l no-force-v4-certs -d "Don't use v4 key signatures on v3 keys"

complete -c gpg -l force-mdc -d "Force the use of encryption with a modification detection code"
complete -c gpg -l disable-mdc -d "Disable the use of the modification detection code"

complete -c gpg -l allow-non-selfsigned-uid -d "Allow the import and use of keys with user IDs which are not self-signed"
complete -c gpg -l no-allow-non-selfsigned-uid -d "Do not allow the import and use of keys with user IDs which are not self-signed"

complete -c gpg -l allow-freeform-uid -d "Disable all checks on the form of the user ID while generating a new one"

complete -c gpg -l ignore-time-conflict -d "Do not fail if signature is older than key"
complete -c gpg -l ignore-valid-from -d "Allow subkeys that have a timestamp from the future"
complete -c gpg -l ignore-crc-error -d "Ignore CRC errors"
complete -c gpg -l ignore-mdc-error -d "Do not fail on MDC integrity protection failure"

complete -c gpg -l lock-once -d "Lock the databases the first time a lock is requested and do not release the lock until the process terminates"
complete -c gpg -l lock-multiple -d "Release the locks every time a lock is no longer needed"

complete -c gpg -l no-random-seed-file -d "Do not create an internal pool file for quicker generation of random numbers"
complete -c gpg -l no-verbose -d "Reset verbose level to 0"
complete -c gpg -l no-greeting -d "Suppress the initial copyright message"
complete -c gpg -l no-secmem-warning -d "Suppress the warning about 'using insecure memory'"
complete -c gpg -l no-permission-warning -d "Suppress the warning about unsafe file and home directory (--homedir) permissions"
complete -c gpg -l no-mdc-warning -d "Suppress the warning about missing MDC integrity protection"

complete -c gpg -l require-secmem -d "Refuse to run if GnuPG cannot get secure memory"

complete -c gpg -l no-require-secmem -d "Do not refuse to run if GnuPG cannot get secure memory (default)"
complete -c gpg -l no-armor -d "Assume the input data is not in ASCII armored format"

complete -c gpg -l no-default-keyring -d "Do not add the default keyrings to the list of keyrings"

complete -c gpg -l skip-verify -d "Skip the signature verification step"

complete -c gpg -l with-colons -d "Print key listings delimited by colons"
complete -c gpg -l with-key-data -d "Print key listings delimited by colons (like --with-colons) and print the public key data"
complete -c gpg -l with-fingerprint -d "Same as the command --fingerprint but changes only the format of the output and may be used together with another command"

complete -c gpg -l fast-list-mode -d "Changes the output of the list commands to work faster"
complete -c gpg -l fixed-list-mode -d "Do not merge primary user ID and primary key in --with-colon listing mode and print all timestamps as UNIX timestamps"

complete -c gpg -l list-only -d "Changes the behaviour of some commands. This is like --dry-run but different"

complete -c gpg -l show-session-key -d "Display the session key used for one message"
complete -c gpg -l override-session-key -x -d "Don't use the public key but the specified session key"

complete -c gpg -l ask-sig-expire -d "Prompt for an expiration time"
complete -c gpg -l no-ask-sig-expire -d "Do not prompt for an expiration time"

complete -c gpg -l ask-cert-expire -d "Prompt for an expiration time"
complete -c gpg -l no-ask-cert-expire -d "Do not prompt for an expiration time"

complete -c gpg -l try-all-secrets -d "Don't look at the key ID as stored in the message but try all secret keys in turn to find the right decryption key"
complete -c gpg -l enable-special-filenames -d "Enable a mode in which filenames of the form -&n, where n is a non-negative decimal number, refer to the file descriptor n and not to a file with that name"

complete -c gpg -l group -x -d "Sets up a named group, which is similar to aliases in email programs"
complete -c gpg -l ungroup -d "Remove a given entry from the --group list"
complete -c gpg -l no-groups -d "Remove all entries from the --group list"

complete -c gpg -l preserve-permissions -d "Don't change the permissions of a secret keyring back to user read/write only"

complete -c gpg -l personal-cipher-preferences -x -d "Set the list of personal cipher preferences to the specified string"
complete -c gpg -l personal-digest-preferences -x -d "Set the list of personal digest preferences to the specified string"
complete -c gpg -l personal-compress-preferences -x -d "Set the list of personal compression preferences to the specified string"
complete -c gpg -l default-preference-list -x -d "Set the list of default preferences to the specified string"
