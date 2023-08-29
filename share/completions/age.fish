complete -c age -s e -l encrypt -n "not __fish_contains_opt -s d decrypt" -d encrypt
complete -c age -s r -l recipient -n "not __fish_contains_opt -s d decrypt; and not __fish_contains_opt -s p passphrase" -d "public key"
complete -c age -s R -l recipients-file -n "not __fish_contains_opt -s d decrypt; and not __fish_contains_opt -s p passphrase" -d "file with public key(s)"
complete -c age -s a -l armor -n "not __fish_contains_opt -s d decrypt" -d "PEM encode ciphertext"
complete -c age -s p -l passphrase -n "not __fish_contains_opt -s d decrypt; and not __fish_contains_opt -s r recipient -s R recipients-file" -d passphrase
complete -c age -s d -l decrypt -n "not __fish_contains_opt -s e encrypt" -d decrypt
complete -c age -s i -l identity -n "__fish_contains_opt -s e encrypt -s d decrypt" -d "file with private key(s)"
complete -c age -s j -n "__fish_contains_opt -s e encrypt -s d decrypt" -d plugin
complete -c age -l version -d "print version number"
