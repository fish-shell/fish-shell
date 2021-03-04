#RUN: %fish -i %s
# Note: ^ this is interactive so we test interactive behavior,
# e.g. the fish_git_prompt variable handlers test `status is-interactive`.
#REQUIRES: command -v git

# Tests run from git (e.g. git rebase --exec 'ninja test'...) inherit a weird git environment.
# Ensure that no git environment variables are inherited.
for varname in (set -x | string match 'GIT_*' | string replace -r ' .*' '')
   set -e $varname
end

# Also ensure that git-core is not in $PATH, as this adds weird git commands like `git-add--interactive`.
set PATH (string match --invert '*git-core*' -- $PATH)

# Do some tests with `git` - completions are interesting,
# but prompts would also be possible.

set -l tmp (mktemp -d)

cd $tmp
git init >/dev/null 2>&1

# Commands and descriptions
# Note: We *can't* list all here because in addition to aliases,
# git also uses all commands in $PATH called `git-something` as custom commands,
# so this depends on system state!
complete -C'git ' | grep '^add'\t
# (note: actual tab character in the check here)
#CHECK: add	Add file contents to the index

touch foo

complete -C'git add '
#CHECK: foo	Untracked file

# Note: We can't rely on the initial branch because that might be
# "master", or it could be changed to something else in future!
git checkout -b newbranch >/dev/null 2>&1
fish_git_prompt
echo # the git prompt doesn't print a newline
#CHECK: (newbranch)

__fish_git_prompt_show_informative_status=1 fish_git_prompt
echo
#CHECK: (newbranch|…1)

# Confirm the mode changes back
fish_git_prompt
echo
#CHECK: (newbranch)
