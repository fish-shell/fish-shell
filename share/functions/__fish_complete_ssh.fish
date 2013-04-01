
function __fish_complete_ssh -d "common completions for ssh commands" --argument command

        complete -c $command -s 1 --description "Protocol version 1 only"
        complete -c $command -s 2 --description "Protocol version 2 only"
	complete -c $command -s 4 --description "IPv4 addresses only"
	complete -c $command -s 6 --description "IPv6 addresses only"
	complete -c $command -s C --description "Compress all data"
	complete -xc $command -s c --description "Encryption algorithm" -a "blowfish 3des des"
	complete -r -c $command -s F --description "Configuration file"
	complete -r -c $command -s i --description "Identity file"
	complete -x -c $command -s o --description "Options" -a "
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
	complete -c $command -s v --description "Verbose mode"
end

