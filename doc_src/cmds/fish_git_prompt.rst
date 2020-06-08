.. _cmd-fish_git_prompt:

fish_git_prompt - output git information for use in a prompt
============================================================

Synopsis
--------

::

     function fish_prompt
          echo -n (pwd)(fish_git_prompt) '$ '
     end

Description
-----------

The ``fish_git_prompt`` function displays information about the current git repository, if any.

`Git <https://git-scm.com>`_ must be installed.

There are numerous customization options, which can be controlled with git options or fish variables. git options, where available, take precedence over the fish variable with the same function. git options can be set on a per-repository or global basis. git options can be set with the `git config` command, while fish variables can be set as usual with the :ref:`set <cmd-set>` command.

- ``$__fish_git_prompt_show_informative_status`` or the git option ``bash.showInformativeStatus`` can be set to enable the "informative" display, which will show a large amount of information - the number of untracked files, dirty files, unpushed/unpulled commits, and more. In large repositories, this can take a lot of time, so it you may wish to disable it in these repositories with  ``git config --local bash.showInformativeStatus false``.

- ``$__fish_git_prompt_showdirtystate`` or the git option ``bash.showDirtyState`` can be set to show if the repository is "dirty", i.e. has uncommitted changes.

- ``$__fish_git_prompt_showuntrackedfiles`` or the git option ``bash.showUntrackedFiles`` can be set to show if the repository has untracked files (that aren't ignored).

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

- ``$__fish_git_prompt_showstashstate`` can be set to display the state of the stash.

- ``$__fish_git_prompt_shorten_branch_len`` can be set to the number of characters that the branch name will be shortened to.

- ``$__fish_git_prompt_describe_style`` can be set to one of the following styles to describe the current HEAD:

     ``contains``
         relative to newer annotated tag, such as ``(v1.6.3.2~35)``
     ``branch``
         relative to newer tag or branch, such as ``(master~4)``
     ``describe``
         relative to older annotated tag, such as ``(v1.6.3.1-13-gdd42c2f)``
     ``default``
         exactly matching tag

- ``$__fish_git_prompt_showcolorhints`` can be set to enable coloring for the branch name and status symbols.

A number of variables set characters and color used as indicators. Many of these have a different default if used with informative status enabled, or ``$__fish_git_prompt_use_informative_chars`` set. The usual default is given first, then the informative default (if it is different). If no default for the colors is given, they default to ``$__fish_git_prompt_color``.

- ``$__fish_git_prompt_char_stateseparator`` (' ', `|`)
- ``$__fish_git_prompt_color`` ('')
- ``$__fish_git_prompt_color_prefix``
- ``$__fish_git_prompt_color_suffix``
- ``$__fish_git_prompt_color_bare``
- ``$__fish_git_prompt_color_merging``

Some variables are only used in some modes, like when informative status is enabled:

- ``$__fish_git_prompt_char_cleanstate`` (✔)
- ``$__fish_git_prompt_color_cleanstate``

Variables used with ``showdirtystate``:

- ``$__fish_git_prompt_char_dirtystate`` (`*`, ✚)
- ``$__fish_git_prompt_char_invalidstate`` (#, ✖)
- ``$__fish_git_prompt_char_stagedstate`` (+, ●)
- ``$__fish_git_prompt_color_dirtystate`` (red with showcolorhints, same as color_flags otherwise)
- ``$__fish_git_prompt_color_invalidstate``
- ``$__fish_git_prompt_color_stagedstate`` (green with showcolorhints, color_flags otherwise)

Variables used with ``showstashstate``:

- ``$__fish_git_prompt_char_stashstate`` (``$``, ⚑)
- ``$__fish_git_prompt_color_stashstate`` (same as color_flags)

Variables used with ``showuntrackedfiles``:

- ``$__fish_git_prompt_char_untrackedfiles`` (%, …)
- ``$__fish_git_prompt_color_untrackedfiles`` (same as color_flags)

Variables used with ``showupstream`` (also implied by informative status):

- ``$__fish_git_prompt_char_upstream_ahead`` (>, ↑)
- ``$__fish_git_prompt_char_upstream_behind`` (<, ↓)
- ``$__fish_git_prompt_char_upstream_diverged`` (<>)
- ``$__fish_git_prompt_char_upstream_equal`` (=)
- ``$__fish_git_prompt_char_upstream_prefix`` ('')
- ``$__fish_git_prompt_color_upstream``

Colors used with ``showcolorhints``:

- ``$__fish_git_prompt_color_branch`` (green)
- ``$__fish_git_prompt_color_branch_detached`` (red)
- ``$__fish_git_prompt_color_flags`` (--bold blue)

Note that all colors can also have a corresponding ``_done`` color. For example, the contents of ``$__fish_git_prompt_color_upstream_done`` is printed right _after_ the upstream.

See also :ref:`fish_vcs_prompt <cmd-fish_vcs_prompt>`, which will call all supported version control prompt functions, including git, Mercurial and Subversion.

Example
--------

A simple prompt that displays git info::

    function fish_prompt
        # ...
        set -g __fish_git_prompt_showupstream auto
        printf '%s %s$' $PWD (fish_git_prompt)
    end
