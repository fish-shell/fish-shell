complete -c fish_delta -f -a '(path filter -rf -- $fish_function_path/*.fish $fish_complete_path/*.fish $__fish_vendor_confdirs/*.fish |
path basename | path change-extension "") config'

complete -c fish_delta -s c -l no-completions -d 'Ignore completions'
complete -c fish_delta -s f -l no-functions -d 'Ignore function files'
complete -c fish_delta -s C -l no-config -d 'Ignore config files'
complete -c fish_delta -s d -l no-diff -d 'Don\'t display the full diff'
complete -c fish_delta -s n -l new -d 'Include new files'
complete -c fish_delta -s V -l vendor -d 'Choose how to count vendor files' -xa 'ignore\t"Skip vendor files" user\t"Count vendor files as belonging to the user" default\t"Count vendor files as fish defaults"'
