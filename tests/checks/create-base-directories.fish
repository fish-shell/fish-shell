#RUN: %fish -C 'set -l fish %fish' %s

# Set a XDG_CONFIG_HOME with both pre-existing and non-existing directories.
set -l dir (mktemp -d)
mkdir -m 0755 $dir/old
set -gx XDG_CONFIG_HOME $dir/old/new

# Launch fish so it will create all missing directories.
$fish -c ''

# Check that existing directories kept their permissions, and new directories
# have the right permissions according to the XDG Base Directory Specification.
# depending on your environment, there might be a '.' printed at the end of the permissions
ls -ld $dir/old $dir/old/new $dir/old/new/fish | awk '{gsub(/\.$/, "", $1); print $1}'
# CHECK: drwxr-xr-x
# CHECK: drwx------
# CHECK: drwx------
