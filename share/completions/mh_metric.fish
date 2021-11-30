function __mh_metric_check_no_text_html_json_opts
  not __fish_seen_argument --long text --long html --long json
  return $status
end

complete --command mh_metric --short-option h --long-option help --description 'Show help'
complete --command mh_metric --short-option v --long-option version --description 'Show version'

# Optional arguments
complete --command mh_metric --long-option include-version \
  --description 'Include version number in output'
complete --command mh_metric --long-option entry-point --no-files --require-parameter \
  --description 'Set MATLAB entry point'
complete --command mh_metric --long-option single --description 'Do not use multi-threaded analysis'
complete --command mh_metric --long-option ignore-config \
  --description 'Ignore all miss_hit.cfg or .miss_hit files'
complete --command mh_metric --long-option input-encoding --no-files --require-parameter \
  --arguments '(__fish_list_python_encodings)' --description 'Change input encoding'

# Pragma options
complete --command mh_metric --long-option ignore-pragmas \
  --description 'Disable special treatment of MISS_HIT pragmas'
complete --command mh_metric --long-option ignore-justifications-with-tickets \
  --description 'Ignore any justifications that mention a ticket'

# Output options
complete --command mh_metric --long-option brief \
  --description 'Don\'t show line-context on messages'
complete --command mh_metric --long-option worst-offenders --no-files --require-parameter \
  --description 'Produce a table of the worst offenders for each metric'
complete --command mh_metric --long-option ci \
  --description 'Do not print any metrics report, only notify about violations'
complete --command mh_metric --long-option text --require-parameter \
  --condition '__mh_metric_check_no_text_html_json_opts' \
  --description 'Print plain-text metrics summary to the given file'
complete --command mh_metric --long-option html --require-parameter \
  --condition '__mh_metric_check_no_text_html_json_opts' \
  --description 'Write HTML metrics report to the file'
complete --command mh_metric --long-option portable-html \
  --condition '__fish_seen_argument --long html' \
  --description 'Use assets/stylesheets from the web instead of the local MISS_HIT install'
complete --command mh_metric --long-option json --require-parameter \
  --condition '__mh_metric_check_no_text_html_json_opts' \
  --description 'Create JSON metrics report in the given file'

# Language options
complete --command mh_metric --long-option octave --description 'Enable support for the Octave language'

# Debugging options
complete --command mh_metric --long-option debug-show-path \
  --description 'Show PATH used for function/class searching'
