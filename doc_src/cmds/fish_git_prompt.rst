.. _cmd-fish_git_prompt:

fish_git_prompt - output git information for use in a prompt
============================================================

Synopsis
--------

.. synopsis::

    fish_git_prompt [FORMAT]

::

    function fish_prompt
         printf '%s' $PWD (fish_git_prompt) ' $ '
    end


Description
-----------

The ``fish_git_prompt`` function displays information about the current git repository, if any.

`Git <https://git-scm.com>`_ must be installed.

It is possible to modify the output format by passing an argument. The default value is ``" (%s)"``.

There are numerous customization options, which can be controlled with git options or fish variables. git options, where available, take precedence over the fish variable with the same function. git options can be set on a per-repository or global basis. git options can be set with the ``git config`` command, while fish variables can be set as usual with the :doc:`set <set>` command.

Boolean options (those which enable or disable something) understand "1", "yes" or "true" to mean true and every other value to mean false.

- ``$__fish_git_prompt_show_informative_status`` or the git option ``bash.showInformativeStatus`` can be set to 1, true or yes to enable the "informative" display, which will show a large amount of information - the number of dirty files, unpushed/unpulled commits, and more.
  In large repositories, this can take a lot of time, so you may wish to disable it in these repositories with  ``git config --local bash.showInformativeStatus false``. It also changes the characters the prompt uses to less plain ones (``✚`` instead of ``*`` for the dirty state for example) , and if you are only interested in that, set ``$__fish_git_prompt_use_informative_chars`` instead.

  Because counting untracked files requires a lot of time, the number of untracked files is only shown if enabled via ``$__fish_git_prompt_showuntrackedfiles`` or the git option ``bash.showUntrackedFiles``.

- ``$__fish_git_prompt_showdirtystate`` or the git option ``bash.showDirtyState`` can be set to 1, true or yes to show if the repository is "dirty", i.e. has uncommitted changes.

- ``$__fish_git_prompt_showuntrackedfiles`` or the git option ``bash.showUntrackedFiles`` can be set to 1, true or yes to show if the repository has untracked files (that aren't ignored).

- ``$__fish_git_prompt_showupstream`` can be set to a list of values to determine how changes between HEAD and upstream are shown:

     ``auto``
          summarize the difference between HEAD and its upstream
     ``verbose``
          show number of commits ahead/behind (+/-) upstream
     ``name``
          if verbose, then also show the upstream abbrev name
     ``informative``
          similar to verbose, but shows nothing when equal - this is the default if informative status is enabled.
     ``git``
          always compare HEAD to @{upstream}
     ``svn``
          always compare HEAD to your SVN upstream
     ``none``
          disables (useful with informative status)

- ``$__fish_git_prompt_showstashstate`` can be set to 1, true or yes to display the state of the stash.

- ``$__fish_git_prompt_shorten_branch_len`` can be set to the number of characters that the branch name will be shortened to.

- ``$__fish_git_prompt_describe_style`` can be set to one of the following styles to describe the current HEAD:

     ``contains``
         relative to newer annotated tag, such as ``(v1.6.3.2~35)``
     ``branch``
         relative to newer tag or branch, such as ``(master~4)``
     ``describe``
         relative to older annotated tag, such as ``(v1.6.3.1-13-gdd42c2f)``
     ``default``
         an exactly matching tag (``(develop)``)

     If none of these apply, the commit SHA shortened to 8 characters is used.

- ``$__fish_git_prompt_showcolorhints`` can be set to 1, true or yes to enable coloring for the branch name and status symbols.

A number of variables set characters and color used as indicators. Many of these have a different default if used with informative status enabled, or ``$__fish_git_prompt_use_informative_chars`` set. The usual default is given first, then the informative default (if it is different). If no default for the colors is given, they default to ``$__fish_git_prompt_color``.

- ``$__fish_git_prompt_char_stateseparator`` (' ', ``|``) - the character to be used between the state characters
- ``$__fish_git_prompt_color`` (no default)
- ``$__fish_git_prompt_color_prefix`` - the color of the ``(`` prefix
- ``$__fish_git_prompt_color_suffix`` - the color of the ``)`` suffix
- ``$__fish_git_prompt_color_bare`` - the color to use for a bare repository - one without a working tree
- ``$__fish_git_prompt_color_merging`` - the color when a merge/rebase/revert/bisect or cherry-pick is in progress

- ``$__fish_git_prompt_char_cleanstate`` (✔ in informative mode) - the character to be used when nothing else applies
- ``$__fish_git_prompt_color_cleanstate`` (no default)

Variables used with ``showdirtystate``:

- ``$__fish_git_prompt_char_dirtystate`` (`*`, ✚) - the number of "dirty" changes, i.e. unstaged files with changes
- ``$__fish_git_prompt_char_invalidstate`` (#, ✖) - the number of "unmerged" changes, e.g. additional changes to already added files
- ``$__fish_git_prompt_char_stagedstate`` (+, ●) - the number of staged files without additional changes
- ``$__fish_git_prompt_color_dirtystate`` (red with showcolorhints, same as color_flags otherwise)
- ``$__fish_git_prompt_color_invalidstate``
- ``$__fish_git_prompt_color_stagedstate`` (green with showcolorhints, color_flags otherwise)

Variables used with ``showstashstate``:

- ``$__fish_git_prompt_char_stashstate`` (``$``, ⚑)
- ``$__fish_git_prompt_color_stashstate`` (same as color_flags)

Variables used with ``showuntrackedfiles``:

- ``$__fish_git_prompt_char_untrackedfiles`` (%, …) - the symbol for untracked files
- ``$__fish_git_prompt_color_untrackedfiles`` (same as color_flags)

Variables used with ``showupstream`` (also implied by informative status):

- ``$__fish_git_prompt_char_upstream_ahead`` (>, ↑) - the character for the commits this repository is ahead of upstream
- ``$__fish_git_prompt_char_upstream_behind`` (<, ↓) - the character for the commits this repository is behind upstream
- ``$__fish_git_prompt_char_upstream_diverged`` (<>) - the symbol if this repository is both ahead and behind upstream
- ``$__fish_git_prompt_char_upstream_equal`` (=) - the symbol if this repo is equal to upstream
- ``$__fish_git_prompt_char_upstream_prefix`` ('')
- ``$__fish_git_prompt_color_upstream``

Colors used with ``showcolorhints``:

- ``$__fish_git_prompt_color_branch`` (green) - the color of the branch if nothing else applies
- ``$__fish_git_prompt_color_branch_detached`` (red) the color of the branch if it's detached (e.g. a commit is checked out)
- ``$__fish_git_prompt_color_branch_dirty`` (no default) the color of the branch if it's dirty and not detached
- ``$__fish_git_prompt_color_branch_staged`` (no default) the color of the branch if it just has something staged and is otherwise clean
- ``$__fish_git_prompt_color_flags`` (--bold blue) - the default color for dirty/staged/stashed/untracked state

Note that all colors can also have a corresponding ``_done`` color. For example, the contents of ``$__fish_git_prompt_color_upstream_done`` is printed right _after_ the upstream.

See also :doc:`fish_vcs_prompt <fish_vcs_prompt>`, which will call all supported version control prompt functions, including git, Mercurial and Subversion.

Example
--------

A simple prompt that displays git info::

    function fish_prompt
        # ...
        set -g __fish_git_prompt_showupstream auto
        printf '%s %s$' $PWD (fish_git_prompt)
    end
