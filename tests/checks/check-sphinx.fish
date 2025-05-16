#RUN: fish_indent=%fish_indent fish=%fish %fish %s
#REQUIRES: command -v sphinx-build

set -g tmp_dir (mktemp -d)

function build_docs --argument-names builder
    set -l repo_root (status dirname)/../..
    set -l docsrc $repo_root/doc_src
    set -l doctree $tmp_dir/doctree
    set -l output_dir $tmp_dir/$builder
    # sphinx-build needs fish_indent in $PATH
    set -lxp PATH (path dirname $fish_indent)
    sphinx-build \
        -j auto \
        -q \
        -W \
        -E \
        -b $builder \
        -c $docsrc \
        -d $doctree \
        $docsrc \
        $output_dir
end

set -l success true
build_docs man || set -l success false
build_docs html || set -l success false

rm -r $tmp_dir

if test $success = false
    exit 1
end
