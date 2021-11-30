complete --command mh_diff --short-option h --long-option help --description 'Show help'

# Optional arguments
complete --command mh_diff --long-option kdiff3 --description 'Use KDiff3 to show a visual diff'
complete --command mh_diff --long-option allow-weird-names \
  --description 'Permit file names not ending in .slx'
