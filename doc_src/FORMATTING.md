# Formatting guide for fish docs

The fish documentation has been updated to support Doxygen 1.8.7+, and while the main benefit of this change is extensive Markdown support, the addition of a fish lexicon and syntax filter, combined with semantic markup rules allows for automatic formatting enhancements across the HTML user_docs, the developer docs and the man pages.

Initially my motivation was to fix a problem with long options ([Issue #1557](https://github.com/fish-shell/fish-shell/issues/1557) on GitHub), but as I worked on fixing the issue I realised there was an opportunity to simplify, reinforce and clarify the current documentation, hopefully making further contribution easier and cleaner, while allowing the documentation examples to presented more clearly with less author effort.

While the documentation is pretty robust to variations in the documentation source, adherence to the following style guide will help keep the already excellent documention in good shape moving forward.

## Line breaks and wrapping

Contrary to the rest of the fish source code, the documentation greatly benefits from the use of long lines and soft wrapping. It allows paragraphs to be treated as complete blocks by Doxygen, means that the semantic filter can see complete lines when deciding on how to apply syntax highlighting, and means that man pages will consistently wrap to the width of the users console in advanced pagers, such as 'most'. 

## Doxygen special commands and aliases

While Markdown syntax forms the basis of the documentation content, there are some exceptions that require the use of Doxygen special commands. On the whole, Doxygen commands should be avoided, especially inline word formatting such as \\c as this would allow Doxygen to make unhelpful assumptions, such as converting double dashes (\--) to n-dashes (–).

### Structure: \\page, \\section and \\subsection

Use of Doxygen sections markers are important, as these determine what will be eventually output as a web page, man page or included in the developer docs. 

Currently the make process for the documentation is quite convoluted, but basically the HTML docs are produced from a single, compiled file, doc.h. This contains a number of \\page markers that produce the various pages used in the documentation. The format of a \\page mark is:

    \page universally_unique_page_id Page title

The source files that contain the page markers are currently:

- __index.hdr.in__: Core documentation
- __commands.hdr.in__: Individual commands
- __tutorial.hdr__: Tutorial
- __design.hdr__: Design document
- __faq.hdr__: Frequently Asked Questions
- __license.hdr__: Fish and 3rd party licences

Unless there is a _VERY_ good reason and developer consensus, new pages should never be added.

The rest of the documentation is structured using \\section and \\subsection markers. Most of the source files (listed above) contain their full content, the exception being commands, which are separated out into source text files in the doc_src directory. These files are concatenated into one file, so each one starts with a \\section declaration. The synopsis, description and examples (if present) are declared as \\subsections. The format of these marks is practically identical to the page mark.

    \section universally_unique_section_id Section title
    \subsection universally_unique_subsection_id Subsection title

Each page, section and subsection id _must_ be unique across the whole of the documentation, otherwise Doxygen will issue a warning.

### Semantic markup: the \\fish .. \\endfish block

While Doxygen has support for \\code..\\endcode blocks with enhanced markup and syntax colouring, it only understands the core Doxygen languages: C, C++, Objective C, Java, PHP, Python, Tcl and Fortran. To enhance Fish's syntax presentation, use the special \\fish..\\endfish blocks instead.

Text placed in this block will be parsed by Doxygen using the included lexicon filter (see lexicon_filter.in) as a Doxygen input filter. The filter is built during make so that it can pick up information on builtins, functions and shell commands mentioned in completions and apply markup to keywords found inside the \\fish block.

Basically, preformatted plain text inside the \\fish block is fed through the filter and is returned marked up so that Doxygen aliases can convert it back to a presentable form, according to the output document type.

For instance:

`echo hello world`

is transformed into:

`@cmnd{echo} @args{hello} @args{world}`

which is then transformed by Doxygen into an HTML version (`make doc`):

`<span class="command">echo</span> <span class="argument">hello</span> <span class="argument">world</span>`

A man page version (`make share/man`):

__echo__ hello world

And a simple HTML version for the developer docs (`make doc`) and the LATEX/PDF manual  (`make doc/refman.pdf`):

`echo hello world`

### Fonts

In older browsers, it was easy to set the fonts used for the three basic type styles (serif, sans-serif and monospace). Modern browsers have removed these options in their respective quests for simplification, assuming the content author will provide suitable styles for the content in the site's CSS, or the end user will provide overriding styles manually. Doxygen's default styling is very simple and most users will just accept this default.

I've tried to use a sensible set of fonts in the documentation's CSS based on 'good' terminal fonts and as a result the firt preference font used throughout the documentation is '[DejaVu](http://dejavu-fonts.org)'. The rationale behaind this is that while DejaVu is getting a little long in the tooth, it still provides the most complete support across serif, sans-serif and monospace styles (giving a well balanced feel and consistent [x-height](http://en.wikipedia.org/wiki/X-height)), has the widest support for extended Unicode characters and has a free, permissive licenses (though it's still incompatible with GPLv2, though arguably less so than the SIL Open Font license, though this is a moot point when using it solely in the docs).

#### Fonts inside \\fish blocks and \`backticks\`

As the point of these contructs is to make fish's syntax clearer to the user, it makes sense to mimic what the user will see in the console, therefore any content is formatted using the monospaced style, specifically monospaced fonts are chosen in the following order:

1. __DejaVu Sans Mono__: Explained above. [[&darr;](http://dejavu-fonts.org)]
2. __Source Code Pro__: Monospaced code font, part of Adobe's free Edge Web Fonts. [[&darr;](https://edgewebfonts.adobe.com)]
3. __Menlo__: Apple supplied variant of DejaVu.
4. __Ubuntu Mono__: Ubuntu Linux's default monospaced font. [[&darr;](http://font.ubuntu.com)]
5. __Consolas__: Modern Microsoft supplied console font.
6. __Monaco__: Apple supplied console font since 1984!
7. __Lucida Console__: Generic mono terminal font, standard in many OS's and distros.
8. __monospace__: Catchall style. Chooses default monospaced font, often Courier.
9. __fixed__: As above, more often used on mobile devices.

#### General Fonts

1. __DejaVu Sans__: As above.[[&darr;](http://dejavu-fonts.org)]
2. __Roboto__: Elegant Google free font and is Doxygen's default [[&darr;](http://www.google.com/fonts/specimen/Roboto)]
3. __Lucida Grande__: Default Apple OS X content font.
4. __Calibri__: Default Microsoft Office font (since 2007).
5. __Verdana__: Good general font found in a lot of OSs.
6. __Helvetica Neue__: Better spaced and balanced Helvetica/Arial variant.
7. __Helvetica__: Standard humanist typeface found almost everywhere.
8. __Arial__: Microsoft's Helvetica.
9. __sans-serif__: Catchall style. Chooses default sans-serif typeface, often Helvetica.

The ordering of the fonts is important as it's designed to allow the documentation to settle into a number of different identities according to the fonts available. If you have the complete DejaVu family installed, then the docs are presented using that, and if your Console is set up to use the same fonts, presentation will be completely consistent.

On OS X, with nothing extra installed, the docs will default to Menlo and Lucida Grande giving a Mac feel. Under Windows, it will default to using Consolas and Calibri on recent versions, giving a modern Windows style.

#### Other sources:

- [Font Squirrel](http://www.fontsquirrel.com): Good source of open source font packages.

### Choosing a CLI style: using a \\fish{style} block

By default, when output as HTML, a \\fish block uses syntax colouring suited to the style of the documentation rather than trying to mimic the terminal. The block has a light, bordered background and a colour scheme that 'suggests' what the user would see in a console.

Additional stying can be applied adding a style declaration:

    \fish{additional_style [another_style...]}
        ...
    \endfish

This will translate to classes applied to the `<div>` tag, like so:

    <div class="fish additional_style another_style">
        ...
    </div>

The various classes are defined in `doc_src/user_doc.css` and new style can be simply added

The documentation currently defines a couple of additional styles:

- __cli-dark__: Used in the _tutorial_ and _FAQ_ to simulate a dark background terminal, with fish's default colours (slightly tweaked for legibility in the browser).

- __synopsis__: A simple colour theme helpful for displaying the logical 'summary' of a command's syntax, options and structure.

## Markdown

Apart from the exceptions discussed above, the rest of the documentation now supports the use of Markdown. As such the use of Doxygen special commands for HTML tags is unnecessary.

There are a few exceptions and extensions to the Markdown [standard](http://daringfireball.net/projects/markdown/) that are documented in the Doxygen [documentation](http://www.stack.nl/~dimitri/doxygen/manual/markdown.html).

### \`Backticks\`

As is standard in Markdown and 'Github Flavoured Markdown' (GFM), backticks can be used to denote inline technical terms in the documentation, `like so`. In the documentation this will set the font to the monospaced 'console' typeface and will cause the enclosed term to stand out.

However, fenced code blocks using 4 spaces or 3 backticks (\`\`\`) should be avoided as Doxygen will interpret these as \\code blocks and try to apply standard syntax colouring, which doesn't work so well for fish examples. Use `\fish..\endfish` blocks instead.

### Lists

Standard Markdown list rules apply, but as Doxygen will collapse white space on output, combined with the use of long lines, it's a good idea to include an extra new line between long list items to assist future editing.

## Special cases

The following can be used in \\fish blocks to render some fish scenarios. These are mostly used in the tutorial when an interactive situation needs to be displayed.

### Custom formatting tags

- `{{` and `}}`: Required when wanting curly braces in regular expression example.
- `\\asis`: \\asis\{This text will not be parsed for fish markup.\}
- `\\bksl`: \\bksl\{Render the contents with a preceding backslash. Useful when presenting output.}
- `\\eror`: \\eror\{This would be shown as an error.\}
- `\\mtch`: \\mtch\{Matched\} items, such as tab completions.
- `\\outp`: \\outp\{This would be rendered as command/script output.\}
- `\\sgst`: auto\\sgst\{suggestion\}.
- `\\smtc`: Matched items \\smtc\{searched\} for, like grep results.
- `\\undr`: \\undr\{These words are underlined\}.

### Prompts and cursors

- `>_`: Display a basic prompt.
- `~>_`: Display a prompt with a the home directory as the current working directory.
- `___` (3 underscores): Display a cursor.


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

## Notes

### Doxygen

Tested on:
- Ubuntu 14.04 with Doxygen 1.8.8, built from [GitHub source](https://github.com/doxygen/doxygen.git).
- CentOS 6.5 with Doxygen 1.8.8, built from [GitHub source](https://github.com/doxygen/doxygen.git).
- Mac OS X 10.9 with Homebrew install Doxygen 1.8.7 and 1.8.8. 

Graphviz was also installed in all the above testing.

Doxygen 1.8.6 and lower do not have the \\htmlonly[block] directive which fixes a multitude of problems in the rendering of the docs. In Doxygen 1.8.7 the list of understood HTML entities was greatly increased. I tested earlier versions and many little issues returned.

As fish ships with pre-built documentation, I don't see this as an issue.

### Updated Configure/Makefile

- Tested on Ubuntu 14.04, CentOS 6.5 and Mac OS X 10.9.
- Makefile has GNU/BSD sed/grep detection.

### HTML output

- The output HTML is HTML5 compliant, but should quickly and elegantly degrade on older browsers without losing basic structure.
- The CSS avoids the use or browser specific extenstions (i.e. -webkit, -moz etc), using the W3C HTML5 standard instead.
- It's been tested in Chrome 37.0 and Firefox 32.0 on Mac OS X 10.9 (+Safari 7), Windows 8.1 (+Internet Explorer 11) and Ubuntu Desktop 14.04.
- My assumption is basically that if someone cares enough to want to install fish, they'll be keeping a browser current.

### Man page output

- Tested on Ubuntu 14.04, CentOS 6.5 and Mac OS X 10.9.
- Output is substantially cleaner.
- Tested in cat, less, more and most pagers using the following fish script:

```
function manTest --description 'Test manpage' --argument page
    set -l pager
    for i in $argv
        switch $i
            case "-l"
                set pager -P '/usr/bin/less -is'
            case "-m"
                set pager -P '/usr/bin/more -s'
            case "-c"
                set pager -P '/bin/cat'
        end
    end
    man $pager ~/Projects/OpenSource/fish-shell/share/man/man1/$page.1
end

# Assumes 'most' is the default system pager.
# NOT PORTABLE! Paths would be need to be updated on other systems.
```

### Developer docs and LATEX/PDF output

- HTML developer docs tested on Ubuntu 14.04, CentOS 6.5 and Mac OS X 10.9.
- LATEX/PDF reference manual tested on Mac OS X 10.9 using MacTEX. PDF production returns an error (due to Doxygen's use of an outdated 'float' package), but manual PDF output is ok.

### Future changes

1. The documentation creation process would be better if it could be modularised further and moved out of the makefile into a number of supporting scripts. This would allow both the automake and Xcode build processes to use the documentation scripts directly. 
2. Remove the Doxygen dependency entirely for the user documentation. This would be very acheivable now that the bulk of the documentation is in Markdown.
3. It would be useful to gauge what parts of the documentation are actually used by users. Judging by the amount of 'missing comment' errors during the developer docs build phase, this aspect of the docs has been rather neglected. If it is not longer used or useful, then this could change the future direction of the documentation and significantly streamline the process.

#### Author: Mark Griffiths [@GitHub](https://github.com/MarkGriffiths)
