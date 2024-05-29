# ssh-keygen
complete -c ssh-keygen -s a -x -d "number of KDF rounds used [default 16]"
complete -c ssh-keygen -s b -x -d "number of bits in the key"
complete -c ssh-keygen -s c -d "change the comment in the private and public key files"

complete -c ssh-keygen -s N -x -d "provide the new passphrase"
complete -c ssh-keygen -s P -x -d "provide the (old) passphrase"
complete -c ssh-keygen -s p -d "request changing the passphrase of a private key file"
complete -c ssh-keygen -s L -d "print the contents of one or more certificates"
complete -c ssh-keygen -s l -d "show fingerprint of public key file"
complete -c ssh-keygen -s v -d "verbose mode: print debugging messages about its progress"

complete -c ssh-keygen -s f -r -d "filename of key file"
complete -c ssh-keygen -s C -x -d "comment of the generated key"
complete -c ssh-keygen -s t -x -d "type of key to create" -a "dsa ecdsa ecdsa-sk ed25519 ed25519-sk rsa"

complete -c ssh-keygen -s A -f -d 'generate host keys of all default key types (rsa, ecdsa, and ed25519)'
complete -c ssh-keygen -s B -f -d 'show the bubblebabble digest of specified key file'
complete -c ssh-keygen -s D -f -d 'download the public keys provided by shared library pkcs11'
complete -c ssh-keygen -s E -x -d 'hash algorithm used when displaying key fingerprints' -a 'md5 sha256'
complete -c ssh-keygen -s e -d 'print public key of a OpenSSH key file in a format specified by the -m option'
complete -c ssh-keygen -s F -x -d 'find hashed host names or addresses in a known_hosts file'
complete -c ssh-keygen -s g -d 'use generic DNS format when printing fingerprint resource records using -r command'
complete -c ssh-keygen -s H -d 'hash a known_hosts file'
complete -c ssh-keygen -s h -d 'create a host certificate when signing a key'
complete -c ssh-keygen -s I -r -d 'specify the key identity when signing a public key'
complete -c ssh-keygen -s i -d 'print an OpenSSH compatible private (or public) key'
complete -c ssh-keygen -s K -d 'download resident keys from a FIDO authenticator'
complete -c ssh-keygen -s k -d 'generate a KRL file'
complete -c ssh-keygen -s M -r -d 'candidate Diffie-Hellman Group Exchange (DH-GEX) parameters' -a ' generate  screen'
complete -c ssh-keygen -s m -r -d 'key format for key generation, for -i, -e and -p flag' -a 'RFC4716 PKCS8 PEM'
complete -c ssh-keygen -s n -r -d 'specify principals (user or host names) to be included in a certificate when signing a key'
complete -c ssh-keygen -s O -d 'specify a key/value option'
complete -c ssh-keygen -s Q -d 'whether keys have been revoked in a KRL'
complete -c ssh-keygen -s q -d 'silence ssh-keygen'
complete -c ssh-keygen -s R -x -d 'removes all keys belonging to the specified hostname from a known_hosts file'
complete -c ssh-keygen -s r -x -d 'print the SSHFP fingerprint resource record named hostname'
complete -c ssh-keygen -s s -r -d 'sign a public key using the specified CA key'
complete -c ssh-keygen -s U -d 'indicates that a CA key resides in a ssh-agent'
complete -c ssh-keygen -s u -d 'update a KRL'
complete -c ssh-keygen -s V -x -d 'specify a validity interval when signing a certificate'
complete -c ssh-keygen -s w -r -d 'path to a needed library when creating FIDO authenticator-hosted keys'
complete -c ssh-keygen -s Y -x -a "\
	find-principals\t'find the principal(s) associated with the public key of a signature'
	match-principals\t'find principal matching the principal name provided using the -I flag'
	check-novalidate\t'check that a signature generated using -Y sign has a valid structure'
	sign\t'Cryptographically sign a file or some data using an SSH key'
	verify\t'Request to verify a signature generated using -Y sign'
"
complete -c ssh-keygen -s y -d 'print an OpenSSH public key from a private OpenSSH format file'
# Note: `ssh -Q` is not available on old versions
complete -c ssh-keygen -s Z -x -d 'cipher kind used for encryption an OpenSSH-format private key file' -a "(ssh -Q cipher 2>/dev/null)"
complete -c ssh-keygen -s z -x -d 'serial number embedded in the certificate'
