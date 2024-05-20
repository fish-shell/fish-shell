# Commands
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
