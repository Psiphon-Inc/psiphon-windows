## Design

Send messages from HTML to C via either "app:" (like, window.location="app:blah") or via page title (like, document.title="blah".
Send messages from C to HTML via new JS thing.


## Notes and discussion points

* Nothing is set in stone. For example, if consensus is against the left-side tabs, they can be changed. Et cetera.
* I have done no custom styling (colours, etc.) at all yet. Everything is default Bootstrap. Some styling notes in the TODO section (probably — haven’t go there).
  - This means that the UI looks quite web-y. Like, probably too web-y.
* XP-with-IE7 will probably be a somewhat degraded visual experience, but it will work fine.
* I realized yesterday that Bittorrent Sync also uses this IE-based web view thing. I attached screenshots of it. It mimics some chrome in its UI. And uses modals, which I’d like to avoid, but maybe blah.
* It feels like there should be more icons or images — it feels like it’s a lot of just text. I have added FontAwesome, so we have a lot of icons that can be easily used. You can see a few of them in the nav tabs.
* Status messages… something. Not a big permanently visible list box, though. I refuse.


## TODO

* Wire up feedback.
* Window size: Choose a starting size; fix it as minimum.
* Add i18n.
* Add “stopping” state.
* Styling: Square rounded corners to be more “modern”. Tone down gradients. (Both those also help with old IE consistency, since they aren’t supported.)
* Fix (or hide): Banner image is broken on IE7. Also, fixed-res bitmap banner is going to look shit on high-res screens. Need to change. UPDATE: Only broken because all data-URI images are broken on IE7.
* Clean up psiclient code.
* Put auto-connect back in. (Should make a registry setting to disable so I don’t have to alter code to stop it.)
* Add flags to egress region combo
* Add settings reset button?
* Fix: Delete key doesn't work in Settings (or in Feedback?)
* Will a long disconnect hang UI? Probably. But is that avoidable in JS, or just because Stop() blocks and is called from message queue handler?
* Bug? Psiphon doesn't consistently launch on Win7+IE9? Only in VM?
* Investigate/fix: Resize event handler causes a ton of constant CPU use on IE7 (and probably elsewhere).

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
