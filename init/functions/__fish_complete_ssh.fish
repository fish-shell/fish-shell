
function __fish_complete_ssh -d "common completions for ssh commands"

	complete -c $argv -s 1 -d (_ "Protocoll version 1 only")
	complete -c $argv -s 2 -d (_ "Protocoll version 2 only")
	complete -c $argv -s 4 -d (_ "IPv4 addresses only")
	complete -c $argv -s 6 -d (_ "IPv6 addresses only")
	complete -c $argv -s C -d (_ "Compress all data")
	complete -xc $argv -s c -d (_ "Encryption algorithm") -a "blowfish 3des des"
	complete -r -c $argv -s F -d (_ "Configuration file")
	complete -r -c $argv -s i -d (_ "Identity file")
	complete -x -c $argv -s o -d (_ "Options") -a "
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
	complete -c $argv -s v -d (_ "Verbose mode")
end

