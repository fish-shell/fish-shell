# RUN: %fish %s
set -l tmp (mktemp -d)
cd $tmp
# resolve CDPATH (issue 6220)
begin
    mkdir -p x
    # CHECK: /{{.*}}/x
    cd x; pwd

    # CHECK: /{{.*}}/x
    set -lx CDPATH ..; cd x; pwd
end
rm -rf $tmp
