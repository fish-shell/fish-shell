#RUN: %fish --interactive %s
# ^ interactive so we can do `complete`
mkdir -p __fish_complete_directories/
cd __fish_complete_directories
mkdir -p test/buildroot
mkdir -p test/fish_expand_test
mkdir -p test/data/abc
mkdir -p test/data/abcd
touch test/data/af
touch test/data/abcdf
mkdir -p test/data/xy
mkdir -p test/data/xyz
touch test/data/xyf
touch test/data/xyzf
__fish_complete_directories test/z
# No match - no output!
__fish_complete_directories test/d
#CHECK: test/data/	Directory
__fish_complete_directories test/data
#CHECK: test/data/	Directory
__fish_complete_directories test/data/
#CHECK: test/data/abc/	Directory
#CHECK: test/data/abcd/	Directory
#CHECK: test/data/xy/	Directory
#CHECK: test/data/xyz/	Directory
__fish_complete_directories test/data/abc 'abc dirs'
#CHECK: test/data/abc/	abc dirs
#CHECK: test/data/abcd/	abc dirs

complete -c mydirs -l give-me-dir -a '(__fish_complete_directories)'
complete -C'mydirs --give-me-dir='
#CHECK: --give-me-dir=test/{{\t}}Directory
