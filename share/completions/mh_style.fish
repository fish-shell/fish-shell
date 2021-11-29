complete --command mh_style --short-option h --long-option help --description 'Show help'
complete --command mh_style --short-option v --long-option version --description 'Show version'

# Optional arguments
complete --command mh_style --long-option include-version \
  --description 'Include version number in output'
complete --command mh_style --long-option entry-point --no-files --require-parameter \
  --description 'Set MATLAB entry point'
complete --command mh_style --long-option single --description 'Do not use multi-threaded analysis'
complete --command mh_style --long-option ignore-config \
  --description 'Ignore all miss_hit.cfg or .miss_hit files'
complete --command mh_style --long-option input-encoding --no-files --require-parameter \
  --arguments 'cp1252' --description 'Change input encoding'
complete --command mh_style --long-option fix \
  --description 'Automatically fix issues where the fix is obvious'
complete --command mh_style --long-option process-slx \
  --description 'Style-check code inside SIMULINK models'

# Pragma options
complete --command mh_style --long-option ignore-pragmas \
  --description 'Disable special treatment of MISS_HIT pragmas'
complete --command mh_style --long-option ignore-justifications-with-tickets \
  --description 'Ignore any justifications that mention a ticket'

# Output options
complete --command mh_style --long-option brief --description 'Don\'t show line-context on messages'
complete --command mh_style --long-option html \
  --require-parameter --description 'Write report to given file as HTML'
complete --command mh_style --long-option json --require-parameter --description 'Produce JSON report'
complete --command mh_style --long-option no-style --description 'Only show warnings and errors'

# Language options
complete --command mh_style --long-option octave --description 'Enable support for the Octave language'

# Debugging options
complete --command mh_style --long-option debug-show-path \
  --description 'Show PATH used for function/class searching'
complete --command mh_style --long-option debug-dump-tree --require-parameter \
  --description 'Dump text-based parse tree to given file'
complete --command mh_style --long-option debug-validate-links \
  --description 'Debug option to check AST links'

# Rule options
complete --command mh_style --long-option file_length --no-files --require-parameter \
  --description 'Specify maximum lines in a file'
complete --command mh_style --long-option line_length --no-files --require-parameter \
  --description 'Specify maximum characters per line'
complete --command mh_style --long-option tab_width --no-files --require-parameter \
  --description 'Specify tab-width'
complete --command mh_style --long-option copyright-entity --no-files --require-parameter \
  --description 'Add company name to check for in Copyright notices'
