set -l rclone_version (rclone version | string match -rg 'rclone v(.*)' | string split .)
or return

# Yes, rclone's parsing here has changed, now they *require* a `-` argument
# where previously they required *not* having it.
if test "$rclone_version[1]" -gt 1; or test "$rclone_version[2]" -gt 62
    rclone completion fish - 2>/dev/null | source
else
    rclone completion fish 2>/dev/null | source
end
