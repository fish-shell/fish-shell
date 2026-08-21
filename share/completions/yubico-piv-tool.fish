set -l action 'version \
            generate set-mgm-key reset \
            pin-retries import-key \
            import-certificate set-chuid \
            request-certificate verify-pin \
            verify-bio change-pin change-puk \
            unblock-pin selfsign-certificate \
            delete-certificate read-certificate \
            read-public-key status \
            test-signature test-decipher \
            list-readers set-ccc write-object \
            read-object attest move-key \
            delete-key'
set -l algorithm 'RSA1024 RSA2048 RSA3072 RSA4096 ECCP256 ECCP384 ED25519 X25519'
set -l hash 'SHA1 SHA256 SHA384 SHA512'
set -l key_format 'PEM PKCS12 GZIP DER SSH'
set -l pin_policy 'never once always matchonce matchalways'
set -l touch_policy 'never always cached'
set -l format 'hex base64 binary'
set -l new_key_algo 'TDES AES128 AES192 AES256'

complete -c yubico-piv-tool -f
complete -c yubico-piv-tool -s h -l help -d 'Print help and exit'
complete -c yubico-piv-tool -l full-help -d 'Print help, including hidden options, and exit'
complete -c yubico-piv-tool -s V -l version -d 'Print version and exit'
complete -c yubico-piv-tool -s v -l verbose -d 'Print more information  (default=0)'
complete -c yubico-piv-tool -s r -l reader -x -a "(yubico-piv-tool -a list-readers -r '' 2>/dev/null)" -d "Only use a matching reader  (default='Yubikey')"
complete -c yubico-piv-tool -s k -l key -d 'Management key to use, if no value is specified key will be asked for'
complete -c yubico-piv-tool -s a -l action -x -a $action
complete -c yubico-piv-tool -s a -l action -x -d 'Action to take'

complete -c yubico-piv-tool -s s -l slot -x -a 9a -d 'PIV Authentication'
complete -c yubico-piv-tool -s s -l slot -x -a 9c -d 'Digital Signature (PIN always checked)'
complete -c yubico-piv-tool -s s -l slot -x -a 9d -d 'Key Management'
complete -c yubico-piv-tool -s s -l slot -x -a 9e -d 'Card Authentication (PIN never checked)'
complete -c yubico-piv-tool -s s -l slot -x -a f9 -d Attestation
complete -c yubico-piv-tool -s s -l slot -x -a '82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f 90 91 92 93 94 95' -d 'Retired Key Management'
complete -c yubico-piv-tool -s s -l slot -x -d 'What key slot to operate on'

complete -c yubico-piv-tool -l to-slot -x -a 9a -d 'PIV Authentication'
complete -c yubico-piv-tool -l to-slot -x -a 9c -d 'Digital Signature (PIN always checked)'
complete -c yubico-piv-tool -l to-slot -x -a 9d -d 'Key Management'
complete -c yubico-piv-tool -l to-slot -x -a 9e -d 'Card Authentication (PIN never checked)'
complete -c yubico-piv-tool -l to-slot -x -a f9 -d Attestation
complete -c yubico-piv-tool -l to-slot -x -a '82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f 90 91 92 93 94 95' -d 'Retired Key Management'
complete -c yubico-piv-tool -l to-slot -x -d 'What slot to move an existing key to'

complete -c yubico-piv-tool -s A -l algorithm -x -a $algorithm
complete -c yubico-piv-tool -s A -l algorithm -x -d 'What algorithm to use'
complete -c yubico-piv-tool -s H -l hash -x -a $hash
complete -c yubico-piv-tool -s H -l hash -x -d 'Hash to use for signatures'
complete -c yubico-piv-tool -s n -l new-key -f -d 'New management key to use for action set-mgm-key, if omitted key will be asked for'
complete -c yubico-piv-tool -l pin-retries -f -d 'Number of retries before the pin code is blocked'
complete -c yubico-piv-tool -l puk-retries -f -d 'Number of retries before the puk code is blocked'
complete -c yubico-piv-tool -s i -l input -r -F -d 'Filename to use as input, - for stdin'
complete -c yubico-piv-tool -s o -l output -r -F -d 'Filename to use as output, - for stdout'
complete -c yubico-piv-tool -s K -l key-format -x -a $key_format
complete -c yubico-piv-tool -s K -l key-format -x -d "Format of the key being read/written  (default='PEM')"
complete -c yubico-piv-tool -l compress -d 'Compress a large certificate using GZIP before import'
complete -c yubico-piv-tool -l global -d 'Reset the whole device over all applications'
complete -c yubico-piv-tool -s p -l password -d 'Password for decryption of private key file, if omitted password will be asked for'
complete -c yubico-piv-tool -s S -l subject -x -d 'The subject to use for certificate request'
complete -c yubico-piv-tool -l serial -x -d 'Serial number of the self-signed certificate'
complete -c yubico-piv-tool -l valid-days -d 'Time (in days) until the self-signed certificate expires'
complete -c yubico-piv-tool -s P -l pin -d 'Pin/puk code for verification, if omitted pin/puk will be asked for'
complete -c yubico-piv-tool -s N -l new-pin -d 'New pin/puk code for changing, if omitted pin/puk will be asked for'
complete -c yubico-piv-tool -l pin-policy -x -a $pin_policy -d 'Set pin policy for action generate or import-key. Only available on YubiKey 4 or newer'
complete -c yubico-piv-tool -l touch-policy -x -a $touch_policy -d 'Set touch policy for action generate, import-key or set-mgm-key. Only available on YubiKey 4 or newer'
complete -c yubico-piv-tool -l id -x -d 'Id of object for write/read object'
complete -c yubico-piv-tool -s f -l format -x -a $format
complete -c yubico-piv-tool -s f -l format -x -d 'Format of data for write/read object'
complete -c yubico-piv-tool -l attestation -f -d 'Add attestation cross-signature  (default=off)'
complete -c yubico-piv-tool -s m -l new-key-algo -x -a $new_key_algo
complete -c yubico-piv-tool -s m -l new-key-algo -x -d 'New management key algorithm to use for action set-mgm-key'
complete -c yubico-piv-tool -l scp11 -f -d "Communication with the YubiKey is done over an encrypted channel. DEPRECATED! Please use the '--enc' flag instead"
complete -c yubico-piv-tool -l enc -f -d 'Communication with the YubiKey is done over an encrypted channel'

complete -c yubico-piv-tool -l sign -d 'Sign data  (default=off)'
complete -c yubico-piv-tool -l stdin-input -d 'Read sensitive values from stdin  (default=off)'
