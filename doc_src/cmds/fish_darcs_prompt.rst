fish_darcs_prompt - output Darcs information for use in a prompt
================================================================

Synopsis
--------

.. synopsis::

    fish_darcs_prompt

::

     function fish_prompt
          printf '%s' $PWD (fish_darcs_prompt) ' $ '
     end

Description
-----------

The ``fish_darcs_prompt`` function displays information about the current Darcs repository, if any.

`Darcs <https://darcs.net/>`_ must be installed.

The prompt always shows the VCS name (configurable via ``$fish_prompt_darcs_name``, default ``darcs``) as there are no branches/channels; it can be unset to remove it entirely. If the repository has unrecorded changes, status counts are shown for each change type:

- ``$fish_color_darcs_normal``, default ``green`` (the prompt label color)
- ``$fish_color_darcs_rebasing``, default ``yellow`` (used when a rebase is in progress)

The logo color can be referenced via ``$fish_darcs_logo_color`` for themes that want the exact Darcs branding::

    set fish_color_darcs_normal $fish_darcs_logo_color

The Darcs status symbols:

- ``$fish_prompt_darcs_status_added``, default ``+``
- ``$fish_prompt_darcs_status_modified``, default ``*``
- ``$fish_prompt_darcs_status_removed``, default ``-``
- ``$fish_prompt_darcs_status_moved``, default ``→``
- ``$fish_prompt_darcs_status_untracked``, default ``…``
- ``$fish_prompt_darcs_status_conflict``, default ``!``

Colors for these symbols can be set with the corresponding variables (``$fish_color_darcs_added`` & so on), which are unset by default so the terminal’s foreground color is used.

``$fish_prompt_darcs_status_order`` can be used to change the order the status symbols appear in. It defaults to ``added removed modified moved conflict untracked``.

See also :doc:`fish_vcs_prompt <fish_vcs_prompt>`, which will call all supported version control prompt functions, including Git, Mercurial, & Darcs.

Example
-------

A simple prompt that displays darcs info::

    function fish_prompt
        ...
        printf '%s %s$' $PWD (fish_darcs_prompt)
    end
