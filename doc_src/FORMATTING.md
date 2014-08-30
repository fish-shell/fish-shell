# Formatting guide for fish docs

The fish documentation has been updated to support Doxygen 1.8+, and while the main benefit of this change is extensive markdown support, the addition of a fish lexicon and syntax filter, combined with semantic markup rules allows for automatic formatting enhancements across the HTML user_docs, the developer docs and the man pages.

Initially my motivation was to fix a problem with long options (see [Issue #1557](https://github.com/fish-shell/fish-shell/issues/1557) on GitHub), but as I worked on fixing the issue I realised there was an opportunity to simplify, reinforce and clarify the current documentation, hopefully making further contribution easier and cleaner, while allowing the documentation examples to presented more clearly with less author effort.

While the documentation is pretty robust to variations in the documentation source, adherence to the following style guide will help keep the already excellent documention in good shape moving forward.

## Line breaks and wrapping

Contrary to the rest of the fish source code, the documentation greatly benefits from the use of long lines and soft wrapping. It allows paragraphs to be treated as complete blocks by Doxygen, means that the semantic filter can see complete lines when deciding on how to apply syntax highlighting, and means that man pages will consistently wrap to the width of the users console in more advanced pagers, such as 'most'. 

## Doxygen special commands and aliases

While Markdown syntax forms the basis of the documentation content, there are some exceptions that require the use of Doxygen special commands. On the whole, Doxygen commands should be avoided, especially inline word formatting such as \\c as this would allow Doxygen to make unhelpful assumptions, such as converting double dashes (\--) to n-dashes (–).

### Structure: \\page, \\section and \\subsection

Use of Doxygen sections markers are important, as these determine what will be eventually output as a web page, man page or included in the developer docs. 

Currently the make process for the documentation is quite convoluted, but basically the HTML docs are produced from a single, compiled file, doc.h. This contains a number of \\page markers that produce the various pages used in the documentation. The format of a \\page mark is:

    \page universally_unique_page_id Page title

The source files that contain the page markers are currently:

- index.hdr.in: Core documentation
- commands.hdr.in: Individual commands
- tutorial.hdr: Tutorial
- design.hdr: Design document
- faq.hdr: Frequently Asked Questions
- license.hdr: Fish and 3rd party licences

Unless there is a _VERY_ good reason and developer consensus, new pages should never be added.

The rest of the documentation is structured using \\section and \\subsection markers. Most of the source files (listed above) contain their full content, the exception being commands, which are separated out into source text files in the doc_src directory. These files are concatenated into one file, so each one starts with a \\section declaration. The synopsis, description and examples (if present) are declared as \\subsections. The format of these marks is practically identical to the page mark.

    \section universally_unique_section_id Section title
    \subsection universally_unique_subsection_id Subsection title

Each page, section and subsection id _must_ be unique across the whole of the documentation, otherwise Doxygen will issue a warning.

### Semantic markup: the \\fish .. \\endfish block

While Doxygen has support for \\code..\\endcode blocks with enhanced markup and syntax colouring, it only understands the core Doxygen languages: C, C++, Objective C, Java, PHP, Python, Tcl and Fortran. To enhance Fish's syntax presentation, use the special \\fish..\\endfish blocks instead.

Text placed in this block will be parsed by Doxygen using the included lexicon filter (see lexicon_filter.in) and a Doxygen input filter. The filter is built during make so that it can pick up information on builtins, functions and shell commands mentioned in completions and apply markup to keywords found inside the \\fish block.

Basically, preformatted plain text inside the \\fish block is fed through the filter and is returned marked up so that Doxygen aliases can convert it back to a presentable form, according to the output document type.

For instance:

`echo hello world`

is transformed into:

`@cmnd{echo} @args{hello} @args{world}`

which is then transformed by Doxygen into an HTML version (`make user_doc`):

`<span class="command">echo</span> <span class="argument">hello</span> <span class="argument">world</span>`

A man page version (`make share/man`):

__echo__ hello world

And a simple HTML version for the developer docs (`make doc`) and the LATEX/PDF manual  (`make doc/refman.pdf`):

`echo hello world`

### Choosing a CLI style: using a \\fish{style} block

### Keyboard shortcuts: @key{} and @cursor_key{}

Graphical keyboard shortcuts can be defined using the following special commands. These allow for the different text requirements across the html and man pages. The HTML uses CSS to create a keyboard style, whereas the man page would display the key as text.

- `@key{lable}`
  Displays a key with a purely textual lable, such as: 'Tab', 'Page Up', 'Page Down', 'Home', 'End', 'F1', 'F19' and so on. 

- `@key{modifier,lable}`
  Displays a keystroke requiring the use of a 'modifier' key, such as 'Control-A', 'Shift-X', 'Alt-Tab' etc.

- `@key{modifier,entity,lable}`
  Displays a keystroke using a graphical entity, such as an arrow symbol for cursor key based shortcuts.

- `@cursor_key{entity,lable}`
  A special case for cursor keys, when no modifier is needed. i.e. `@cursor_key{&uarr;,up}` for the up arrow key.

Some useful Unicode/HTML5 entities:

- Up arrow: `&uarr;`
- Down arrow: `&darr;`
- Left arrow: `&larr;`
- Right arrow `&rarr;`
- Shift: `&#8679;`
- Tab: `&rarrb;`
- Mac option: `&#8997;`
- Mac command: `&#8984;`

## Markdown



### Backticks


### Lists

### Synopsis rules

### Prompts and cursor

#### Author: Mark Griffiths [@GitHub](https://github.com/MarkGriffiths)