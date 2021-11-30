complete --command mh_copyright --short-option h --long-option help --description 'Show help'
complete --command mh_copyright --short-option v --long-option version --description 'Show version'

# Optional arguments
complete --command mh_copyright --long-option include-version \
  --description 'Include version number in output'
complete --command mh_copyright --long-option entry-point --no-files --require-parameter \
  --description 'Set MATLAB entry point'
complete --command mh_copyright --long-option single --description 'Do not use multi-threaded analysis'
complete --command mh_copyright --long-option ignore-config \
  --description 'Ignore all miss_hit.cfg or .miss_hit files'
complete --command mh_copyright --long-option input-encoding --no-files --require-parameter \
  --arguments '(__fish_list_python_encodings)' --description 'Change input encoding'
complete --command mh_copyright --long-option process-slx \
  --description 'Style-check code inside SIMULINK models'

# Pragma options
complete --command mh_copyright --long-option ignore-pragmas \
  --description 'Disable special treatment of MISS_HIT pragmas'
complete --command mh_copyright --long-option ignore-justifications-with-tickets \
  --description 'Ignore any justifications that mention a ticket'

# Output options
complete --command mh_copyright --long-option brief --description 'Don\'t show line-context on messages'

# Language options
complete --command mh_copyright --long-option octave \
  --description 'Enable support for the Octave language'

# Debugging options
complete --command mh_copyright --long-option debug-show-path \
  --description 'Show PATH used for function/class searching'

# Copyright action
complete --command mh_copyright --long-option update-year \
  --description 'Update the end year in copyright notices for the primary copyright holder'
complete --command mh_copyright --long-option merge \
  --description 'Merge all non-3rd party copyright notices into one for the primary copyright holder'
complete --command mh_copyright --long-option change-entity --no-files --require-parameter \
  --description 'Change notices from the specified copyright holder into the primary copyright holder'
complete --command mh_copyright --long-option add-notice \
  --description 'Add a copyright notice to files that do not have one yet'
complete --command mh_copyright --long-option style --no-files --require-parameter --arguments 'dynamic\t"Pick the existing style"
c_first\t"Pick for MATLAB code"
c_last\t"Pick for Octave code"' --description ''

# Copyright data
complete --command mh_copyright --long-option year --description 'Use the current year'
complete --command mh_copyright --long-option primary-entity --no-files --require-parameter \
  --description 'Specify the primary copyright entity'
complete --command mh_copyright --long-option template-range --no-files --require-parameter \
  --description 'Specify text template to use for a copyright notice with a year range'
complete --command mh_copyright --long-option template --no-files --require-parameter \
  --description 'Specify text template to use for a copyright notice with a single year'
