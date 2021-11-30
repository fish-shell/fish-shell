complete --command matlab --short-option h --long-option help --description 'Show help'

# Mode options
complete --command matlab --old-option desktop \
  --description 'Start MATLAB without a controlling terminal'
complete --command matlab --old-option nodesktop \
  --description 'Run the JVM software without opening the MATLAB desktop'
complete --command matlab --old-option nojvm \
  --description 'Start MATLAB without the JVM software'

# Display options
complete --command matlab --old-option noFigureWindows \
  --description 'Disable the display of figure windows in MATLAB'
complete --command matlab --old-option nosplash \
  --description 'Do not display the splash screen during startup'
complete --command matlab --old-option nodisplay \
  --description 'Start the JVM software without starting the MATLAB desktop'
complete --command matlab --old-option display --no-files --require-parameter \
  --description 'Send X commands to X Window Server display xDisp'

# Set initial working folder
complete --command matlab --old-option sd --description 'Set the MATLAB folder'
complete --command matlab --old-option useStartupFolderPref \
  --description 'Set the MATLAB folder to the value specified by the Initial working folder preference'

# Debugging options
complete --command matlab --old-option logfile --description 'Copy Command Window output into filename'
complete --command matlab --short-option n \
  --description 'Display the final values of environment variables/arguments passed to the executable'
complete --command matlab --short-option e \
  --description 'Display all environment variables and their values to standard output'
complete --command matlab --old-option Ddebugger --no-files --require-parameter \
  --description 'Start MATLAB in debug mode'
complete --command matlab --old-option jdb --no-files --require-parameter \
  --description 'Enable use of the Java debugger'
complete --command matlab --old-option debug \
  --description 'Display information for debugging X-based problems'

# Execute MATLAB script or function
complete --command matlab --old-option batch --no-files --require-parameter \
  --description 'Execute MATLAB script, statement, or function non-interactively'
complete --command matlab --short-option r --no-files --require-parameter \
  --description 'Execute the MATLAB statement'

# Use single computational thread
complete --command matlab --old-option singleCompThread \
  --description 'Limit MATLAB to a single computational thread'

# Disable searching custom Java class path
complete --command matlab --old-option nouserjavapath \
  --description 'Disable use of javaclasspath.txt and javalibrarypath.txt files'

# OpenGL library options
complete --command matlab --old-option softwareopengl \
  --description 'Force MATLAB to start with software OpenGL libraries'
complete --command matlab --old-option nosoftwareopengl \
  --description 'Disable auto-selection of OpenGL software'

# Specify license file
complete --command matlab --short-option c --require-parameter \
  --description 'Use the specified license file'
