fish_svn_prompt - output Subversion information for use in a prompt
===================================================================

Synopsis
--------

.. synopsis::

    fish_svn_prompt

::

     function fish_prompt
          printf '%s' $PWD (fish_svn_prompt) ' $ '
     end

Description
-----------

The fish_svn_prompt function displays information about the current Subversion repository, if any.

`Subversion <https://subversion.apache.org/>`_ (``svn``) must be installed.

There are numerous customization options, which can be controlled with fish variables.

- :envvar:`__fish_svn_prompt_color_revision`
    the colour of the revision number to display in the prompt
- :envvar:`__fish_svn_prompt_char_separator`
    the separator between status characters

A number of variables control the symbol ("display") and color ("color") for the different status indicators:

- :envvar:`__fish_svn_prompt_char_added_display`
- :envvar:`__fish_svn_prompt_char_added_color`
- :envvar:`__fish_svn_prompt_char_conflicted_display`
- :envvar:`__fish_svn_prompt_char_conflicted_color`
- :envvar:`__fish_svn_prompt_char_deleted_display`
- :envvar:`__fish_svn_prompt_char_deleted_color`
- :envvar:`__fish_svn_prompt_char_ignored_display`
- :envvar:`__fish_svn_prompt_char_ignored_color`
- :envvar:`__fish_svn_prompt_char_modified_display`
- :envvar:`__fish_svn_prompt_char_modified_color`
- :envvar:`__fish_svn_prompt_char_replaced_display`
- :envvar:`__fish_svn_prompt_char_replaced_color`
- :envvar:`__fish_svn_prompt_char_unversioned_external_display`
- :envvar:`__fish_svn_prompt_char_unversioned_external_color`
- :envvar:`__fish_svn_prompt_char_unversioned_display`
- :envvar:`__fish_svn_prompt_char_unversioned_color`
- :envvar:`__fish_svn_prompt_char_missing_display`
- :envvar:`__fish_svn_prompt_char_missing_color`
- :envvar:`__fish_svn_prompt_char_versioned_obstructed_display`
- :envvar:`__fish_svn_prompt_char_versioned_obstructed_color`
- :envvar:`__fish_svn_prompt_char_locked_display`
- :envvar:`__fish_svn_prompt_char_locked_color`
- :envvar:`__fish_svn_prompt_char_scheduled_display`
- :envvar:`__fish_svn_prompt_char_scheduled_color`
- :envvar:`__fish_svn_prompt_char_switched_display`
- :envvar:`__fish_svn_prompt_char_switched_color`
- :envvar:`__fish_svn_prompt_char_token_present_display`
- :envvar:`__fish_svn_prompt_char_token_present_color`
- :envvar:`__fish_svn_prompt_char_token_other_display`
- :envvar:`__fish_svn_prompt_char_token_other_color`
- :envvar:`__fish_svn_prompt_char_token_stolen_display`
- :envvar:`__fish_svn_prompt_char_token_stolen_color`
- :envvar:`__fish_svn_prompt_char_token_broken_display`
- :envvar:`__fish_svn_prompt_char_token_broken_color`

See also :doc:`fish_vcs_prompt <fish_vcs_prompt>`, which will call all supported version control prompt functions, including git, Mercurial and Subversion.

Example
-------

A simple prompt that displays svn info::

    function fish_prompt
        ...
        printf '%s %s$' $PWD (fish_svn_prompt)
    end


