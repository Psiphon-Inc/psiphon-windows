Send messages from HTML to C via either "app:" (like, window.location="app:blah") or via page title (like, document.title="blah".
Send messages from C to HTML via new JS thing.

- IE7: Can't use data URIs. But IE8+ okay.
- IE7, IE8, IE9: Responsive-ness doesn't work.
- IE7, IE8, IE9: Navbar with `navbar-fixed-top` crashes view. The problem is setting `border-width` to 0, as in `border-width: 0 0 1px;`
  - Caused by this, in navbar.less:
    ```
    .navbar-fixed-top .navbar-inner,
    .navbar-static-top .navbar-inner {
      border-width: 0 0 1px;
    }
    ```

