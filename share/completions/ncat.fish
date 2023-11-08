# Completions for ncat (https://www.nmap.org)

complete -c ncat -f -a "(__fish_print_hostnames)"

# PROTOCOL OPTIONS
complete -c ncat -s 4 -d "IPv4 only"
complete -c ncat -s 6 -d "IPv6 only"
complete -c ncat -s U -l unixsock -d "Use Unix domain sockets"
complete -c ncat -s u -l udp -d "Use UDP"
complete -c ncat -l sctp -d "Use SCTP"
complete -c ncat -l vsock -d "Use AF_VSOCK sockets"

# CONNECT MODE OPTIONS
complete -c ncat -s g -x -d "Loose source routing"
complete -c ncat -s G -x -d "Set source routing pointer"
complete -c ncat -s p -l source-port -x -d "Specify source port"
complete -c ncat -s s -l source -x -d "Specify source address"

# LISTEN MODE OPTIONS
complete -c ncat -s l -l listen -d "Listen for connections"
complete -c ncat -s m -l max-conns -x -d "Specify maximum number of connections"
complete -c ncat -s k -l keep-open -d "Accept multiple connections"
complete -c ncat -l broker -d "Connection brokering"
complete -c ncat -l chat -d "Ad-hoc \"chat server\""

# SSL OPTIONS
complete -c ncat -l ssl -d "Use SSL"
complete -c ncat -l ssl-verify -d "Verify server certificates"
complete -c ncat -l ssl-cert -r -d "Specify SSL certificate"
complete -c ncat -l ssl-key -r -d "Specify SSL private key"
complete -c ncat -l ssl-trustfile -r -d "List trusted certificates"
function __fish_complete_openssl_ciphers
    openssl ciphers -s -stdname | string replace -r '^([^ ]*) - ([^ ]*).*$' '$2\t$1'
    for cs in COMPLEMENTOFDEFAULT ALL COMPLEMENTOFALL HIGH MEDIUM LOW eNULL NULL aNULL kRSA aRSA RSA kDHr kDHd kDH kDHE kEDH DH DHE EDH ADH kEECDH kECDHE ECDH ECDHE EECDH AECDH aDSS DSS aDH aECDSA ECDSA TLSv1.2 TLSv1.0 SSLv3 AES128 AES256 AES AESGCM AESCCM AESCCM8 ARIA128 ARIA256 ARIA CAMELLIA128 CAMELLIA256 CAMELLIA CHACHA20 3DES DES RC4 RC2 IDEA SEED MD5 SHA1 SHA SHA256 SHA384 aGOST aGOST01 kGOST GOST94 GOST89MAC PSK kPSK kECDHEPSK kDHEPSK kRSAPSK aPSK SUITEB128 SUITEB128ONLY SUITEB192
        printf "%s\tCipher String\n" $cs
    end
end
complete -c ncat -l ssl-ciphers -x -a "(__fish_complete_list : __fish_complete_openssl_ciphers)" -d "Specify SSL ciphersuites"
complete -c ncat -l ssl-servername -x -a "(__fish_print_hostnames)" -d "Request distinct server name"
complete -c ncat -l ssl-alpn -x -d "Specify ALPN protocol list"

# PROXY OPTIONS
complete -c ncat -l proxy -x -d "Specify proxy address"
complete -c ncat -l proxy-type -x -a "http socks4 socks5" -d "Specify proxy protocol"
complete -c ncat -l proxy-auth -x -d "Specify proxy credentials"
complete -c ncat -l proxy-dns -x -a "local remote both none" -d "Specify where to resolve proxy destination"

# COMMAND EXECUTION OPTIONS
complete -c ncat -s e -l exec -r -d "Execute command"
complete -c ncat -s c -l sh-exec -r -d "Execute command via sh"
complete -c ncat -l lua-exec -r -d "Execute a .lua script"

# ACCESS CONTROL OPTIONS
complete -c ncat -l allow -x -a "(__fish_print_hostnames)" -d "Allow connections"
complete -c ncat -l allowfile -r -d "Allow connections from file"
complete -c ncat -l deny -x -a "(__fish_print_hostnames)" -d "Deny connections"
complete -c ncat -l denyfile -r -d "Deny connections from file"

# TIMING OPTIONS
complete -c ncat -s d -l delay -x -d "Specify line delay"
complete -c ncat -s i -l idle-timeout -x -d "Specify idle timeout"
complete -c ncat -s w -l wait -x -d "Specify connect timeout"

# OUTPUT OPTIONS
complete -c ncat -s o -l output -r -d "Save session data"
complete -c ncat -s x -l hex-dump -r -d "Save session data in hex"
complete -c ncat -l append-output -d "Append output"
complete -c ncat -s v -l verbose -d "Be verbose"

# MISC OPTIONS
complete -c ncat -s C -l crlf -d "Use CRLF as EOL"
complete -c ncat -s h -l help -d "Help screen"
complete -c ncat -l recv-only -d "Only receive data"
complete -c ncat -l send-only -d "Only send data"
complete -c ncat -l no-shutdown -d "Do not shutdown into half-duplex mode"
complete -c ncat -s n -l nodns -d "Do not resolve hostnames"
complete -c ncat -s t -l telnet -d "Answer Telnet negotiations"
complete -c ncat -l version -d "Display version"
complete -c ncat -s z -d "Report connection status only"
