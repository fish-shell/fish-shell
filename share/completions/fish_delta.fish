complete -c ghoti_delta -f -a '(path filter -rf -- $ghoti_function_path/*.ghoti $ghoti_complete_path/*.ghoti $__ghoti_vendor_confdirs/*.ghoti |
path basename | path change-extension "") config'

complete -c ghoti_delta -s c -l no-completions -d 'Ignore completions'
complete -c ghoti_delta -s f -l no-functions -d 'Ignore function files'
complete -c ghoti_delta -s C -l no-config -d 'Ignore config files'
complete -c ghoti_delta -s d -l no-diff -d 'Don\'t display the full diff'
complete -c ghoti_delta -s n -l new -d 'Include new files'
complete -c ghoti_delta -s V -l vendor -d 'Choose how to count vendor files' -xa 'ignore\t"Skip vendor files" user\t"Count vendor files as belonging to the user" default\t"Count vendor files as ghoti defaults"'
