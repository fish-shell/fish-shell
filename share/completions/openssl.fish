function __fish_openssl_subcommand_options --description "Print options for openssl subcommand"
    set -l cmd (commandline -poc)
    openssl list -options $cmd[2] | string replace -r -- '^(\S*)\s*.*' '-$1'
end

if command openssl list -1 2>&1 >/dev/null
    complete -c openssl -n __fish_use_subcommand -x -a "(openssl list -1 -commands -cipher-commands -digest-commands)"
else
    complete -c openssl -n __fish_use_subcommand -x -a "(openssl help 2>&1 | string match -rv '^[A-Z]|^\$' | string split -n ' ')"
end
complete -c openssl -n 'not __fish_use_subcommand && string match -qr -- "^-" (commandline -ct)' -a "(__fish_openssl_subcommand_options)"
