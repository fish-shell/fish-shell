fish_git_prompt - output git information for use in a prompt
============================================================

Description
-----------

The fish_git_prompt function can be used to display information about the current git repository, if any.

For obvious reasons, it requires having git installed.

There are numerous configuration options, either as fish variables or git config variables. If a git config variable is supported, it will be used if set, and the fish variable will only be used if it isn't.

- $__fish_git_prompt_show_informative_status or the git config option "bash.showInformativeStatus" can be set to enable the "informative" display, which will show a large amount of information - the number of untracked files, dirty files, unpushed/unpulled commits, etc... In large repositories, this can take a lot of time, so it is recommended to disable it there, via ``git config bash.showInformativeStatus false``.

- $__fish_git_prompt_showdirtystate or the git config option "bash.showDirtyState" can be set to show if the repository is "dirty", i.e. has uncommitted changes.

- $__fish_git_prompt_showuntrackedfiles or the git config option "bash.showUntrackedFiles" can be set to show if the repository has untracked files (that aren't ignored).

- $__fish_git_prompt_showupstream can be set to a number of values to determine how changes between HEAD and upstream are shown:

     verbose        show number of commits ahead/behind (+/-) upstream
     name           if verbose, then also show the upstream abbrev name
     informative    similar to verbose, but shows nothing when equal - this is the default if show_informative_status is set.
     git            always compare HEAD to @{upstream}
     svn            always compare HEAD to your SVN upstream
     none           disables (useful with show_informative_status)

- $__fish_git_prompt_showstashstate can be set to display the state of the stash.

- $__fish_git_prompt_shorten_branch_len can be set to the number of characters that the branch name will be shortened to.

- $__fish_git_prompt_describe_style can be set to a number of styles that describe the current HEAD:

     contains
     branch
     describe
     default

- $__fish_git_prompt_showcolorhints can be set to enable coloring for certain things.

A number of variables to set characters and color used to indicate things. Many of these have a different default if used with informative status enabled.

- $__fish_git_prompt_char_stateseparator
- $__fish_git_prompt_color
- $__fish_git_prompt_color_prefix
- $__fish_git_prompt_color_suffix
- $__fish_git_prompt_color_bare
- $__fish_git_prompt_color_merging

Some variables are only used in some modes, like when informative status is enabled (by setting $__fish_git_prompt_show_informative_status):
- $__fish_git_prompt_char_cleanstate
- $__fish_git_prompt_color_cleanstate

Variables used with showdirtystate:
- $__fish_git_prompt_char_dirtystate
- $__fish_git_prompt_char_invalidstate
- $__fish_git_prompt_char_stagedstate
- $__fish_git_prompt_color_dirtystate
- $__fish_git_prompt_color_invalidstate
- $__fish_git_prompt_color_stagedstate

Variables used with showstashstate:
- $__fish_git_prompt_char_stashstate
- $__fish_git_prompt_color_stashstate

Variables used with showuntrackedfiles:
- $__fish_git_prompt_char_untrackedfiles
- $__fish_git_prompt_color_untrackedfiles

Variables used with showupstream (also implied by informative status):
- $__fish_git_prompt_char_upstream_ahead
- $__fish_git_prompt_char_upstream_behind
- $__fish_git_prompt_char_upstream_diverged
- $__fish_git_prompt_char_upstream_equal
- $__fish_git_prompt_char_upstream_prefix
- $__fish_git_prompt_color_upstream

Colors used with showcolorhints:
- $__fish_git_prompt_color_branch
- $__fish_git_prompt_color_branch_detached
- $__fish_git_prompt_color_dirtystate
- $__fish_git_prompt_color_flags

Note that all colors can also have a corresponding "_done" color. E.g. $__fish_git_prompt_color_upstream_done, used right _after_ the upstream.

See also fish_vcs_prompt, which will call all supported vcs-prompt functions, including git, hg and svn.

Example
--------

A simple prompt that displays git info::

    function fish_prompt
        # ...
        set -g __fish_git_prompt_showupstream auto
        printf '%s %s$' $PWD (fish_git_prompt)
    end


