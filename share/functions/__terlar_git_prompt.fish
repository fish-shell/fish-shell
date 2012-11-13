set -gx fish_color_git_clean green
set -gx fish_color_git_staged yellow
set -gx fish_color_git_dirty red

set -gx fish_color_git_added green
set -gx fish_color_git_modified blue
set -gx fish_color_git_renamed magenta
set -gx fish_color_git_copied magenta
set -gx fish_color_git_deleted red
set -gx fish_color_git_untracked yellow
set -gx fish_color_git_unmerged red

function __terlar_git_prompt --description 'Write out the git prompt'
  set -l branch (git rev-parse --abbrev-ref HEAD ^/dev/null)
  if test -z $branch
    return
  end

  echo -n '|'

  set -l index (git status --porcelain ^/dev/null|cut -c 1-2|sort -u)

  if test -z "$index"
    set_color $fish_color_git_clean
    echo -n $branch'✓'
    set_color normal
    return
  end

  if printf '%s\n' $index|grep '^[ADRCM]' >/dev/null
    set_color $fish_color_git_staged
  else
    set_color $fish_color_git_dirty
  end

  echo -n $branch'⚡'

  for i in $index
    switch $i
      case 'A '               ; set added
      case 'M ' ' M'          ; set modified
      case 'R '               ; set renamed
      case 'C '               ; set copied
      case 'D ' ' D'          ; set deleted
      case '\?\?'             ; set untracked
      case 'U*' '*U' 'DD' 'AA'; set unmerged
    end
  end

  if set -q added
    set_color $fish_color_git_added
    echo -n '✚'
  end
  if set -q modified
    set_color $fish_color_git_modified
    echo -n '*'
  end
  if set -q renamed
    set_color $fish_color_git_renamed
    echo -n '➜'
  end
  if set -q copied
    set_color $fish_color_git_copied
    echo -n '⇒'
  end
  if set -q deleted
    set_color $fish_color_git_deleted
    echo -n '✖'
  end
  if set -q untracked
    set_color $fish_color_git_untracked
    echo -n '?'
  end
  if set -q unmerged
    set_color $fish_color_git_unmerged
    echo -n '!'
  end

  set_color normal
end
