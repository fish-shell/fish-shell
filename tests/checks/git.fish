#RUN: %fish -i %s
# Note: ^ this is interactive so we test interactive behavior,
# e.g. the fish_git_prompt variable handlers test `status is-interactive`.
#REQUIRES: command -v git

# Tests run from git (e.g. git rebase --exec 'ninja test'...) inherit a weird git environment.
# Ensure that no git environment variables are inherited.
for varname in (set -x | string match 'GIT_*' | string replace -r ' .*' '')
    set -e $varname
end

set -gx GIT_CONFIG_GLOBAL /dev/null # No ~/.gitconfig. We could also override $HOME.
set -gx GIT_CONFIG_NOSYSTEM true # No /etc/gitconfig

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

# First set up a test alias - *before loading the completions*
git config --local alias.re 'restore --staged'

complete -C'git ' | grep '^add'\t
# (note: actual tab character in the check here)
#CHECK: add	Add file contents to the index

touch foo

complete -C'git add '
#CHECK: foo	Untracked file

git config alias.s status
complete 'git s --s'
# CHECK --short

# Note: We can't rely on the initial branch because that might be
# "master", or it could be changed to something else in future!
git checkout -b newbranch >/dev/null 2>&1
fish_git_prompt
echo # the git prompt doesn't print a newline
#CHECK: (newbranch)

set -g __fish_git_prompt_show_informative_status 1
fish_git_prompt
echo
#CHECK: (newbranch|…1)
set -e __fish_git_prompt_show_informative_status

# Confirm the mode changes back
fish_git_prompt
echo
#CHECK: (newbranch)

# (for some reason stagedstate is only shown with showdirtystate?)
set -g __fish_git_prompt_showdirtystate 1
git add foo
fish_git_prompt
echo
#CHECK: (newbranch +)

set -g __fish_git_prompt_showuntrackedfiles 1
touch bananan
fish_git_prompt
echo
#CHECK: (newbranch +%)

set -g __fish_git_prompt_status_order untrackedfiles stagedstate
fish_git_prompt
echo
#CHECK: (newbranch %+)

set -g __fish_git_prompt_status_order untrackedfiles
fish_git_prompt
echo
#CHECK: (newbranch %)

set -e __fish_git_prompt_showuntrackedfiles
set -e __fish_git_prompt_status_order

git -c user.email=banana@example.com -c user.name=banana commit -m foo >/dev/null
fish_git_prompt
echo
#CHECK: (newbranch)

echo "test" > foo
fish_git_prompt
echo
#CHECK: (newbranch *)

git add foo
fish_git_prompt
echo
#CHECK: (newbranch +)



# Turn on everything and verify we correctly ignore sus config files.
set -g __fish_git_prompt_status_order stagedstate invalidstate dirtystate untrackedfiles stashstate
set -g __fish_git_prompt_showdirtystate 1
set -g __fish_git_prompt_show_informative_status 1
set -g __fish_git_prompt_showuntrackedfiles 1
rm -Rf .git *
git init >/dev/null 2>&1
echo -n > ran.txt
git config core.fsmonitor 'echo fsmonitor >> ran.txt; false'
git config core.sshCommand 'echo sshCommand >> ran.txt; false'
git config diff.external 'echo diff >> ran.txt; false'
touch untracked_file
fish_git_prompt > /dev/null
cat ran.txt # should output nothing

test "$(complete -C'git re ')" = "$(complete -C'git restore --staged ')"
or begin
    echo -- Oops re completes unlike restore --staged
end
