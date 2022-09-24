# Glob fish's source directory.
# This timing is bound to change if the repo does,
# so it's best to build two fishes, check out one version of the repo,
# and then run this script with both.
set -l dir (dirname (status current-filename))
for i in (seq 1 10)
    echo $dir/../../**
end
