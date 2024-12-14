if set -l rclone_version (rclone version | string match -rg 'rclone v?(.*)' | string split .) &&
        test "$rclone_version[1]" -lt 1 ||
        test "$rclone_version[1]" -eq 1 &&
        test "$rclone_version[2]" -le 62
  # version is definitely <= 1.62, adding a `-` would be an error
  rclone completion fish
else
  # For newer versions, this requires an `-`. Without a `-`, it would
  # try to write to /etc/completions/fish.
  # If we can't determine the version, assume a recent one. An error
  # is better than trying to write to /etc unexpectedly.
  rclone completion fish -
end 2>/dev/null | source

