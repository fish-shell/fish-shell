.. _cmd-fish_hg_prompt:

fish_hg_prompt - output Mercurial information for use in a prompt
=================================================================

Synopsis
--------

::

     function fish_prompt
          echo -n (pwd)(fish_hg_prompt) '$ '
     end

Description
-----------

The fish_hg_prompt function displays information about the current Mercurial repository, if any.

`Mercurial <https://www.mercurial-scm.org/>`_ (``hg``) must be installed.

By default, only the current branch is shown because ``hg status`` can take be slow on large repository. You can enable a more informative prompt by setting the variable ``$fish_prompt_hg_show_informative_status``, for example::

	set --universal fish_prompt_hg_show_informative_status

If you enabled the informative status, there are numerous customization options, which can be controlled with fish variables.

- ``$fish_color_hg_clean``, ``$fish_color_hg_modified`` and ``$fish_color_hg_dirty`` are colors used when the repository has the respective status.

Some colors for status symbols:

- ``$fish_color_hg_added``
- ``$fish_color_hg_renamed``
- ``$fish_color_hg_copied``
- ``$fish_color_hg_deleted``
- ``$fish_color_hg_untracked``
- ``$fish_color_hg_unmerged``

The status symbols themselves:

- ``$fish_prompt_hg_status_added``, default '✚'
- ``$fish_prompt_hg_status_modified``, default '*'
- ``$fish_prompt_hg_status_copied``, default '⇒'
- ``$fish_prompt_hg_status_deleted``, default '✖'
- ``$fish_prompt_hg_status_untracked``, default '?'
- ``$fish_prompt_hg_status_unmerged``, default '!'

Finally, ``$fish_prompt_hg_status_order``, which can be used to change the order the status symbols appear in. It defaults to ``added modified copied deleted untracked unmerged``.

See also :ref:`fish_vcs_prompt <cmd-fish_vcs_prompt>`, which will call all supported version control prompt functions, including git, Mercurial and Subversion.

Example
-------

A simple prompt that displays hg info::

    function fish_prompt
        ...
        set -g fish_prompt_hg_show_informative_status
        printf '%s %s$' $PWD (fish_hg_prompt)
    end


