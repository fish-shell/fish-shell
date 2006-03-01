
function __fish_complete_ssh -d "common completions for ssh commands"

	complete -c $argv -s 1 -d (N_ "Protocoll version 1 only")
	complete -c $argv -s 2 -d (N_ "Protocoll version 2 only")
	complete -c $argv -s 4 -d (N_ "IPv4 addresses only")
	complete -c $argv -s 6 -d (N_ "IPv6 addresses only")
	complete -c $argv -s C -d (N_ "Compress all data")
	complete -xc $argv -s c -d (N_ "Encryption algorithm") -a "blowfish 3des des"
	complete -r -c $argv -s F -d (N_ "Configuration file")
	complete -r -c $argv -s i -d (N_ "Identity file")
	complete -x -c $argv -s o -d (N_ "Options") -a "
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
	complete -c $argv -s v -d (N_ "Verbose mode")
end

