# RUN: %fish %s

# Ensure that, if variable expansion results in multiple strings
# and one of them fails a glob, that we don't fail the entire expansion.
set -l oldpwd (pwd)
set dir (mktemp -d)
cd $dir
mkdir a
mkdir b
touch ./b/file.txt

set dirs ./a ./b
echo $dirs/*.txt
# CHECK: ./b/file.txt

cd $oldpwd
rm -Rf $dir
