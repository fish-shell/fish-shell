# Commands
complete -c ssh-copy-id -d Remote -xa "(__fish_complete_user_at_hosts)"
complete -c ssh-copy-id -d Remote -k -fa '(__ssh_history_completions)'

# Options
complete -c ssh-copy-id -s i -d IdentityFile -r -F
complete -c ssh-copy-id -s p -d Port -r -x
complete -c ssh-copy-id -s F -d 'Alternate ssh config file' -r -F

# Load completions shared by various ssh tools like ssh, scp and sftp.
complete -c ssh-copy-id -s o -d Options -xa "
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

complete -c ssh-copy-id -s f -d 'Force mode'
complete -c ssh-copy-id -s n -d 'Dry run'
complete -c ssh-copy-id -s s -d 'Use sftp'
complete -c ssh-copy-id -s h -s \? -d 'Show help'
