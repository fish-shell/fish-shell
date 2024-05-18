# Completions for qubes-gpg-client.
# These completions are mostly taken from fish's completions for gpg.
#
# Functions to generate completion options through gpg invocations
# are not implemented for this tool, as they are incompatible with
# Qubes's qrexec authorization prompts for split GPG (even if this worked,
# the resulting behavior would be quite annoying)
#
# Completions for nearly all of qubes-gpg-client's command line options
# are implemented-- the few that are excluded are not intended for
# interactive usage or are poorly documented.

complete -c qubes-gpg-client -s b -l detach-sign -d "Make a detached signature"
complete -c qubes-gpg-client -s a -l armor -d "Create ASCII armored output"
complete -c qubes-gpg-client -s c -l symmetric -d "Encrypt with a symmetric cipher using a passphrase"
complete -c qubes-gpg-client -l decrypt -d "Decrypt specified file or stdin"
complete -c qubes-gpg-client -s e -l encrypt -d "Encrypt data"
complete -c qubes-gpg-client -s k -l list-keys -d "List all keys from the public keyrings, or just the ones given on the command line"
complete -c qubes-gpg-client -s K -l list-secret-keys -d "List all keys from the secret keyrings, or just the ones given on the command line"
complete -c qubes-gpg-client -s n -l dry-run -d "Don't make any changes (this is not completely implemented)"
complete -c qubes-gpg-client -s o -l output -r -d "Write output to specified file"
complete -c qubes-gpg-client -s q -l quiet -d "Quiet mode"
complete -c qubes-gpg-client -s r -l recipient -x -d "Encrypt for specified user id"
complete -c qubes-gpg-client -s R -l hidden-recipient -x -d "Encrypt for specified user id, but hide the keyid of the key"
complete -c qubes-gpg-client -s s -l sign -d "Make a signature"
complete -c qubes-gpg-client -s t -l textmode -d "Treat input files as text and store them in the OpenPGP canonical text form with standard 'CRLF' line endings"
complete -c qubes-gpg-client -s u -l local-user -x -d "Use specified key as the key to sign with"
complete -c qubes-gpg-client -s v -l verbose -d "Give more information during processing"
complete -c qubes-gpg-client -l always-trust -d "Skip key validation and assume that used keys are always valid"
complete -c qubes-gpg-client -l batch -d "Batch mode"
complete -c qubes-gpg-client -l cert-digest-algo -x -d "Use specified message digest algorithm when signing a key"
complete -c qubes-gpg-client -l charset -l display-charset -xa " iso-8859-1 iso-8859-2 iso-8859-15 koi8-r utf-8 " -d "Set the native character set"
complete -c qubes-gpg-client -l cipher-algo -x -d "Use specified cipher algorithm"
complete -c qubes-gpg-client -l clearsign -d "Make a clear text signature"
complete -c qubes-gpg-client -l command-fd -x -d "Read user input from specified file descriptor"
complete -c qubes-gpg-client -l comment -x -d "Use specified string as comment string"
complete -c qubes-gpg-client -l compress-algo -x -d "Use specified compression algorithm"
complete -c qubes-gpg-client -l digest-algo -x -d "Use specified message digest algorithm"
complete -c qubes-gpg-client -l disable-cipher-algo -x -d "Never allow the use of specified cipher algorithm"
complete -c qubes-gpg-client -l disable-mdc -d "Disable the use of the modification detection code"
complete -c qubes-gpg-client -l disable-pubkey-algo -x -d "Never allow the use of specified public key algorithm"
complete -c qubes-gpg-client -l emit-version -d "Include the version string in ASCII armored output"
complete -c qubes-gpg-client -l export -x -d 'Export all or the given keys from all keyrings'
complete -c qubes-gpg-client -l fingerprint -d "List all keys with their fingerprints"
complete -c qubes-gpg-client -l fixed-list-mode -d "Do not merge primary user ID/key in --with-colons listing mode and make timestamps UNIX time"
complete -c qubes-gpg-client -l force-mdc -d "Force the use of encryption with a modification detection code"
complete -c qubes-gpg-client -l force-v3-sigs -d "Force v3 signatures for signatures on data"
complete -c qubes-gpg-client -l force-v4-certs -d "Always use v4 key signatures even on v3 keys"
complete -c qubes-gpg-client -l gnupg -d "Use standard GnuPG behavior"
complete -c qubes-gpg-client -l list-options -d "Display various internal configuration parameters of GnuPG"
complete -c qubes-gpg-client -l list-only -d "Changes the behaviour of some commands. This is like --dry-run but different"
complete -c qubes-gpg-client -l list-public-keys -x -d "List all keys from the public keyrings, or just the ones given on the command line"
complete -c qubes-gpg-client -l list-sigs -d "Same as --list-keys, but the signatures are listed too"
complete -c qubes-gpg-client -l max-output -x -d "Sets a limit on the number of bytes that will be generated when processing a file"
complete -c qubes-gpg-client -l no-comments -d "Don't use a comment string"
complete -c qubes-gpg-client -l no-emit-version -d "Don't include the version string in ASCII armored output"
complete -c qubes-gpg-client -l no-force-v3-sigs -d "Do not force v3 signatures for signatures on data"
complete -c qubes-gpg-client -l no-force-v4-certs -d "Don't use v4 key signatures on v3 keys"
complete -c qubes-gpg-client -l no-greeting -d "Suppress the initial copyright message"
complete -c qubes-gpg-client -l no-secmem-warning -d "Suppress the warning about 'using insecure memory'"
complete -c qubes-gpg-client -l no-tty -d "Never write output to terminal"
complete -c qubes-gpg-client -l no-verbose -d "Reset verbose level to 0"
complete -c qubes-gpg-client -l openpgp -d "Reset all packet, cipher, and digest options to strict OpenPGP behavior"
complete -c qubes-gpg-client -l personal-cipher-preferences -x -d "Set the list of personal cipher preferences to the specified string"
complete -c qubes-gpg-client -l personal-compress-preferences -x -d "Set the list of personal compression preferences to the specified string"
complete -c qubes-gpg-client -l personal-digest-preferences -x -d "Set the list of personal digest preferences to the specified string"
complete -c qubes-gpg-client -l s2k-cipher-algo -x -d "Use specified cipher algorithm to protect secret keys"
complete -c qubes-gpg-client -l s2k-digest-algo -x -d "Use specified digest algorithm to mangle the passphrases"
complete -c qubes-gpg-client -l s2k-count -x -d "Specify how many times the passphrase mangling for symmetric encryption is repeated"
complete -c qubes-gpg-client -l s2k-mode -xa "0\t'Plain passphrase' 1\t'Salted passphrase' 3\t'Repeated salted mangling'" -d "Selects how passphrases are mangled"
complete -c qubes-gpg-client -l status-fd -x -d "Write special status strings to the specified file descriptor"
complete -c qubes-gpg-client -l store -d "Store only (make a simple RFC1991 packet)"
complete -c qubes-gpg-client -l trust-model -xa "pgp classic direct always" -d "Specify trust model"
complete -c qubes-gpg-client -l use-agent -d "Try to use the GnuPG-Agent"
complete -c qubes-gpg-client -l verify -r -d "Assume specified file or stdin is sigfile and verify it"
complete -c qubes-gpg-client -l version -d "Display version and supported algorithms, and exit"
complete -c qubes-gpg-client -l with-colons -d "Print key listings delimited by colons"
complete -c qubes-gpg-client -l with-fingerprint -d "Like --fingerprint but only change the output format"
complete -c qubes-gpg-client -l with-keygrip -d "Include the keygrip in the key listings"

# qubes-gpg-client-wrapper wraps qubes-gpg-client and qubes-gpg-import-key, but the latter needs no
# completions (only takes a file as an argument)
complete -c qubes-gpg-client-wrapper -w qubes-gpg-client
