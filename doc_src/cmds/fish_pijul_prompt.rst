fish_pijul_prompt - output Pijul information for use in a prompt
================================================================

Synopsis
--------

.. synopsis::

    fish_pijul_prompt

::

     function fish_prompt
          printf '%s' $PWD (fish_pijul_prompt) ' $ '
     end

Description
-----------

The ``fish_pijul_prompt`` function displays information about the current Pijul repository, if any.

`Pijul <https://pijul.org/>`_ & `jq <https://jqlang.github.io/jq/>`_ must both be installed.

The prompt always shows the current channel. If the repository has unrecorded changes, status counts are shown for each change type:

- ``$fish_color_pijul_normal``, default ``green`` (the prompt label color)
- ``$fish_color_pijul_conflict``, default ``red`` (the label color when unresolved conflicts are present)
- ``$fish_color_pijul_rebasing`` (reserved for a future rebase-in-progress display; setting it currently has no effect)

The Pijul status symbols:

- ``$fish_prompt_pijul_status_added``, default ``+``
- ``$fish_prompt_pijul_status_removed``, default ``-``
- ``$fish_prompt_pijul_status_modified``, default ``*``
- ``$fish_prompt_pijul_status_moved``, default ``⇄``
- ``$fish_prompt_pijul_status_restored``, default ``↺``
- ``$fish_prompt_pijul_status_resolved``, default ``✓``
- ``$fish_prompt_pijul_status_untracked``, default ``…``
- ``$fish_prompt_pijul_status_conflict``, default ``!``

Colors for these symbols can be set with the corresponding variables (``$fish_color_pijul_added`` & so on), which are unset by default so the terminal’s foreground color is used.

``$fish_prompt_pijul_status_order`` can be used to change the order the status symbols appear in. It defaults to ``added removed modified moved restored resolved conflict untracked``.

Conflicts
---------

Unresolved conflicts are shown as a ``!`` count of conflicted files. If the installed Pijul reports a ``conflicts`` field in ``pijul status --output-format json``, it is used directly & is authoritative.

Older Pijul versions have no such field, so the working copy is attempted to be scanned (ripgrep or falling back to find + grep) instead for Pijul’s conflict markers (``<<<<<<< N``, ``======= N``, ``>>>>>>> N``, where ``N`` is a conflict number). This scan can be slow on huge repositories/remotes, so it can be disabled by setting ``$fish_prompt_pijul_scan_conflicts`` to ``0`` or empty. Scanning is enabled by default.

Examples
--------

A simple prompt that displays pijul info::

    function fish_prompt
        ...
        printf '%s %s$' $PWD (fish_pijul_prompt)
    end
