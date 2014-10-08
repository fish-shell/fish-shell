# name: Robbyrussell
# author: Bruno Ferreira Pinto

function fish_prompt

  if not set -q -g __fish_robbyrussell_functions_defined
    set -g __fish_robbyrussell_functions_defined
    function _git_branch_name
      echo (git symbolic-ref HEAD ^/dev/null | sed -e 's|^refs/heads/||')
    end

    function _num_branches
      set -l num (math (git for-each-ref refs/heads | wc -l) - 1)
      if [ $num != 0 ]; echo " $num"B; else; echo ''; end
    end

    function _has_stash
      git rev-parse --verify refs/stash >/dev/null 2>&1
      if [ $status = 0 ]; echo ' S'; else; echo ''; end
    end

    function _is_git_dirty
      echo (git status -s --ignore-submodules=dirty ^/dev/null)
    end
  end

  set -l cyan (set_color -o cyan)
  set -l yellow (set_color -o yellow)
  set -l red (set_color -o red)
  set -l blue (set_color -o blue)
  set -l normal (set_color normal)

  set -l arrow "$red➜ "
  set -l cwd $cyan(basename (prompt_pwd))

  if [ (_git_branch_name) ]
    set -l git_branch $red(_git_branch_name)
    set -l num_branches $red(_num_branches)
    set -l has_stash $red(_has_stash)
    set git_info "$blue git:($git_branch$num_branches$has_stash$blue)"

    if [ (_is_git_dirty) ]
      set -l dirty "$yellow ✗"
      set git_info "$git_info$dirty"
    end
  end

  echo -n -s $arrow ' '$cwd $git_info $normal ' '
end
