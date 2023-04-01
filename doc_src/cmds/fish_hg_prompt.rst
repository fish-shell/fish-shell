.. _cmd-ghoti_hg_prompt:

ghoti_hg_prompt - output Mercurial information for use in a prompt
=================================================================

Synopsis
--------

.. synopsis::

    ghoti_hg_prompt

::

     function ghoti_prompt
          printf '%s' $PWD (ghoti_hg_prompt) ' $ '
     end

Description
-----------

The ghoti_hg_prompt function displays information about the current Mercurial repository, if any.

`Mercurial <https://www.mercurial-scm.org/>`_ (``hg``) must be installed.

By default, only the current branch is shown because ``hg status`` can be slow on a large repository. You can enable a more informative prompt by setting the variable ``$ghoti_prompt_hg_show_informative_status``, for example::

    set --universal ghoti_prompt_hg_show_informative_status

If you enabled the informative status, there are numerous customization options, which can be controlled with ghoti variables.

- ``$ghoti_color_hg_clean``, ``$ghoti_color_hg_modified`` and ``$ghoti_color_hg_dirty`` are colors used when the repository has the respective status.

Some colors for status symbols:

- ``$ghoti_color_hg_added``
- ``$ghoti_color_hg_renamed``
- ``$ghoti_color_hg_copied``
- ``$ghoti_color_hg_deleted``
- ``$ghoti_color_hg_untracked``
- ``$ghoti_color_hg_unmerged``

The status symbols themselves:

- ``$ghoti_prompt_hg_status_added``, default '✚'
- ``$ghoti_prompt_hg_status_modified``, default '*'
- ``$ghoti_prompt_hg_status_copied``, default '⇒'
- ``$ghoti_prompt_hg_status_deleted``, default '✖'
- ``$ghoti_prompt_hg_status_untracked``, default '?'
- ``$ghoti_prompt_hg_status_unmerged``, default '!'

Finally, ``$ghoti_prompt_hg_status_order``, which can be used to change the order the status symbols appear in. It defaults to ``added modified copied deleted untracked unmerged``.

See also :doc:`ghoti_vcs_prompt <ghoti_vcs_prompt>`, which will call all supported version control prompt functions, including git, Mercurial and Subversion.

Example
-------

A simple prompt that displays hg info::

    function ghoti_prompt
        ...
        set -g ghoti_prompt_hg_show_informative_status
        printf '%s %s$' $PWD (ghoti_hg_prompt)
    end


