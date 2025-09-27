#RUN: %fish %s
#REQUIRES: command -v sphinx-build
#REQUIRES: python -c 'import sphinx_markdown_builder'

set -l workspace_root (status dirname)/../..
if not test -e $workspace_root/.git
    return
end

$workspace_root/build_tools/release-notes.sh -q >?relnotes.md
or {
    echo "Failed to build Markdown release notes."
    return
}
sed -n 1p relnotes.md | grep -q '^## fish \S* (released .*)'
or echo "Unexpected changelog title"
