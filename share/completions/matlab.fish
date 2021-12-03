# Completion for: MATLAB R2021b

function __matlab_check_no_desktop_nodesktop_opts
  not __fish_seen_argument --old desktop --old nodesktop
  return $status
end

function __matlab_check_no_batch_r_opts
  not __fish_seen_argument --old batch --short r
  return $status
end

complete --command matlab --short-option h --long-option help --description 'Show help'

# Mode options
complete --command matlab --old-option desktop --condition '__matlab_check_no_desktop_nodesktop_opts' \
  --description 'Start without a controlling terminal'
complete --command matlab --old-option nodesktop --condition '__matlab_check_no_desktop_nodesktop_opts' \
  --description 'Run the JVM software without opening the desktop'
complete --command matlab --old-option nojvm \
  --description 'Start without the JVM software'

# Display options
complete --command matlab --old-option noFigureWindows \
  --description 'Disable the display of figure windows'
complete --command matlab --old-option nosplash \
  --description 'Don\'t display the splash screen during startup'
complete --command matlab --old-option nodisplay \
  --description 'Start the JVM software without starting desktop'
complete --command matlab --old-option display --exclusive \
  --description 'Send X commands to X Window Server display xDisp'

# Set initial working folder
complete --command matlab --old-option sd --require-parameter --description 'Set the folder'
complete --command matlab --old-option useStartupFolderPref \
  --description 'Change the folder to the Initial working folder preference'

# Debugging options
complete --command matlab --old-option logfile --require-parameter \
  --arguments '(__fish_complete_suffix .log)' --description 'Copy Command Window output into filename'
complete --command matlab --short-option n \
  --description 'Display the environment variables/arguments passed to the executable'
complete --command matlab --short-option e \
  --description 'Display all environment variables and their values to standard output'
complete --command matlab --short-option Dgdb --exclusive --description 'Start in debug mode'
complete --command matlab --short-option Dlldb --exclusive --description 'Start in debug mode'
complete --command matlab --short-option Ddbx --exclusive --description 'Start in debug mode'
complete --command matlab --old-option jdb --exclusive \
  --description 'Enable use of the Java debugger'
complete --command matlab --old-option debug \
  --description 'Display information for debugging X-based problems'

# Execute MATLAB script or function
complete --command matlab --old-option batch --exclusive \
  --condition '__matlab_check_no_batch_r_opts' \
  --description 'Execute script, statement, or function non-interactively'
complete --command matlab --short-option r --exclusive \
  --condition '__matlab_check_no_batch_r_opts' \
  --description 'Execute the statement'

# Use single computational thread
complete --command matlab --old-option singleCompThread \
  --description 'Limit to a single computational thread'

# Disable searching custom Java class path
complete --command matlab --old-option nouserjavapath \
  --description 'Disable use of javaclasspath.txt and javalibrarypath.txt files'

# OpenGL library options
complete --command matlab --old-option softwareopengl \
  --description 'Force to start with software OpenGL libraries'
complete --command matlab --old-option nosoftwareopengl \
  --description 'Disable auto-selection of OpenGL software'

# Specify license file
complete --command matlab --short-option c --require-parameter \
  --description 'Use the specified license file'
