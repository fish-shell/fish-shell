#RUN: %fish %s
#REQUIRES: command -v sphinx-build
#REQUIRES: python -c 'import sphinx_markdown_builder'

set -l build_script (status dirname)/../../build_tools/release-notes.sh
$build_script -q >relnotes.md
or echo "Failed to build Markdown release notes."
sed -n 1p relnotes.md
# CHECK: ## fish {{.*}} (released {{.*}})
