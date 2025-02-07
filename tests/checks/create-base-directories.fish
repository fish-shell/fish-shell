#RUN: fish=%fish %fish %s

# Set a XDG_CONFIG_HOME with both pre-existing and non-existing directories.
set -l dir (mktemp -d)
mkdir -m 0755 $dir/old
set -gx XDG_CONFIG_HOME $dir/old/new

# Launch fish so it will create all missing directories.
$fish -c ''

# Check that existing directories kept their permissions, and new directories
# have the right permissions according to the XDG Base Directory Specification.
# Use command ls to turn off indicators in case of xattrs or similar.
command ls -ld $dir/old $dir/old/new $dir/old/new/fish | awk '{print $1}'
# CHECK: drwxr-xr-x
# CHECK: drwx------
# CHECK: drwx------
