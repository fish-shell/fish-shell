# Prints the current git branch, if any
function __fish_git_branch_prompt
  set gitdir (git rev-parse --git-dir 2>/dev/null)
  if [ -z $gitdir ]
    return 0
  end

  set branch (git-symbolic-ref HEAD 2>/dev/null| cut -d / -f 3)

  # check for rebase, bisect, etc
  # TODO

  # no branch, print hash of HEAD
  if [ -z $branch ]
    set branch (git log HEAD\^..HEAD --pretty=format:%h 2>/dev/null)
  end

  if [ ! -z $branch ]
    echo " ($branch) "
  end
end

