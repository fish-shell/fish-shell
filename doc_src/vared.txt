\section vared vared - interactively edit the value of an environment variable

\subsection vared-synopsis Synopsis
\fish{synopsis}
vared VARIABLE_NAME
\endfish

\subsection vared-description Description

`vared` is used to interactively edit the value of an environment variable. Array variables as a whole can not be edited using `vared`, but individual array elements can.


\subsection vared-example Example

`vared PATH[3]` edits the third element of the PATH array
