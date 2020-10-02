/*
 * sidebar.js
 * ~~~~~~~~~~
 *
 * This script makes the Sphinx sidebar collapsible and implements intelligent
 * scrolling. This is a slightly modified version of Sphinx's own sidebar.js.
 *
 * .sphinxsidebar contains .sphinxsidebarwrapper.  This script adds in
 * .sphixsidebar, after .sphinxsidebarwrapper, the #sidebarbutton used to
 * collapse and expand the sidebar.
 *
 * When the sidebar is collapsed the .sphinxsidebarwrapper is hidden and the
 * width of the sidebar and the margin-left of the document are decreased.
 * When the sidebar is expanded the opposite happens.  This script saves a
 * per-browser/per-session cookie used to remember the position of the sidebar
 * among the pages.  Once the browser is closed the cookie is deleted and the
 * position reset to the default (expanded).
 *
 * :copyright: Copyright 2007-2011 by the Sphinx team, see AUTHORS.
 * :license: BSD, see LICENSE for details.
 *
 */

$(function() {
  // global elements used by the functions.
  // the 'sidebarbutton' element is defined as global after its
  // creation, in the add_sidebar_button function
  var jwindow = $(window);
  var jdocument = $(document);
  var bodywrapper = $('.bodywrapper');
  var documentwrapper = $('.documentwrapper');
  var sidebar = $('.sphinxsidebar');
  var sidebarwrapper = $('.sphinxsidebarwrapper');

  // original margin-left of the bodywrapper and width of the sidebar
  // with the sidebar expanded
  var bw_margin_expanded = bodywrapper.css('margin-left');
  var ssb_width_expanded = sidebar.width();

  // margin-left of the bodywrapper and width of the sidebar
  // with the sidebar collapsed
  var bw_margin_collapsed = '.8em';
  var ssb_width_collapsed = '.8em';

  // colors used by the current theme
  var dark_color = '#AAAAAA';
  var light_color = '#CCCCCC';

  function get_viewport_height() {
    if (window.innerHeight)
      return window.innerHeight;
    else
      return jwindow.height();
  }

  function sidebar_is_collapsed() {
    return sidebarwrapper.is(':not(:visible)');
  }

  function toggle_sidebar() {
    if (sidebar_is_collapsed())
      expand_sidebar();
    else
      collapse_sidebar();
    // adjust the scrolling of the sidebar
    scroll_sidebar();
  }

  function collapse_sidebar() {
    sidebarwrapper.hide();
    sidebar.css('width', ssb_width_collapsed);
    bodywrapper.css('margin-left', bw_margin_collapsed);
    sidebarbutton.css({
        'margin-left': '0',
        'height': documentwrapper.height(),
        'border-radius': '5px'
    });
    sidebarbutton.find('span').text('»');
    sidebarbutton.attr('title', _('Expand sidebar'));
    document.cookie = 'sidebar=collapsed';
  }

  function expand_sidebar() {
    bodywrapper.css('margin-left', bw_margin_expanded);
    sidebar.css('width', ssb_width_expanded);
    sidebarwrapper.show();
    sidebarbutton.css({
        'margin-left': ssb_width_expanded-12,
        'height': Math.max(sidebarwrapper.height(), documentwrapper.height()),
        'border-radius': '0 5px 5px 0'
    });
    sidebarbutton.find('span').text('«');
    sidebarbutton.attr('title', _('Collapse sidebar'));
    //sidebarwrapper.css({'padding-top':
    //  Math.max(window.pageYOffset - sidebarwrapper.offset().top, 10)});
    document.cookie = 'sidebar=expanded';
  }

  function add_sidebar_button() {
    sidebarwrapper.css({
        'float': 'left',
        'margin-right': '0',
        'width': ssb_width_expanded - 28
    });
    // create the button
    sidebar.append(
      '<div id="sidebarbutton"><span>&laquo;</span></div>'
    );
    var sidebarbutton = $('#sidebarbutton');
    // find the height of the viewport to center the '<<' in the page
    var viewport_height = get_viewport_height();
    var sidebar_offset = sidebar.offset().top;
    var sidebar_height = Math.max(documentwrapper.height(), sidebar.height());
    sidebarbutton.find('span').css({
        'display': 'block',
        'position': 'fixed',
        'top': Math.min(viewport_height/2, sidebar_height/2 + sidebar_offset) - 10
    });

    sidebarbutton.click(toggle_sidebar);
    sidebarbutton.attr('title', _('Collapse sidebar'));
    sidebarbutton.css({
        'border-radius': '0 5px 5px 0',
        'color': '#444444',
        'background-color': '#CCCCCC',
        'font-size': '1.2em',
        'cursor': 'pointer',
        'height': sidebar_height,
        'padding-top': '1px',
        'padding-left': '1px',
        'margin-left': ssb_width_expanded - 12
    });

    sidebarbutton.hover(
      function () {
          $(this).css('background-color', dark_color);
      },
      function () {
          $(this).css('background-color', light_color);
      }
    );
  }

  function set_position_from_cookie() {
    if (!document.cookie)
      return;
    var items = document.cookie.split(';');
    for(var k=0; k<items.length; k++) {
      var key_val = items[k].split('=');
      var key = key_val[0];
      if (key == 'sidebar') {
        var value = key_val[1];
        if ((value == 'collapsed') && (!sidebar_is_collapsed()))
          collapse_sidebar();
        else if ((value == 'expanded') && (sidebar_is_collapsed()))
          expand_sidebar();
      }
    }
  }

  add_sidebar_button();
  var sidebarbutton = $('#sidebarbutton');
  set_position_from_cookie();


  /* intelligent scrolling */
  function scroll_sidebar() {
    var sidebar_height = sidebarwrapper.height();
    var viewport_height = get_viewport_height();
    var offset = sidebar.position()['top'];
    var wintop = jwindow.scrollTop();
    var winbot = wintop + viewport_height;
    var curtop = sidebarwrapper.position()['top'];
    var curbot = curtop + sidebar_height;
    // does sidebar fit in window?
    if (sidebar_height < viewport_height) {
      // yes: easy case -- always keep at the top
      sidebarwrapper.css('top', $u.min([$u.max([0, wintop - offset - 10]),
                            jdocument.height() - sidebar_height - 200]));
    }
    else {
      // no: only scroll if top/bottom edge of sidebar is at
      // top/bottom edge of window
      if (curtop > wintop && curbot > winbot) {
        sidebarwrapper.css('top', $u.max([wintop - offset - 10, 0]));
      }
      else if (curtop < wintop && curbot < winbot) {
        sidebarwrapper.css('top', $u.min([winbot - sidebar_height - offset - 20,
                              jdocument.height() - sidebar_height - 200]));
      }
    }
  }
  jwindow.scroll(scroll_sidebar);
});
