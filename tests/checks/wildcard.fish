# RUN: fish=%fish %fish %s

# Ensure that, if variable expansion results in multiple strings, matching
# globs expand and unmatched globs survive literally.
set -l oldpwd (pwd)
set dir (mktemp -d)
cd $dir
mkdir a
mkdir b
touch ./b/file.txt

set dirs ./a ./b
echo $dirs/*.txt # CHECK: ./a/*.txt ./b/file.txt

# Ordinary unmatched wildcards remain as literal arguments.
printf '<%s>\n' definitely-no-match-*.log
# CHECK: <definitely-no-match-*.log>
printf '<%s>\n' prefix-*.log
# CHECK: <prefix-*.log>
printf '<%s>\n' user@example.invalid:/some/path/*.log
# CHECK: <user@example.invalid:/some/path/*.log>
printf '<%s>\n' definitely-no-match-**/file.log
# CHECK: <definitely-no-match-**/file.log>
$fish --features no-qmark-noglob -c "printf '<%s>\n' definitely-no-match-?.log"
# CHECK: <definitely-no-match-?.log>

# Matching wildcards still expand normally.
touch a.log b.log
printf '<%s>\n' *.log
# CHECK: <a.log>
# CHECK: <b.log>

# Quoted and escaped wildcards remain literal.
printf '<%s>\n' 'quoted-*.log' escaped-\*.log
# CHECK: <quoted-*.log>
# CHECK: <escaped-*.log>

# A command substitution used to form an unmatched wildcard runs only once.
set -g wildcard_cmdsubst_marker $dir/cmdsubst-runs
function wildcard_cmdsubst_once
    echo run >>$wildcard_cmdsubst_marker
    echo cmdsubst
end
printf '<%s>\n' (wildcard_cmdsubst_once)-*.log
# CHECK: <cmdsubst-*.log>
string match -a run <$wildcard_cmdsubst_marker | count
# CHECK: 1
functions --erase wildcard_cmdsubst_once
set --erase wildcard_cmdsubst_marker

# Special nullglob contexts retain their empty-result behavior.
set unmatched definitely-no-match-*.log
count $unmatched
# CHECK: 0
count definitely-no-match-*.log
# CHECK: 0
path basename definitely-no-match-*.log | count
# CHECK: 0
for unmatched in definitely-no-match-*.log
    echo unexpected-for-value
end

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
