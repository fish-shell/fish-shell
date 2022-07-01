complete -c optimus-manager -f
complete -c optimus-manager -l switch -d 'Set the GPU mode to MODE. You need to log out then log in to apply the change' -xa 'integrated nvidia hybrid'
complete -c optimus-manager -l temp-config -d 'Set a path to a temporary configuration file to use for the next reboot ONLY' -r
complete -c optimus-manager -s h -l help -d 'show this help message and exit'
complete -c optimus-manager -s v -l version -d 'Print version and exit'
complete -c optimus-manager -l status -d 'Print current status of optimus-manager'
complete -c optimus-manager -l print-mode -d 'Print the GPU mode that your current desktop session is running on'
complete -c optimus-manager -l print-next-mode -d 'Print the GPU mode that will be used the next time you log into your session'
complete -c optimus-manager -l print-startup -d 'Print the GPU mode that will be used on startup'
complete -c optimus-manager -l unset-temp-config -d 'Undo --temp-config (unset temp config path)'
complete -c optimus-manager -l no-confirm -d 'Do not ask for confirmation and skip all warnings when switching GPUs'
complete -c optimus-manager -l cleanup -d 'Remove auto-generated configuration files left over by the daemon'
