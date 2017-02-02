function __fish_complete_ssh -d "common completions for ssh commands" --argument command
    complete -c $command -s 1 -d "Protocol version 1 only"
    complete -c $command -s 2 -d "Protocol version 2 only"
    complete -c $command -s 4 -d "IPv4 addresses only"
    complete -c $command -s 6 -d "IPv6 addresses only"
    complete -c $command -s C -d "Compress all data"
    complete -xc $command -s c -d "Encryption algorithm" -a "blowfish 3des des"
    complete -r -c $command -s F -d "Configuration file"
    complete -r -c $command -s i -d "Identity file"
    complete -x -c $command -s o -d "Options" -a "
		AddressFamily
		BatchMode
		BindAddress
		ChallengeResponseAuthentication
		CheckHostIP
		Cipher
		Ciphers
		Compression
		CompressionLevel
		ConnectionAttempts
		ConnectTimeout
		ControlMaster
		ControlPath
		GlobalKnownHostsFile
		GSSAPIAuthentication
		GSSAPIDelegateCredentials
		Host
		HostbasedAuthentication
		HostKeyAlgorithms
		HostKeyAlias
		HostName
		IdentityFile
		IdentitiesOnly
		LogLevel
		MACs
		NoHostAuthenticationForLocalhost
		NumberOfPasswordPrompts
		PasswordAuthentication
		Port
		PreferredAuthentications
		Protocol
		ProxyCommand
		PubkeyAuthentication
		RhostsRSAAuthentication
		RSAAuthentication
		SendEnv
		ServerAliveInterval
		ServerAliveCountMax
		SmartcardDevice
		StrictHostKeyChecking
		TCPKeepAlive
		UsePrivilegedPort
		User
		UserKnownHostsFile
		VerifyHostKeyDNS
	"
    complete -c $command -s v -d "Verbose mode"
end
