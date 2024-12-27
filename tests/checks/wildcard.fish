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
echo $dirs/*.txt # CHECK: ./b/file.txt
echo */foo/
# CHECKERR: {{.*}}checks/wildcard.fish (line {{\d+}}): No matches for wildcard '*/foo/'. See `help wildcards-globbing`.
# CHECKERR: echo */foo/
# CHECKERR:      ^~~~~^


cd $oldpwd
rm -Rf $dir


# Verify that we can do wildcard expansion when we don't have read access to some path components.
# See #2099
set -l where ./fish_wildcard_permissions_test/noaccess/yesaccess
mkdir -p $where
chmod 300 (dirname $where) # no read permissions
mkdir -p $where
# "__env.fish" here to confirm ordering - #6593.
touch $where/alpha.txt $where/beta.txt $where/delta.txt $where/__env.fish
echo $where/*
#CHECK: ./fish_wildcard_permissions_test/noaccess/yesaccess/__env.fish ./fish_wildcard_permissions_test/noaccess/yesaccess/alpha.txt ./fish_wildcard_permissions_test/noaccess/yesaccess/beta.txt ./fish_wildcard_permissions_test/noaccess/yesaccess/delta.txt
chmod 700 (dirname $where) # so we can delete it
rm -rf ./fish_wildcard_permissions_test
