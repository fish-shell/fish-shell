
function __fish_complete_ssh -d "common completions for ssh commands"

	complete -c $argv -s 1 --description "Protocoll version 1 only"
	complete -c $argv -s 2 --description "Protocoll version 2 only"
	complete -c $argv -s 4 --description "IPv4 addresses only"
	complete -c $argv -s 6 --description "IPv6 addresses only"
	complete -c $argv -s C --description "Compress all data"
	complete -xc $argv -s c --description "Encryption algorithm" -a "blowfish 3des des"
	complete -r -c $argv -s F --description "Configuration file"
	complete -r -c $argv -s i --description "Identity file"
	complete -x -c $argv -s o --description "Options" -a "
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
	complete -c $argv -s v --description "Verbose mode"
end

