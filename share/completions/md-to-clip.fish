# Source: https://github.com/command-line-interface-pages/v2-tooling/tree/main/md-to-clip
complete -c md-to-clip -s h -l help -d 'Display help'
complete -c md-to-clip -s v -l version -d 'Display version'
complete -c md-to-clip -s a -l author -d 'Display author'
complete -c md-to-clip -s e -l email -d 'Display author email'
complete -c md-to-clip -o nfs -l no-file-save -d 'Whether to display conversion result in stdout instead of writing it to a file'
complete -c md-to-clip -o od -l output-directory -d 'Directory where conversion result will be written'
complete -c md-to-clip -o spc -l special-placeholder-config -d 'Config with special placeholders'
