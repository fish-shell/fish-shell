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

complete -c systemd-cryptenroll -xa "(__fish_complete_blockdevice)"

complete -c systemd-cryptenroll -l password -d "Enroll a regular password"
complete -c systemd-cryptenroll -l recovery-key -d "Enroll an auto-generated recovery key"
complete -c systemd-cryptenroll -l pkcs11-token-uri -kxa "(__fish_cryptenroll_pkcs11_devices)" -d "Enroll a PKCS#11 security token or smartcard"
complete -c systemd-cryptenroll -l fido2-device -kxa "(__fish_cryptenroll_fido2_devices)" -d "Enroll a FIDO2 security token"
complete -c systemd-cryptenroll -l fido2-with-client-pin -xa "yes no" -d "Require to enter a PIN when unlocking the volume"
complete -c systemd-cryptenroll -l fido2-with-user-presence -xa "yes no" -d "Require to verify presence when unlocking the volume"
complete -c systemd-cryptenroll -l fido2-with-user-verification -xa "yes no" -d "Require user verification when unlocking the volume"
complete -c systemd-cryptenroll -l tpm2-device -kxa "(__fish_cryptenroll_tpm2_devices)" -d "Enroll a TPM2 security chip"
complete -c systemd-cryptenroll -l tpm2-pcrs -x -d "Bind the enrollment of TPM2 device to speficied PCRs"
complete -c systemd-cryptenroll -l wipe-slot -kxa "(__fish_complete_list , __fish_cryptenroll_complete_wipe)" -d "Wipes one or more LUKS2 key slots"
complete -c systemd-cryptenroll -l help -s h -d "Print a short help"
complete -c systemd-cryptenroll -l version -d "Print a short version string"
