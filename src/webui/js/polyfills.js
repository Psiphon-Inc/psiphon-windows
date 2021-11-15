/*
https://github.com/twolfson/computedStyle
Copyright (c) 2013 Todd Wolfson

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/
(function () {
  // DEV: We don't use var but favor parameters since these play nicer with minification
function computedStyle(el, prop, getComputedStyle, style) {
  getComputedStyle = window.getComputedStyle;
  style =
      // If we have getComputedStyle
      getComputedStyle ?
        // Query it
        // TODO: From CSS-Query notes, we might need (node, null) for FF
        getComputedStyle(el) :

      // Otherwise, we are in IE and use currentStyle
        el.currentStyle;
  if (style) {
    return style
    [
      // Switch to camelCase for CSSOM
      // DEV: Grabbed from jQuery
      // https://github.com/jquery/jquery/blob/1.9-stable/src/css.js#L191-L194
      // https://github.com/jquery/jquery/blob/1.9-stable/src/core.js#L593-L597
      prop.replace(/-(\w)/gi, function (word, letter) {
        return letter.toUpperCase();
      })
    ];
  }
}

  window.computedStyle = computedStyle;
}());
