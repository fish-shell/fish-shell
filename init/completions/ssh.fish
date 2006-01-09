#
# Common completions for all ssh commands
#

for i in ssh scp
	complete -c $i -s 1 -d (_ "Protocoll version 1 only")
	complete -c $i -s 2 -d (_ "Protocoll version 2 only")
	complete -c $i -s 4 -d (_ "IPv4 addresses only")
	complete -c $i -s 6 -d (_ "IPv6 addresses only")
	complete -c $i -s C -d (_ "Compress all data")
	complete -xc $i -s c -d (_ "Encryption algorithm") -a "blowfish 3des des"
	complete -r -c $i -s F -d (_ "Configuration file")
	complete -r -c $i -s i -d (_ "Identity file")
	complete -x -c $i -s o -d (_ "Options") -a "
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
	complete -c $i -s v -d (_ "Verbose mode")
end;

#
# ssh specific completions
#

complete -x -c ssh -d Hostname -a "

(__fish_print_hostnames)

(
	#Prepend any username specified in the completion to the hostname
	echo (commandline -ct)|sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames)

(__fish_print_users)@
"

complete -c ssh -s a -d (_ "Disables forwarding of the authentication agent")
complete -c ssh -s A -d (_ "Enables forwarding of the authentication agent")
complete -x -c ssh -s b -d (_ "Interface to transmit from") -a "
(
	cat /proc/net/arp ^/dev/null| grep -v '^IP'|cut -d ' ' -f 1 ^/dev/null
)
"

complete -x -c ssh -s e -d (_ "Escape character") -a "^ none"
complete -c ssh -s f -d (_ "Go to background")
complete -c ssh -s g -d (_ "Allow remote host to connect to local forwarded ports")
complete -c ssh -s I -d (_ "Smartcard device")
complete -c ssh -s k -d (_ "Disable forwarding of Kerberos tickets")
complete -c ssh -s l -x -a "(__fish_complete_users)" -d (_ "User")
complete -c ssh -s m -d (_ "MAC algorithm")
complete -c ssh -s n -d (_ "Prevent reading from stdin")
complete -c ssh -s N -d (_ "Do not execute remote command")
complete -c ssh -s p -x -d (_ "Port")
complete -c ssh -s q -d (_ "Quiet mode")
complete -c ssh -s s -d (_ "Subsystem")
complete -c ssh -s t -d (_ "Force pseudo-tty allocation")
complete -c ssh -s T -d (_ "Disable pseudo-tty allocation")
complete -c ssh -s x -d (_ "Disable X11 forwarding")
complete -c ssh -s X -d (_ "Enable X11 forwarding")
complete -c ssh -s L -d (_ "Locally forwarded ports")
complete -c ssh -s R -d (_ "Remotely forwarded ports")
complete -c ssh -s D -d (_ "Dynamic port forwarding")
