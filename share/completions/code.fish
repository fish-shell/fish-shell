# Visual Studio Code

function __fish_complete_vscode_extensions
    command --quiet code; and code --list-extensions
end

complete -c code -s d -l diff -d 'Compare two files with each other'
complete -c code -s a -l add -d 'Add folder(s) to the last active window'
complete -c code -s g -l goto -x -d 'line and character position'
complete -c code -s n -l new-window -d 'Force to open a new window'
complete -c code -s r -l reuse-window -d 'Force to open a file or folder in an already opened window'
complete -c code -s w -l wait -d 'Wait for the files to be closed before returning'
complete -c code -l locale -x -d 'The locale to use (e.g. en-US or zh-TW)'
complete -c code -l user-data-dir -ra "(__fish_complete_directories)" -d 'Specifies the directory that user data is kept in'
complete -c code -s v -l version -d 'Print version'
complete -c code -s h -l help -d 'Print usage'
complete -c code -l folder-uri -d 'Opens a window with given folder uri(s)'
complete -c code -l file-uri -d 'Opens a window with given file uri(s)'

# Extensions management
complete -c code -l extensions-dir -d 'Set the root path for extensions'
complete -c code -l list-extensions -d 'List the installed extensions'
complete -c code -l show-versions -d 'Show versions of installed extensions' -n '__fish_seen_argument -l list-extensions'
complete -c code -l install-extension -xa "(__fish_complete_vscode_extensions)" -d 'Installs or updates the extension'
complete -c code -l force -n '__fish_seen_argument -l install-extension' -d 'Avoid prompts when installing'
complete -c code -l enable-proposed-api -xa "(__fish_complete_vscode_extensions)" -d 'Enables proposed API features for extensions'
complete -c code -l uninstall-extension -xa "(__fish_complete_vscode_extensions)" -d 'Uninstall extension'
complete -c code -l disable-extension -xa "(__fish_complete_vscode_extensions)" -d 'Disable extension(s)'
complete -c code -l disable-extensions -d 'Disable all installed extensions'

# Troubleshooting
complete -c code -l verbose -d 'Print verbose output (implies --wait)'
complete -c code -l log -a 'critical error warn info debug trace off' -d 'Log level to use (default: info)'
complete -c code -s s -l status -d 'Print process usage and diagnostics information'
complete -c code -s p -l prof-modules -d 'Capture JS module performance markers'
complete -c code -l prof-startup -d 'Run CPU profiler during startup'
complete -c code -l inspect-extensions -d 'Allow debugging and profiling of extensions'
complete -c code -l inspect-brk-extensions -x -d 'Allow debugging and profiling of extensions'
complete -c code -l disable-gpu -d 'Disable GPU hardware acceleration'
complete -c code -l upload-logs -d 'Uploads logs from current session to a secure endpoint'
complete -c code -l max-memory -x -d 'Max memory size for a window (Mbytes)'
