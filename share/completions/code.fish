# Visual Studio Code

function __fish_complete_vscode_extensions
    command --quiet code; and code --list-extensions
end

complete -c code -s d -l diff -d 'Compare two files with each other'
complete -c code -s m -l merge -d 'Perform a three-way merge'
complete -c code -s a -l add -d 'Add folder(s) to the last active window'
complete -c code -s g -l goto -r -d 'line and character position'
complete -c code -s n -l new-window -d 'Force to open a new window'
complete -c code -s r -l reuse-window -d 'Force to open a file or folder in an already opened window'
complete -c code -s w -l wait -d 'Wait for the files to be closed before returning'
complete -c code -l locale -x -d 'The locale to use (e.g. en-US or zh-TW)'
complete -c code -l user-data-dir -ra "(__fish_complete_directories)" -d 'Specifies the directory that user data is kept in'
complete -c code -l profile -d 'Opens the provided folder or workspace with the given profile'
complete -c code -s v -l version -d 'Print version'
complete -c code -s h -l help -d 'Print usage'

# Extensions management
complete -c code -l extensions-dir -r -d 'Set the root path for extensions'
complete -c code -l list-extensions -d 'List the installed extensions'
complete -c code -l show-versions -d 'Show versions of installed extensions' -n '__fish_seen_argument -l list-extensions'
complete -c code -l category -x -d 'Filters installed extensions by provided category' -n '__fish_seen_argument -l list-extensions'
complete -c code -l install-extension -ra "(__fish_complete_vscode_extensions)" -d 'Installs or updates the extension'
complete -c code -l force -n '__fish_seen_argument -l install-extension' -d 'Updates to the latest version'
complete -c code -l pre-release -n '__fish_seen_argument -l install-extension' -d 'Installs the pre-release version'
complete -c code -l update-extensions -d 'Update the installed extensions'
complete -c code -l enable-proposed-api -xa "(__fish_complete_vscode_extensions)" -d 'Enables proposed API features for extensions'
complete -c code -l uninstall-extension -xa "(__fish_complete_vscode_extensions)" -d 'Uninstall extension'
complete -c code -l disable-extension -xa "(__fish_complete_vscode_extensions)" -d 'Disable extension(s)'
complete -c code -l disable-extensions -d 'Disable all installed extensions'

# Troubleshooting
complete -c code -l verbose -d 'Print verbose output (implies --wait)'
complete -c code -l log -xa 'critical error warn info debug trace off' -d 'Log level to use (default: info)'
complete -c code -s s -l status -d 'Print process usage and diagnostics information'
complete -c code -l prof-startup -d 'Run CPU profiler during startup'
complete -c code -l sync -xa 'on off' -d 'Turn sync on or off'
complete -c code -l inspect-extensions -x -d 'Allow debugging and profiling of extensions'
complete -c code -l inspect-brk-extensions -x -d 'Allow debugging and profiling of extensions'
complete -c code -l disable-lcd-text -d 'Disable LCD font rendering'
complete -c code -l disable-gpu -d 'Disable GPU hardware acceleration'
complete -c code -l disable-chromium-sandbox -d 'Disable the Chromium sandbox environment'
complete -c code -l telemetry -d 'Shows all telemetry events which VS code collects'
