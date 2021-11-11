
## PsiCash

## Design

Communication in both directions is enabled by the [`mctrl`](https://github.com/Psiphon-Inc/mctrl) HTML control that we are using (and some changes we have made to it).

Messages are passed from JavaScript (frontend) to C (backend) via attempts to change location to `psi:...`. For example: `psi:start` or `psi:sendfeedback?<feedback_data>`. These URLs get processed by the backend code and handled appropriately.

Messages are passed from C (backend) to JavaScript (frontend) via called to exposed JS functions, such as `HtmlCtrlInterface_SetState` and `HtmlCtrlInterface_AddNotice`.


## Prerequisites

Install [Node](https://nodejs.org/). v6.x works, and v4.x will probably also work.

```
$ npm install -g grunt-cli
# In the webui directory...
$ npm ci
```

## Developing

This is what I use for quickly showing the UI in a browser. It just grabs the source from Bitbucket and returns it with appropriate Content-Types to display it.
https://bb.githack.com/psiphon/psiphon-circumvention-system/raw/default/Client/psiclient/webui/main.html

You can serve it locally with:
```
$ grunt serve
# then go to http://localhost:9000/main.html
```

To verify in an IE8 VM:
```
$ grunt
$ python -m http.server
# then go to http://hostvmip:8000/main-inline.html
```

The UI "web site" is in the [webui](https://bitbucket.org/psiphon/psiphon-circumvention-system/src/default/Client/psiclient/webui/) directory. [main.html](https://bitbucket.org/psiphon/psiphon-circumvention-system/src/default/Client/psiclient/webui/main.html) is the... main HTML. You'll see main-inline.html as well; that's the file that's actually used in the app and is generated from main.html by the [Gruntfile](https://bitbucket.org/psiphon/psiphon-circumvention-system/src/default/Client/psiclient/webui/Gruntfile.js).

The UI uses jQuery, lodash, Bootstrap, and a few other libs. App executable size is very important, so don't get crazy with new libs, but a tiny bit of bloat might be okay.

You'll notice that old versions of Bootstrap and jQuery are being used. We NEED TO SUPPORT INTERNET EXPLORER 7. Because we need to support Windows XP SP3 or whatever. MS Edge and new IE allow for mimicking old IE (F12 and then upper right of new pane), which is essential for testing. MS used to provide VMs with old OS and IE versions, but they're difficult to find now. Try out the existing UI githack link in pseudo-old IE, so you can get a feel for the still-usable degradation.

If we end up feeling that functional old IE support is untenable, then we may come to the decision to just hide the UI for them.

The UI logic is all in main.js. For example, [here's the code](https://bitbucket.org/psiphon/psiphon-circumvention-system/src/e36a48574442c739ea68e72f253c1eea73d5f559/Client/psiclient/webui/js/main.js?at=default&fileviewer=file-view-default#main.js-2456) that triggers the tunnel to stop (after the user clicks the button). It basically makes a request for `psi:stop`.

The other important part of this is the C++ code that talks to the UI. The [`HTMLUI_BeforeNavigateHandler`](https://bitbucket.org/psiphon/psiphon-circumvention-system/src/7b94cbff1644bf93edce4f7088f4b73d8d58e60e/Client/psiclient/psiclient.cpp?at=default&fileviewer=file-view-default#psiclient.cpp-769) function watches for our special command URLs and takes the appropriate action (like stopping the tunnel).

There are lots more UI communications helper functions here. In order to push info into the UI they call JS functions (via the HTML control).

(Copy-pasted from email.)

### Debugging in app

1. Probably put a `debugger;` line where you'd like to break in the JavaScript.
2. Make a non-minified build -- probably use `grunt serve`.
3. Make a debug build of the app and run it (but _not_ under the debugger).
4. In MSVC, in the "Debug" menu, click "Attach to Process...".
5. In that dialog, change the "Attach to" setting to "Script code".
6. Do whatever is needed to hit your `debugger` line. Or pause the debugger and set some breakpoints.


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


## Adding new egress regions

1. **`main.html`:** Search for `"GB"`. You will find two instances: The egress combo in the Settings tab, and the `AvailableEgressRegions` notice tester in the Debug tab. In both those sections, copy-paste one of the existing entries, and change the country code and country name to fit the new country.

2. **`_locales/en/messages.json`**: Search for `"United Kingdom"`. You will find the country name string table entries. Copy-paste one of them and modify the country code and name to fit the new country.

3. Run `grunt`, as indicated in the "Building" section.

4. Test `main.html` in your browser. Commit.


## TODO

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

NOTE: We no longer support IE7 (because we no longer support XP or Vista). That means that some of the limitations below can be eased.

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

* IE8 can't cope with the babel-ization for `for (let x of arr)`. Don't use it.


## Links to tools used

* [grunt-inline](https://github.com/chyingp/grunt-inline) ([npm](https://www.npmjs.com/package/grunt-inline))
* [Bootstrap v2](http://getbootstrap.com/2.3.2/index.html) (the last version that supports IE7)
* [IcoMoon](https://icomoon.io)
* [Less CSS](http://lesscss.org/)
* [jQuery](https://jquery.com/)
* [jQueryUI](http://jqueryui.com/)
* [i18next](http://i18next.com) ([github](https://github.com/i18next/i18next))
