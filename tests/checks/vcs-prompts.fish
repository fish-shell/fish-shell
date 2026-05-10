#RUN: %fish %s
#REQUIRES: command -v git

# Verify VCS prompts strip control characters from
# state files before showing them (same class as prompt_pwd / #12629).

set -l tmp (mktemp -d)

# --- fish_hg_prompt ---
# Stub `hg` to satisfy `command -sq hg` without requiring real Mercurial.
mkdir $tmp/bin
echo '#!/bin/sh' >$tmp/bin/hg
chmod +x $tmp/bin/hg
set -gx PATH $tmp/bin $PATH

mkdir -p $tmp/hgrepo/.hg
touch $tmp/hgrepo/.hg/dirstate
printf 'main\e]0;OHNO\a' >$tmp/hgrepo/.hg/branch

cd $tmp/hgrepo
fish_hg_prompt
echo
# CHECK: {{.*}} (main]0;OHNO)

# --- fish_git_prompt: rebase-merge state with escape sequence in head-name ---
mkdir $tmp/gitrepo
cd $tmp/gitrepo
git init -q
mkdir .git/rebase-merge
printf 'refs/heads/main\e]0;OHNO\a' >.git/rebase-merge/head-name
: >.git/rebase-merge/msgnum
: >.git/rebase-merge/end

fish_git_prompt
echo
# CHECK:  (main]0;OHNO|REBASE-m)
