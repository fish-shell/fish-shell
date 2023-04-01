# RUN: %ghoti %s

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

cd $oldpwd
rm -Rf $dir


# Verify that we can do wildcard expansion when we don't have read access to some path components.
# See #2099
set -l where ./ghoti_wildcard_permissions_test/noaccess/yesaccess
mkdir -p $where
chmod 300 (dirname $where) # no read permissions
mkdir -p $where
# "__env.ghoti" here to confirm ordering - #6593.
touch $where/alpha.txt $where/beta.txt $where/delta.txt $where/__env.ghoti
echo $where/*
#CHECK: ./ghoti_wildcard_permissions_test/noaccess/yesaccess/__env.ghoti ./ghoti_wildcard_permissions_test/noaccess/yesaccess/alpha.txt ./ghoti_wildcard_permissions_test/noaccess/yesaccess/beta.txt ./ghoti_wildcard_permissions_test/noaccess/yesaccess/delta.txt
chmod 700 (dirname $where) # so we can delete it
rm -rf ./ghoti_wildcard_permissions_test
