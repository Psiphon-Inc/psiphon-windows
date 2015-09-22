## Design

Communication in both directions is enabled by the [`mctrl`](https://github.com/Psiphon-Inc/mctrl) HTML control that we are using (and some changes we have made to it).

Messages are passed from JavaScript (frontend) to C (backend) via attempts to change location to `psi:...`. For example: `psi:start` or `psi:sendfeedback?<feedback_data>`. These URLs get processed by the backend code and handled appropriately.

Messages are passed from C (backend) to JavaScript (frontend) via called to exposed JS functions, such as `HtmlCtrlInterface_SetState` and `HtmlCtrlInterface_AddNotice`.


## Prerequisites

```
$ npm install -g grunt-cli
# In the webui directory...
$ npm install .
```

## Building

```
# In the webui directory...
$ grunt
```

And then build the project in Visual Studio.


## i18n

Running `grunt` will process available languages and generate fake (development) translation files.

The fake translation files can also be generated manually:

```
$ cd utils
$ node fake-translations.js
```


## TODO

* Automation: Switch to VS2015.
* Update user guide on website.


## Longer-term TODO

* Allow copying selected text
* IE7 (and others?): Long text in pre/code inside error modal doesn't wrap and goes too far.
* Add title to banner image/link with text indicating that home page(s) will load.
  - Could get fancier and pass the URLs from win32 to JS and display them.
* Translate (win32) window title.
* HighDPI: Apply button wiggle shows through forgot-to-apply modal.
* HighDPI: Fix bottom-right slide-out notification. Gets truncated.
* Force Connect box to be centered on egress combo.
* Add functionality to export an encrypted diagnostic file to disk and tell user to attach it to an email to us.
  - Detect feedback send fail and tell user to do this.
  - Existing encrypted feedback email service should process these.
* Maybe add another, higher, log priority. This would be the only priority shown by default. Use it for, e.g., split tunnel messages. This would be for messages that the "might realistically want or need to see" rather than "maybe might help someone sometime". Would need a checkbox to also show the mid-priority logs.
* More elaborate connection animation?
* Maybe use larger/sharper flag sprites. Hard to find 64px images. Maybe from: http://www.browniesblog.com/A55CBC/blog.nsf/dx/responsive-css-sprites.html
* Better OOBE. Introductory screen. Basic help/info. Language.
* Create tool to extract English strings from HTML and re-populate messages.json
  - This will help to make sure all strings are in string table, and no defunct strings remain.
  - However: Remember that strings might be loaded from JS, not just HTML.
  - At the same time, maybe add `[html]` to all strings keys?
* Add upstream proxy verification at settings time. Prevent user from navigating away from settings page until verification is done. Verification will probably/certainly need to be done in C code -- so some async back-and-forth will be necessary.
* Split HTML into logical parts and use templating engine
* Split up JS in logical modules
* Ditto CSS
* Unit tests
* Will a long disconnect hang UI? Probably. But is that avoidable in JS, or just because Stop() blocks and is called from message queue handler?
* Maybe don't use custom font, since it looks even less like a native app? Or just use it for headings?
* Is logo jagged on some displays? Need dpp media queries to choose image rather than resize?
* Support [internationalized domain names](https://en.wikipedia.org/wiki/Internationalized_domain_name) for upstream proxy hostname (and anywhere else).
  - Right now the user will have to enter punycode directly or the hostname won't work.


## Technical and compatibility notes

* MSVC can't cope with paths in resource names. So anything that needs to be accessed via resource must be in root.

* IE7: Can't use data URIs. But IE8+ okay.

* IE7, IE8, IE9: Responsive-ness doesn't work.

* IE7, IE8, IE9: Navbar with `navbar-fixed-top` crashes view. The problem is setting `border-width` to 0, as in `border-width: 0 0 1px;`
  - Caused by this, in navbar.less:
    ```
    .navbar-fixed-top .navbar-inner,
    .navbar-static-top .navbar-inner {
      border-width: 0 0 1px;
    }
    ```

* IE<=9: Can't remove the `outline` around focused elements.

* When building Modernizr, do *not* include the HTML5 video check. This causes `MF.dll` (or the like) to load, which doesn't exist on the "N" version of Windows and triggers an error dialog. (The "N" versions don't have Media Player, IE, etc.)

* If we ever want to show home pages inside the app, we will want to disable script error messages. To do so, we'll probably need to implement the [`IOleCommandTarget`](https://msdn.microsoft.com/en-us/library/windows/desktop/ms683797%28v=vs.85%29.aspx) interface. [Ref1](https://groups.google.com/forum/#!topic/microsoft.public.inetsdk.programming.webbrowser_ctl/tE19dIF1uog), [ref2](https://support.microsoft.com/kb/261003).

* Avoid using `opacity` on text -- for example, to de-emphasize or disable text. On IE<=9, text with reduced `opacity` looks jagged and bad.

* IE7 has some horrible positioning and visibility bugs. If you can't figure out why something is invisible (or not) or is being positioned in weird way, read about [`hasLayout`](http://haslayout.net/haslayout) ([also](http://www.satzansatz.de/cssd/onhavinglayout.html)) and ["disappearing content"](http://www.positioniseverything.net/explorer/ienondisappearcontentbugPIE/index.htm).

* IE7: jQuery's `$().clone()` doesn't seem to work.

* ALL: `keyup` and `keydown` events are unreliable. Use `keypress` instead.
  - Related: `change` event doesn't fire for text boxes until focus is lost.

* IE7: Can't use `float:right` on an element that's a child/grandchild of a `postition:absolute` element. See the comment for `#settings-accordion` in `lteIE7.css` for details.


## Links to tools used

* [grunt-inline](https://github.com/chyingp/grunt-inline) ([npm](https://www.npmjs.com/package/grunt-inline))
* [Bootstrap v2](http://getbootstrap.com/2.3.2/index.html) (the last version that supports IE7)
* [IcoMoon](https://icomoon.io)
* [Less CSS](http://lesscss.org/)
* [jQuery](https://jquery.com/)
* [jQueryUI](http://jqueryui.com/)
* [i18next](http://i18next.com) ([github](https://github.com/i18next/i18next))
