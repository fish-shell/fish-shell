set command jv

complete -c $command -f

complete -c $command -s h -l help -d 'Show [h]elp'
complete -c $command -s v -l version -d 'Show [v]ersion'

complete -c $command -s f -l assert-format \
    -d 'Enable the [f]ormat assertions for validated files'

complete -c $command -s c -l assert-content \
    -d 'Enable the [c]ontent assertions for validated files'

complete -c $command -l cacert -F -r \
    -d 'Specify the PEM certificate file for a peer'

complete -c $command \
    -a '2020\tdefault 4 6 7 2019' \
    -s d -l draft -x \
    -d 'Specify the [d]raft used for a schema'

complete -c $command -s k -l insecure \
    -d 'Use insecure TLS connection for a schema validation'

complete -c $command \
    -a 'simple\tdefault alt flag basic detailed' \
    -s o -l output -x \
    -d 'Specify the [o]utput format for messages'

complete -c $command -s q -l quiet \
    -d 'Do not show errors for validated files'
