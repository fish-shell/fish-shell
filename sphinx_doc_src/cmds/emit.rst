.. _cmd-emit:

emit - Emit a generic event
===========================

Synopsis
--------

::

    emit EVENT_NAME [ARGUMENTS...]

Description
-----------

``emit`` emits, or fires, an event. Events are delivered to, or caught by, special functions called :ref:`event handlers <event>`. The arguments are passed to the event handlers as function arguments.


Example
-------

The following code first defines an event handler for the generic event named 'test_event', and then emits an event of that type.



::

    function event_test --on-event test_event
        echo event test: $argv
    end
    
    emit test_event something



Notes
-----

Note that events are only sent to the current fish process as there is no way to send events from one fish process to another.
