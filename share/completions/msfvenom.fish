function __fish_msf_db_running
    msfdb status 2>/dev/null | string match -q -r 'Database started|Active: active'
end

function __fish_complete_msf_payloads
    if not set -q __fish_msf_cached_payloads; and __fish_msf_db_running
        set -g __fish_msf_cached_payloads (msfvenom -l payloads | string replace -f -r '^\s*([[:lower:]]\S*)\s+(\S.*)' '$1\t$2' | string collect)
    end
    echo $__fish_msf_cached_payloads
end

function __fish_complete_msf_formats
    if not set -q __fish_msf_cached_formats; and __fish_msf_db_running
        set -g __fish_msf_cached_formats (msfvenom -l formats | string replace -f -r '^\s*([[:lower:]]\S*)' '$1' | string collect)
    end
    echo $__fish_msf_cached_formats
end

function __fish_complete_msf_encoders
    if not set -q __fish_msf_cached_encoders; and __fish_msf_db_running
        set -g __fish_msf_cached_encoders (msfvenom -l encoders | string replace -f -r '^\s*([[:lower:]]\S*)\s+\S+\s+(\S.*)' '$1\t$2' | string collect)
    end
    echo $__fish_msf_cached_encoders
end

function __fish_complete_msf_encrypt
    if not set -q __fish_msf_cached_encrypt; and __fish_msf_db_running
        set -g __fish_msf_cached_encrypt (msfvenom -l encrypt | string replace -f -r '^\s*([[:lower:]]\S*)' '$1' | string collect)
    end
    echo $__fish_msf_cached_encrypt
end

function __fish_complete_msf_archs
    if not set -q __fish_msf_cached_archs; and __fish_msf_db_running
        set -g __fish_msf_cached_archs (msfvenom -l archs | string replace -f -r '^\s*([[:lower:]]\S*)' '$1' | string collect)
    end
    echo $__fish_msf_cached_archs
end

function __fish_complete_msf_platforms
    if not set -q __fish_msf_cached_platforms; and __fish_msf_db_running
        set -g __fish_msf_cached_platforms (msfvenom -l platforms | string replace -f -r '^\s*([[:lower:]]\S*)' '$1' | string collect)
    end
    echo $__fish_msf_cached_platforms
end

complete -c msfvenom -f

complete -c msfvenom -s l -l list -xa 'payloads encoders nops platforms archs encrypt formats all' -d 'List all modules for type'
complete -c msfvenom -s p -l payload -xa "(__fish_complete_msf_payloads)" -d 'Payload to use'
complete -c msfvenom -l list-options -d 'List options for payload'
complete -c msfvenom -s f -l format -xa "(__fish_complete_msf_formats)" -d 'Output format'
complete -c msfvenom -s e -l encoder -xa "(__fish_complete_msf_encoders)" -d 'The encoder to use'
complete -c msfvenom -l service-name -x -d 'Service name to use when generating a service binary'
complete -c msfvenom -l sec-name -x -d 'Section name when generating Windows binaries'
complete -c msfvenom -l smallest -d 'Generate the smallest possible payload'
complete -c msfvenom -l encrypt -xa "(__fish_complete_msf_encrypt)" -d 'Type of encryption to apply to the shellcode'
complete -c msfvenom -l encrypt-key -x -d 'A key to be used for --encrypt'
complete -c msfvenom -l encrypt-iv -x -d 'An initialization vector for --encrypt'
complete -c msfvenom -s a -l arch -xa "(__fish_complete_msf_archs)" -d 'The architecture to use'
complete -c msfvenom -l platform -xa "(__fish_complete_msf_platforms)" -d 'The platform to use'
complete -c msfvenom -s o -l out -F -d 'Save the payload to a file'
complete -c msfvenom -s b -l bad-chars -x -d 'Characters to avoid'
complete -c msfvenom -s n -l nopsled -x -d 'Prepend a nopsled'
complete -c msfvenom -l pad-nops -d 'Use nopsled size as total payload size'
complete -c msfvenom -s s -l space -x -d 'Maximum size of the resulting payload'
complete -c msfvenom -l encoder-space -x -d 'Maximum size of the encoded payload'
complete -c msfvenom -s i -l iterations -x -d 'Number of times to encode the payload'
complete -c msfvenom -s c -l add-code -F -d 'Additional win32 shellcode file to include'
complete -c msfvenom -s x -l template -F -d 'Custom executable file to use as a template'
complete -c msfvenom -s k -l keep -d 'Inject the payload as a new thread (for template)'
complete -c msfvenom -s v -l var-name -x -d 'Custom variable name for certain output formats'
complete -c msfvenom -s t -l timeout -x -d 'Number of seconds to wait when reading the payload'
complete -c msfvenom -s h -l help -d 'Show help message'
