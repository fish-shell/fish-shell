function __fish_complete_ssh -d "common completions for ssh commands" --argument-names command
    complete -c $command -s 4 -d "IPv4 only"
    complete -c $command -s 6 -d "IPv6 only"
    complete -c $command -s A -d "Enables forwarding of the authentication agent"
    complete -c $command -s C -d "Compress all data"
    complete -c $command -s c -d "Encryption algorithm" -xa "(__fish_complete_list , __fish_ssh_ciphers)"
    complete -c $command -s F -d "Configuration file" -rF
    complete -c $command -s i -d "Identity key file" -rF
    complete -c $command -s J -d 'ProxyJump host' -xa "(__fish_complete_user_at_hosts)"
    complete -c $command -s o -d Options -xa "
        AddKeysToAgent
        AddressFamily
        BatchMode
        BindAddress
        BindInterface
        CanonicalDomains
        CanonicalizeFallbackLocal
        CanonicalizeHostname
        CanonicalizeMaxDots
        CanonicalizePermittedCNAMEs
        CASignatureAlgorithms
        CertificateFile
        ChallengeResponseAuthentication
        CheckHostIP
        Ciphers
        ClearAllForwardings
        Compression
        ConnectionAttempts
        ConnectTimeout
        ControlMaster
        ControlPath
        ControlPersist
        DynamicForward
        EscapeChar
        ExitOnForwardFailure
        FingerprintHash
        ForwardAgent
        ForwardX11
        ForwardX11Timeout
        ForwardX11Trusted
        GatewayPorts
        GlobalKnownHostsFile
        GSSAPIAuthentication
        GSSAPIClientIdentity
        GSSAPIDelegateCredentials
        GSSAPIKexAlgorithms
        GSSAPIKeyExchange
        GSSAPIRenewalForcesRekey
        GSSAPIServerIdentity
        GSSAPITrustDns
        HashKnownHosts
        Host
        HostbasedAuthentication
        HostbasedKeyTypes
        HostKeyAlgorithms
        HostKeyAlias
        Hostname
        IdentitiesOnly
        IdentityAgent
        IdentityFile
        IPQoS
        KbdInteractiveAuthentication
        KbdInteractiveDevices
        KexAlgorithms
        LocalCommand
        LocalForward
        LogLevel
        MACs
        Match
        NoHostAuthenticationForLocalhost
        NumberOfPasswordPrompts
        PasswordAuthentication
        PermitLocalCommand
        PKCS11Provider
        Port
        PreferredAuthentications
        ProxyCommand
        ProxyJump
        ProxyUseFdpass
        PubkeyAcceptedKeyTypes
        PubkeyAuthentication
        RekeyLimit
        RemoteCommand
        RemoteForward
        RequestTTY
        SendEnv
        ServerAliveCountMax
        ServerAliveInterval
        SetEnv
        StreamLocalBindMask
        StreamLocalBindUnlink
        StrictHostKeyChecking
        TCPKeepAlive
        Tunnel
        TunnelDevice
        UpdateHostKeys
        User
        UserKnownHostsFile
        VerifyHostKeyDNS
        VisualHostKey
        XAuthLocation
    "
    complete -c $command -s q -d "Quiet mode"
    complete -c $command -s v -d "Verbose mode"
end

function __fish_ssh_ciphers -d "List of possible SSH cipher algorithms"
    ssh -Q cipher
end

function __fish_ssh_macs -d "List of possible SSH MAC algorithms"
    ssh -Q mac
end
