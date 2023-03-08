# General
complete -c zabbix_get -f -s s -l host -d "Specify host name or IP address of a host."
complete -c zabbix_get -f -s p -l port -d "Specify port number of agent running on the host. Default is 10050."
complete -c zabbix_get -f -s I -l source-address -d "Specify source IP address."
complete -c zabbix_get -f -s t -l timeout -d "Specify timeout. Valid range: 1-30 seconds (default: 30)"
complete -c zabbix_get -f -s k -l key -d "Specify key of item to retrieve value for."
complete -c zabbix_get -f -s h -l help -d "Display this help and exit."
complete -c zabbix_get -f -s V -l version -d "Output version information and exit."


# TLS
complete -c zabbix_get -f -r -l tls-connect -a "unencrypted psk cert" -d "How to connect to agent."
complete -c zabbix_get -l tls-ca-file -F -d "Full pathname of a file containing the top-level CA(s) certificates for peer certificate verification."
complete -c zabbix_get -l tls-crl-file -F -d " Full pathname of a file containing revoked certificates."
complete -c zabbix_get -f -l tls-agent-cert-issuer -d "Allowed agent certificate issuer."
complete -c zabbix_get -f -l tls-agent-cert-subject -d "Allowed agent certificate subject."
complete -c zabbix_get -l tls-cert-file -d "Full pathname of a file containing the certificate or certificate chain."
complete -c zabbix_get -l tls-key-file -d "Full pathname of a file containing the private key."
complete -c zabbix_get -f -l tls-psk-identity -d "PSK-identity string."
complete -c zabbix_get -l tls-psk-file -d "Full pathname of a file containing the pre-shared key."
complete -c zabbix_get -f -l tls-cipher13 -d "Cipher string for OpenSSL 1.1.1 or newer for TLS 1.3. Override the default ciphersuite selection criteria. This option is not available if OpenSSL version is less than 1.1.1."
complete -c zabbix_get -f -l tls-cipher -d "GnuTLS priority string (for TLS 1.2 and up) or OpenSSL cipher string (only for TLS 1.2). Override the default ciphersuite selection criteria."

