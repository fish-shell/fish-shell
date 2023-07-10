Building the Documentation
==========================

The documentation can be built locally by running the following commands from the root source directory::

    mkdir build
    cd build
    cmake .. -DBUILD_DOCS=ON
    make doc

The documentation can then be viewed in your web browser by running::

    open user_doc/html/index.html
