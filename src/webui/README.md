## Design

Send messages from HTML to C via either "psi:" (like, window.location="psi:blah") or via page title (like, document.title="blah".
Send messages from C to HTML via new JS thing.


## i18n

To generate fake translation files from the English `messages.json`:

```
$ cd utils
$ node fake-translations.js
```


## Notes and discussion points

* It feels like there should be more icons or images -- it feels like it’s a lot of just text. I have added FontAwesome, so we have a lot of icons that can be easily used -- you can see a few of them in the nav tabs. But custom images would probably be good/better.


## TODO

* Automation: Switch to VS2013.
* Update user guide on website.
* Nicer looking, most usable connect button/state?
* More elaborate connection animation?


## Longer-term TODO

* Minimize to systray
* Maybe use larger/sharper flag sprites. Hard to find 64px images. Maybe from: http://www.browniesblog.com/A55CBC/blog.nsf/dx/responsive-css-sprites.html
* Links should show target on hover. (Tooltip? `title` attr?)
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


## Technical and compatibility notes

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


## Links to tools used

* [grunt-inline](https://github.com/chyingp/grunt-inline) ([npm](https://www.npmjs.com/package/grunt-inline))
* [Bootstrap v2](http://getbootstrap.com/2.3.2/index.html) (the last version that supports IE7)
* [FontAwesome v3](https://fortawesome.github.io/Font-Awesome/3.2.1/) (the last version that supports IE7)
* [Less CSS](http://lesscss.org/)
* [jQuery](https://jquery.com/)
* [jQueryUI](http://jqueryui.com/)
* [i18next](http://i18next.com) ([github](https://github.com/i18next/i18next))
