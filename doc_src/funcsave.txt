\section funcsave funcsave - save the definition of a function to the user's autoload directory

\subsection funcsave-synopsis Synopsis
\fish{synopsis}
funcsave FUNCTION_NAME
\endfish

\subsection funcsave-description Description

`funcsave` saves the current definition of a function to a file in the fish configuration directory. This function will be automatically loaded by current and future fish sessions. This can be useful if you have interactively created a new function and wish to save it for later use.

Note that because fish loads functions on-demand, saved functions will not function as <a href="index.html#event">event handlers</a> until they are run or sourced otherwise. To activate an event handler for every new shell, add the function to your <a href="index.html#initialization">shell initialization file</a> instead of using `funcsave`.
