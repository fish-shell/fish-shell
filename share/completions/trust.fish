# localization: skip(not-needed)
# Fish completion for trust (p11-kit trust tool).

function __fish_trust_needs_subcommand
    not __fish_seen_subcommand_from list extract extract-compat anchor dump check-format
end

# URIs of the trust anchors (pkcs11:...), described by their label
function __fish_trust_anchors
    command trust list --filter=ca-anchors 2>/dev/null | while read -l line
        switch $line
            case 'pkcs11:*'
                set uri $line
            case '*label: *'
                test -n "$uri"; and printf '%s\t%s\n' $uri (string replace -r '^\s*label:\s*' '' -- $line)
                set -e uri
        end
    end
end

complete -c trust -f

# Subcommands
complete -c trust -n __fish_trust_needs_subcommand -a list -d 'List trust or certificates'
complete -c trust -n __fish_trust_needs_subcommand -a extract -d 'Extract certificates and trust'
complete -c trust -n __fish_trust_needs_subcommand -a extract-compat -d 'Extract trust compatibility bundles'
complete -c trust -n __fish_trust_needs_subcommand -a anchor -d 'Add, remove, change trust anchors'
complete -c trust -n __fish_trust_needs_subcommand -a dump -d 'Dump trust objects in internal format'
complete -c trust -n __fish_trust_needs_subcommand -a check-format -d 'Check the format of .p11-kit files'
complete -c trust -n __fish_trust_needs_subcommand -s h -l help -d 'Show help'

# Options common to every subcommand
complete -c trust -n 'not __fish_trust_needs_subcommand' -s v -l verbose -d 'Show verbose debug output'
complete -c trust -n 'not __fish_trust_needs_subcommand' -s q -l quiet -d 'Suppress command output'
complete -c trust -n 'not __fish_trust_needs_subcommand' -l help -d 'Show help'

# --filter
complete -c trust -n '__fish_seen_subcommand_from list extract' -l filter -x -d 'Filter of what to export' -a '
    ca-anchors\t"Certificate anchors"
    blocklist\t"Distrusted certificates"
    trust-policy\t"Anchors and blocklist"
    certificates\t"All certificates"
    pkcs11:object=\t"A PKCS#11 URI"
'
complete -c trust -n '__fish_seen_subcommand_from dump' -l filter -x -d 'Filter of what to export' -a '
    all\t"All objects"
    pkcs11:object=\t"A PKCS#11 URI"
'

# --purpose
complete -c trust -n '__fish_seen_subcommand_from list extract' -l purpose -x -d 'Limit to certificates usable for the purpose' -a '
    server-auth\t"Authenticating servers"
    client-auth\t"Authenticating clients"
    email\t"Email protection"
    code-signing\t"Authenticating signed code"
'

# extract
complete -c trust -n '__fish_seen_subcommand_from extract' -l format -x -d 'Format to extract to' -a '
    x509-file\t"DER X.509 certificate file"
    x509-directory\t"Directory of X.509 certificates"
    pem-bundle\t"File containing multiple PEM blocks"
    pem-directory\t"Directory of PEM files"
    pem-directory-hash\t"Directory of PEM files with hash links"
    openssl-bundle\t"OpenSSL specific PEM bundle"
    openssl-directory\t"Directory of OpenSSL specific files"
    java-cacerts\t"Java keystore cacerts file"
    edk2-cacerts\t"Cacerts file for EDK2 HTTPS config"
'
complete -c trust -n '__fish_seen_subcommand_from extract' -s f -l overwrite -d 'Overwrite output file or directory'
complete -c trust -n '__fish_seen_subcommand_from extract' -l comment -d 'Add comments to bundles if possible'
complete -c trust -n '__fish_seen_subcommand_from extract' -F # destination

# extract-compat
complete -c trust -n '__fish_seen_subcommand_from extract-compat' -a extract -d 'Update the system trust store'
complete -c trust -n '__fish_seen_subcommand_from extract-compat' -s o -l output -x -a '(__fish_complete_directories)' -d 'Write the extracted trust store to this directory'

# anchor
complete -c trust -n '__fish_seen_subcommand_from anchor' -l store -d 'Add the given certificate files as trust anchors'
complete -c trust -n '__fish_seen_subcommand_from anchor' -l remove -d 'Remove anchors, given by file or PKCS#11 URI'
complete -c trust -n '__fish_seen_subcommand_from anchor' -F
complete -c trust -n '__fish_seen_subcommand_from anchor; and __fish_seen_argument -l remove' -a '(__fish_trust_anchors)'

# check-format
complete -c trust -n '__fish_seen_subcommand_from check-format' -F
