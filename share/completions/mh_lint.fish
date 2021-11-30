function __mh_lint_check_no_html_json_opts
  not __fish_seen_argument --long html --long json
  return $status
end

complete --command mh_lint --short-option h --long-option help --description 'Show help'
complete --command mh_lint --short-option v --long-option version --description 'Show version'

# Optional arguments
complete --command mh_lint --long-option include-version \
  --description 'Include version number in output'
complete --command mh_lint --long-option entry-point --no-files --require-parameter \
  --description 'Set MATLAB entry point'
complete --command mh_lint --long-option single --description 'Do not use multi-threaded analysis'
complete --command mh_lint --long-option ignore-config \
  --description 'Ignore all miss_hit.cfg or .miss_hit files'
complete --command mh_lint --long-option input-encoding --no-files --require-parameter \
  --arguments '(__fish_list_python_encodings)' --description 'Change input encoding'

# Pragma options
complete --command mh_lint --long-option ignore-pragmas \
  --description 'Disable special treatment of MISS_HIT pragmas'
complete --command mh_lint --long-option ignore-justifications-with-tickets \
  --description 'Ignore any justifications that mention a ticket'

# Output options
complete --command mh_lint --long-option brief --description 'Don\'t show line-context on messages'
complete --command mh_lint --long-option html \
  --require-parameter --condition '__mh_lint_check_no_html_json_opts' \
  --description 'Write report to given file as HTML'
complete --command mh_lint --long-option json --require-parameter \
  --condition '__mh_lint_check_no_html_json_opts' \
  --description 'Produce JSON report'

# Language options
complete --command mh_lint --long-option octave --description 'Enable support for the Octave language'

# Debugging options
complete --command mh_lint --long-option debug-show-path \
  --description 'Show PATH used for function/class searching'
complete --command mh_lint --long-option debug-dump-tree --require-parameter \
  --description 'Dump text-based parse tree to given file'
