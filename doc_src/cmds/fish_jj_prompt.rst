fish_jj_prompt - output Jujutsu information for use in a prompt
=================================================================

Synopsis
--------

.. synopsis::

    fish_jj_prompt

::

     function fish_prompt
          printf '%s' $PWD (fish_jj_prompt) ' $ '
     end

Description
-----------

The ``fish_jj_prompt`` function displays information about the current Jujutsu repository, if any.

It currently reports whether the working copy commit has conflicts.

If ``jj`` is not installed, or the current directory is not in a Jujutsu repository, ``fish_jj_prompt`` returns 1 and prints nothing.

See also :doc:`fish_vcs_prompt <fish_vcs_prompt>`, which will call all supported version control prompt functions, including Jujutsu, git and Mercurial.

Example
-------

A simple prompt that displays Jujutsu information::

    function fish_prompt
        printf '%s %s$' $PWD (fish_jj_prompt)
    end
