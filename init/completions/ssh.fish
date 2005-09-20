#
# Common completions for all ssh commands
#

for i in ssh scp
	complete -c $i -s 1 -d "Protocall version 1 only"
	complete -c $i -s 2 -d "Protocall version 2 only"
	complete -c $i -s 4 -d "IPv4 addresses only"
	complete -c $i -s 6 -d "IPv6 addresses only"
	complete -c $i -s C -d "Compress all data"
	complete -xc $i -s c -d "Encryption algorithm" -a "blowfish 3des des"
	complete -r -c $i -s F -d "Configuration file"
	complete -r -c $i -s i -d "Identity file"
	complete -x -c $i -s o -d "Options" -a "
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
	complete -c $i -s v -d "Verbose mode"
end;

#
# ssh specific completions
#

complete -x -c ssh -d Hostname -a "

(__fish_print_hostnames)

(
	#Prepend any username specified in the completion to the hostname
	echo (commandline -ct)|grep -o '.*@'
)(__fish_print_hostnames)

(__fish_print_users)@
"

complete -c ssh -s a -d "Disables forwarding of the authentication agent"
complete -c ssh -s A -d "Enables forwarding of the authentication agent"
complete -x -c ssh -s b -d "Interface to transmit from" -a "
(
	cat /proc/net/arp ^/dev/null| grep -v '^IP'|cut -d ' ' -f 1 ^/dev/null
)
"

complete -x -c ssh -s e -d "Escape character" -a "^ none"
complete -c ssh -s f -d "Go to background"
complete -c ssh -s g -d "Allow remote host to connect to local forwarded ports"
complete -c ssh -s I -d "Smartcard device"
complete -c ssh -s k -d "Disable forwarding of Kerberos tickets"
complete -c ssh -s l -x -a "(__fish_complete_users)" -d "User"
complete -c ssh -s m -d "MAC algorithm"
complete -c ssh -s n -d "Prevent reading from stdin"
complete -c ssh -s N -d "Do not execute remote command"
complete -c ssh -s p -x -d "Port"
complete -c ssh -s q -d "Quiet mode"
complete -c ssh -s s -d "Subsystem"
complete -c ssh -s t -d "Force pseudo-tty allocation"
complete -c ssh -s T -d "Disable pseudo-tty allocation"
complete -c ssh -s x -d "Disable X11 forwarding"
complete -c ssh -s X -d "Enable X11 forwarding"
complete -c ssh -s L -d "Locally forwarded ports"
complete -c ssh -s R -d "Remotely forwarded ports"
complete -c ssh -s D -d "Dynamic port forwarding"
