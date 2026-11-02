function __fish_cryptenroll_pkcs11_devices
    systemd-cryptenroll --pkcs11-token-uri=list 2>/dev/null | tail -n +2 | string replace -r '(\S+)\s(\S+).*' '$1\t$2'
    echo -e "auto\tautomatically determine URI of security token"
    echo -e "list\tlist available PKCS#11 tokens"
end

function __fish_cryptenroll_fido2_devices
    systemd-cryptenroll --fido2-device=list 2>/dev/null | tail -n +2 | string replace -ar ' +' ' ' | string replace -r '(\S+)\s(.*)' '$1\t$2'
    echo -e "auto\tautomatically determine device node of security token"
    echo -e "list\tlist available FIDO2 tokens"
end

function __fish_cryptenroll_tpm2_devices
    systemd-cryptenroll --tpm2-device=list 2>/dev/null | tail -n +2 | string replace -r '(\S+)\s(\S+).*' '$1\t$2'
    echo -e "auto\tautomatically determine device node of TPM2 device"
    echo -e "list\tlist available TPM2 devices"
end

function __fish_cryptenroll_complete_slot_types
    echo -e "password\tunlocked by a traditional passphrase"
    echo -e "recovery\tunlocked by a recovery key"
    echo -e "pkcs11\tunlocked by a PKCS#11 token"
    echo -e "fido2\tunlocked by a FIDO2 token"
    echo -e "tpm2\tunlocked by a TPM2 chip"
end

function __fish_cryptenroll_complete_wipe
    echo -e "all\twipe all key slots"
    echo -e "empty\twipe key slots unlocked by an empty passphrase"
    echo -e "password\twipe key slots unlocked by a traditional passphrase"
    echo -e "recovery\twipe key slots unlocked by a recovery key"
    echo -e "pkcs11\twipe key slots unlocked by a PKCS#11 token"
    echo -e "fido2\twipe key slots unlocked by a FIDO2 token"
    echo -e "tpm2\twipe key slots unlocked by a TPM2 chip"
    for i in (seq 0 31)
        echo -e "$i\twipe key slot $i"
    end
end

function __fish_cryptenroll_unlock_fido2_devices
    __fish_cryptenroll_fido2_devices | string match -v 'list*'
end

function __fish_cryptenroll_unlock_tpm2_devices
    __fish_cryptenroll_tpm2_devices | string match -v 'list*'
end

complete -c systemd-cryptenroll -xa "(__fish_complete_blockdevice)"

complete -c systemd-cryptenroll -l no-pager -d "Do not start a pager"
complete -c systemd-cryptenroll -l list-devices -d "List candidate block devices to operate on"
complete -c systemd-cryptenroll -l firstboot -d "Interactively enroll a credential"
complete -c systemd-cryptenroll -l prompt-suppress -kxa "(__fish_stripprefix='^--prompt-suppress=' __fish_complete_list , __fish_cryptenroll_complete_slot_types)" -d "Skip the first-boot wizard if a listed slot type exists"
complete -c systemd-cryptenroll -l chrome -xa "yes no" -d "Show a colour bar in first-boot mode"
complete -c systemd-cryptenroll -l mute-console -xa "yes no" -d "Mute the console while the first-boot wizard runs"

complete -c systemd-cryptenroll -l unlock-empty -d "Unlock the volume with an empty password"
complete -c systemd-cryptenroll -l unlock-key-file -r -d "Unlock the volume with a key file"
complete -c systemd-cryptenroll -l unlock-fido2-device -kxa "(__fish_cryptenroll_unlock_fido2_devices)" -d "Unlock the volume with a FIDO2 device"
complete -c systemd-cryptenroll -l unlock-tpm2-device -kxa "(__fish_cryptenroll_unlock_tpm2_devices)" -d "Unlock the volume with a TPM2 device"
complete -c systemd-cryptenroll -l unlock-headless -d "Try headless unlock mechanisms in turn"

complete -c systemd-cryptenroll -l password -d "Enroll a regular password"
complete -c systemd-cryptenroll -l recovery-key -d "Enroll an auto-generated recovery key"
complete -c systemd-cryptenroll -l pkcs11-token-uri -kxa "(__fish_cryptenroll_pkcs11_devices)" -d "Enroll a PKCS#11 security token or smartcard"
complete -c systemd-cryptenroll -l fido2-device -kxa "(__fish_cryptenroll_fido2_devices)" -d "Enroll a FIDO2 security token"
complete -c systemd-cryptenroll -l fido2-salt-file -r -d "Use salt from a file instead of generating one"
complete -c systemd-cryptenroll -l fido2-parameters-in-header -xa "yes no" -d "Store FIDO2 parameters in the LUKS2 header"
complete -c systemd-cryptenroll -l fido2-credential-algorithm -xa "es256 rs256 eddsa" -d "Specify COSE algorithm for FIDO2 credential"
complete -c systemd-cryptenroll -l fido2-with-client-pin -xa "yes no" -d "Require to enter a PIN when unlocking the volume"
complete -c systemd-cryptenroll -l fido2-with-user-presence -xa "yes no" -d "Require to verify presence when unlocking the volume"
complete -c systemd-cryptenroll -l fido2-with-user-verification -xa "yes no" -d "Require user verification when unlocking the volume"
complete -c systemd-cryptenroll -l tpm2-device -kxa "(__fish_cryptenroll_tpm2_devices)" -d "Enroll a TPM2 security chip"
complete -c systemd-cryptenroll -l tpm2-device-key -r -d "Enroll a TPM2 device using its public key"
complete -c systemd-cryptenroll -l tpm2-seal-key-handle -x -d "Specify handle of key to use for sealing"
complete -c systemd-cryptenroll -l tpm2-pcrs -x -d "Bind the enrollment of TPM2 device to specified PCRs"
complete -c systemd-cryptenroll -l tpm2-public-key -r -d "Enroll signed TPM2 PCR policy against PEM public key"
complete -c systemd-cryptenroll -l tpm2-public-key-policyref -x -d "Specify policy reference for the signed TPM2 PCR policy"
complete -c systemd-cryptenroll -l tpm2-public-key-pcrs -x -d "Enroll signed TPM2 PCR policy for specified PCRs"
complete -c systemd-cryptenroll -l tpm2-signature -r -d "Validate public key enrollment with JSON signature file"
complete -c systemd-cryptenroll -l tpm2-pcrlock -r -d "Specify pcrlock policy to lock against"
complete -c systemd-cryptenroll -l tpm2-with-pin -xa "yes no direct" -d "Require entering a PIN when unlocking the volume"
complete -c systemd-cryptenroll -l tpm2-argon2id-memory -x -d "Argon2id memory cost in bytes"
complete -c systemd-cryptenroll -l tpm2-argon2id-iterations -x -d "Argon2id iteration count"
complete -c systemd-cryptenroll -l tpm2-argon2id-parallelism -x -d "Argon2id parallelism/lane count"
complete -c systemd-cryptenroll -l tpm2-argon2id-iter-time -x -d "Target Argon2id benchmark time in seconds"
complete -c systemd-cryptenroll -l wipe-slot -kxa "(__fish_stripprefix='^--wipe-slot=' __fish_complete_list , __fish_cryptenroll_complete_wipe)" -d "Wipes one or more LUKS2 key slots"
complete -c systemd-cryptenroll -l help -s h -d "Print a short help"
complete -c systemd-cryptenroll -l version -d "Print a short version string"
