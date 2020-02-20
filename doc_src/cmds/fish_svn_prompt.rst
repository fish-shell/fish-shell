.. _cmd-fish_svn_prompt:

fish_svn_prompt - output Subversion information for use in a prompt
===================================================================

Synopsis
--------

::

     function fish_prompt
          echo -n (pwd)(fish_svn_prompt) '$ '
     end

Description
-----------

The fish_svn_prompt function displays information about the current Subversion repository, if any.

`Subversion <https://subversion.apache.org/>`_ (``svn``) must be installed.

There are numerous customization options, which can be controlled with fish variables.

- ``$__fish_svn_prompt_color_revision``
    the colour of the revision number to display in the prompt
- ``$__fish_svn_prompt_char_separator``
    the separator between status characters

A number of variables control the symbol ("display") and color ("color") for the different status indicators:

- ``$__fish_svn_prompt_char_added_display``
- ``$__fish_svn_prompt_char_added_color``
- ``$__fish_svn_prompt_char_conflicted_display``
- ``$__fish_svn_prompt_char_conflicted_color``
- ``$__fish_svn_prompt_char_deleted_display``
- ``$__fish_svn_prompt_char_deleted_color``
- ``$__fish_svn_prompt_char_ignored_display``
- ``$__fish_svn_prompt_char_ignored_color``
- ``$__fish_svn_prompt_char_modified_display``
- ``$__fish_svn_prompt_char_modified_color``
- ``$__fish_svn_prompt_char_replaced_display``
- ``$__fish_svn_prompt_char_replaced_color``
- ``$__fish_svn_prompt_char_unversioned_external_display``
- ``$__fish_svn_prompt_char_unversioned_external_color``
- ``$__fish_svn_prompt_char_unversioned_display``
- ``$__fish_svn_prompt_char_unversioned_color``
- ``$__fish_svn_prompt_char_missing_display``
- ``$__fish_svn_prompt_char_missing_color``
- ``$__fish_svn_prompt_char_versioned_obstructed_display``
- ``$__fish_svn_prompt_char_versioned_obstructed_color``
- ``$__fish_svn_prompt_char_locked_display``
- ``$__fish_svn_prompt_char_locked_color``
- ``$__fish_svn_prompt_char_scheduled_display``
- ``$__fish_svn_prompt_char_scheduled_color``
- ``$__fish_svn_prompt_char_switched_display``
- ``$__fish_svn_prompt_char_switched_color``
- ``$__fish_svn_prompt_char_token_present_display``
- ``$__fish_svn_prompt_char_token_present_color``
- ``$__fish_svn_prompt_char_token_other_display``
- ``$__fish_svn_prompt_char_token_other_color``
- ``$__fish_svn_prompt_char_token_stolen_display``
- ``$__fish_svn_prompt_char_token_stolen_color``
- ``$__fish_svn_prompt_char_token_broken_display``
- ``$__fish_svn_prompt_char_token_broken_color``

See also :ref:`fish_vcs_prompt <cmd-fish_vcs_prompt>`, which will call all supported version control prompt functions, including git, Mercurial and Subversion.

Example
-------

A simple prompt that displays svn info::

    function fish_prompt
        ...
        printf '%s %s$' $PWD (fish_svn_prompt)
    end


