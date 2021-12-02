# Completion for: MATLAB R2021b
function __fish_matlabcheck_no_desktop_nodesktop_opts
    not __fish_seen_argument --old desktop --old nodesktop
    return $status
end

function __fish_matlabcheck_no_batch_r_opts
    not __fish_seen_argument --old batch --short r
    return $status
end

complete -c matlab -s h -l help -d 'Show help'

# Mode options
complete -c matlab -o desktop -n __fish_matlabcheck_no_desktop_nodesktop_opts -d 'Start without a controlling terminal'
complete -c matlab -o nodesktop -n __fish_matlabcheck_no_desktop_nodesktop_opts -d 'Run the JVM software without opening the desktop'
complete -c matlab -o nojvm -d 'Start without the JVM software'

# Display options
complete -c matlab -o noFigureWindows -d 'Disable the display of figure windows'
complete -c matlab -o nosplash -d 'Don\'t display the splash screen during startup'
complete -c matlab -o nodisplay -d 'Start the JVM software without starting desktop'
complete -c matlab -o display -x -d 'Send X commands to X Window Server display xDisp'

# Set initial working folder
complete -c matlab -o sd -r -d 'Set the folder'
complete -c matlab -o useStartupFolderPref -d 'Change the folder to the Initial working folder preference'

# Debugging options
complete -c matlab -o logfile -r -a '(__fish_complete_suffix .log)' -d 'Copy Command Window output into filename'
complete -c matlab -s n -d 'Display the environment variables/arguments passed to the executable'
complete -c matlab -s e -d 'Display all environment variables and their values to standard output'
complete -c matlab -s Dgdb -x -d 'Start in debug mode'
complete -c matlab -s Dlldb -x -d 'Start in debug mode'
complete -c matlab -s Ddbx -x -d 'Start in debug mode'
complete -c matlab -o jdb -x -d 'Enable use of the Java debugger'
complete -c matlab -o debug -d 'Display information for debugging X-based problems'

# Execute MATLAB script or function
complete -c matlab -o batch -x -n __fish_matlabcheck_no_batch_r_opts -d 'Execute script, statement, or function non-interactively'
complete -c matlab -s r -x -n __fish_matlabcheck_no_batch_r_opts -d 'Execute the statement'

# Use single computational thread
complete -c matlab -o singleCompThread -d 'Limit to a single computational thread'

# Disable searching custom Java class path
complete -c matlab -o nouserjavapath -d 'Disable use of javaclasspath.txt and javalibrarypath.txt files'

# OpenGL library options
complete -c matlab -o softwareopengl -d 'Force to start with software OpenGL libraries'
complete -c matlab -o nosoftwareopengl -d 'Disable auto-selection of OpenGL software'

# Specify license file
complete -c matlab -s c -r -d 'Use the specified license file'
