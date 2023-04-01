.. _cmd-ghoti_vcs_prompt:

ghoti_vcs_prompt - output version control system information for use in a prompt
===============================================================================

Synopsis
--------

.. synopsis::

    ghoti_vcs_prompt

::

     function ghoti_prompt
          printf '%s' $PWD (ghoti_vcs_prompt) ' $ '
     end

Description
-----------

The ``ghoti_vcs_prompt`` function displays information about the current version control system (VCS) repository, if any.

It calls out to VCS-specific functions. The currently supported systems are:

- :doc:`ghoti_git_prompt <ghoti_git_prompt>`
- :doc:`ghoti_hg_prompt <ghoti_hg_prompt>`
- :doc:`ghoti_svn_prompt <ghoti_svn_prompt>`

If a VCS isn't installed, the respective function does nothing.

The Subversion prompt is disabled by default, because it's slow on large repositories. To enable it, modify ``ghoti_vcs_prompt`` to uncomment it. See :doc:`funced <funced>`.

For more information, see the documentation for each of the functions above.

Example
-------

A simple prompt that displays all known VCS info::

    function ghoti_prompt
        ...
        set -g __ghoti_git_prompt_showupstream auto
        printf '%s %s$' $PWD (ghoti_vcs_prompt)
    end
