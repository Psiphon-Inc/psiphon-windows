"use strict";

function _typeof(obj) { if (typeof Symbol === "function" && typeof Symbol.iterator === "symbol") { _typeof = function _typeof(obj) { return typeof obj; }; } else { _typeof = function _typeof(obj) { return obj && typeof Symbol === "function" && obj.constructor === Symbol && obj !== Symbol.prototype ? "symbol" : typeof obj; }; } return _typeof(obj); }

/*
 * Copyright (c) 2015, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
// ...Except third-party code below.
// Avoid `console` errors in browsers that lack a console.
(function () {
  var method;

  var noop = function noop() {};

  var methods = ['assert', 'clear', 'count', 'debug', 'dir', 'dirxml', 'error', 'exception', 'group', 'groupCollapsed', 'groupEnd', 'info', 'log', 'markTimeline', 'profile', 'profileEnd', 'table', 'time', 'timeEnd', 'timeline', 'timelineEnd', 'timeStamp', 'trace', 'warn'];
  var length = methods.length;
  var console = window.console = window.console || {};

  while (length--) {
    method = methods[length]; // Only stub undefined methods.

    if (!console[method]) {
      console[method] = noop;
    }
  }
})(); // Place any jQuery/helper plugins in here.

/*!
 * jQuery.scrollTo
 * Copyright (c) 2007 Ariel Flesler - aflesler ○ gmail • com | https://github.com/flesler
 * Licensed under MIT
 * https://github.com/flesler/jquery.scrollTo
 * @projectDescription Lightweight, cross-browser and highly customizable animated scrolling with jQuery
 * @author Ariel Flesler
 * @version 2.1.2
 */


;

(function (factory) {
  'use strict';

  if (typeof define === 'function' && define.amd) {
    // AMD
    define(['jquery'], factory);
  } else if (typeof module !== 'undefined' && module.exports) {
    // CommonJS
    module.exports = factory(require('jquery'));
  } else {
    // Global
    factory(jQuery);
  }
})(function ($) {
  'use strict';

  var $scrollTo = $.scrollTo = function (target, duration, settings) {
    return $(window).scrollTo(target, duration, settings);
  };

  $scrollTo.defaults = {
    axis: 'xy',
    duration: 0,
    limit: true
  };

  function isWin(elem) {
    return !elem.nodeName || $.inArray(elem.nodeName.toLowerCase(), ['iframe', '#document', 'html', 'body']) !== -1;
  }

  $.fn.scrollTo = function (target, duration, settings) {
    if (_typeof(duration) === 'object') {
      settings = duration;
      duration = 0;
    }

    if (typeof settings === 'function') {
      settings = {
        onAfter: settings
      };
    }

    if (target === 'max') {
      target = 9e9;
    }

    settings = $.extend({}, $scrollTo.defaults, settings); // Speed is still recognized for backwards compatibility

    duration = duration || settings.duration; // Make sure the settings are given right

    var queue = settings.queue && settings.axis.length > 1;

    if (queue) {
      // Let's keep the overall duration
      duration /= 2;
    }

    settings.offset = both(settings.offset);
    settings.over = both(settings.over);
    return this.each(function () {
      // Null target yields nothing, just like jQuery does
      if (target === null) return;
      var win = isWin(this),
          elem = win ? this.contentWindow || window : this,
          $elem = $(elem),
          targ = target,
          attr = {},
          toff;

      switch (_typeof(targ)) {
        // A number will pass the regex
        case 'number':
        case 'string':
          if (/^([+-]=?)?\d+(\.\d+)?(px|%)?$/.test(targ)) {
            targ = both(targ); // We are done

            break;
          } // Relative/Absolute selector


          targ = win ? $(targ) : $(targ, elem);

        /* falls through */

        case 'object':
          if (targ.length === 0) return; // DOMElement / jQuery

          if (targ.is || targ.style) {
            // Get the real position of the target
            toff = (targ = $(targ)).offset();
          }

      }

      var offset = $.isFunction(settings.offset) && settings.offset(elem, targ) || settings.offset;
      $.each(settings.axis.split(''), function (i, axis) {
        var Pos = axis === 'x' ? 'Left' : 'Top',
            pos = Pos.toLowerCase(),
            key = 'scroll' + Pos,
            prev = $elem[key](),
            max = $scrollTo.max(elem, axis);

        if (toff) {
          // jQuery / DOMElement
          attr[key] = toff[pos] + (win ? 0 : prev - $elem.offset()[pos]); // If it's a dom element, reduce the margin

          if (settings.margin) {
            attr[key] -= parseInt(targ.css('margin' + Pos), 10) || 0;
            attr[key] -= parseInt(targ.css('border' + Pos + 'Width'), 10) || 0;
          }

          attr[key] += offset[pos] || 0;

          if (settings.over[pos]) {
            // Scroll to a fraction of its width/height
            attr[key] += targ[axis === 'x' ? 'width' : 'height']() * settings.over[pos];
          }
        } else {
          var val = targ[pos]; // Handle percentage values

          attr[key] = val.slice && val.slice(-1) === '%' ? parseFloat(val) / 100 * max : val;
        } // Number or 'number'


        if (settings.limit && /^\d+$/.test(attr[key])) {
          // Check the limits
          attr[key] = attr[key] <= 0 ? 0 : Math.min(attr[key], max);
        } // Don't waste time animating, if there's no need.


        if (!i && settings.axis.length > 1) {
          if (prev === attr[key]) {
            // No animation needed
            attr = {};
          } else if (queue) {
            // Intermediate animation
            animate(settings.onAfterFirst); // Don't animate this axis again in the next iteration.

            attr = {};
          }
        }
      });
      animate(settings.onAfter);

      function animate(callback) {
        var opts = $.extend({}, settings, {
          // The queue setting conflicts with animate()
          // Force it to always be true
          queue: true,
          duration: duration,
          complete: callback && function () {
            callback.call(elem, targ, settings);
          }
        });
        $elem.animate(attr, opts);
      }
    });
  }; // Max scrolling position, works on quirks mode
  // It only fails (not too badly) on IE, quirks mode.


  $scrollTo.max = function (elem, axis) {
    var Dim = axis === 'x' ? 'Width' : 'Height',
        scroll = 'scroll' + Dim;
    if (!isWin(elem)) return elem[scroll] - $(elem)[Dim.toLowerCase()]();
    var size = 'client' + Dim,
        doc = elem.ownerDocument || elem.document,
        html = doc.documentElement,
        body = doc.body;
    return Math.max(html[scroll], body[scroll]) - Math.min(html[size], body[size]);
  };

  function both(val) {
    return $.isFunction(val) || $.isPlainObject(val) ? val : {
      top: val,
      left: val
    };
  } // Add special hooks so that window scroll properties can be animated


  $.Tween.propHooks.scrollLeft = $.Tween.propHooks.scrollTop = {
    get: function get(t) {
      return $(t.elem)[t.prop]();
    },
    set: function set(t) {
      var curr = this.get(t); // If interrupt is true and user scrolled, stop animating

      if (t.options.interrupt && t._last && t._last !== curr) {
        return $(t.elem).stop();
      }

      var next = Math.round(t.now); // Don't waste CPU
      // Browsers don't render floating point scroll

      if (curr !== next) {
        $(t.elem)[t.prop](next);
        t._last = this.get(t);
      }
    }
  }; // AMD requirement

  return $scrollTo;
}); // Smarter resize event
// http://www.paulirish.com/2009/throttled-smartresize-jquery-event-handler/


(function ($, sr) {
  // debouncing function from John Hann
  // http://unscriptable.com/index.php/2009/03/20/debouncing-javascript-methods/
  var debounce = function debounce(func, threshold, execAsap) {
    var timeout;
    return function debounced() {
      var obj = this,
          args = arguments;

      function delayed() {
        if (!execAsap) func.apply(obj, args);
        timeout = null;
      }

      if (timeout) clearTimeout(timeout);else if (execAsap) func.apply(obj, args);
      timeout = setTimeout(delayed, threshold || 100);
    };
  }; // smartresize


  jQuery.fn[sr] = function (fn) {
    return fn ? this.bind('resize', debounce(fn)) : this.trigger(sr);
  };
})(jQuery, 'smartresize');
/*
$.revealablePassword can be called on `<input type="password">` elements to add a reveal button.
`command` can be one of three values:
  - `init`: Initializes the reveal button for the given elements.
  - 'set': Sets the pasword field to the given `value`. Also resets the revealed state to hidden.
  - 'clear': Clears the password field. (Equivalent to `set('')`.)
*/


(function ($) {
  var dataKey = '$.revealablePassword';

  $.fn.revealablePassword = function () {
    var command = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 'init';
    var value = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : null;

    if (command === 'init') {
      this.each(function () {
        var $passwordInput = $(this); // We can't use $.clone here because the `type` attribute change won't get picked up by IE8.
        // The original password input will the canonical source of the value.
        // On IE8, e.outerHTML doesn't include quotes around all attributes.
        // Known issue: On IE8, when pasting via mouse, no event will fire on the first
        // change. This means that, for example, the "Apply Settings" button won't be enabled.
        // This seems to be unavoidable. The workaround is to change focus after the mouse-paste.

        var $revealedText = $($passwordInput.prop('outerHTML').replace(/type="?password"?/, 'type="text"')).prop('id', $passwordInput.prop('id') + '-plaintext').addClass('hidden').on('propertychange input change keydown keyup keypress blur', function (e) {
          if (e.type === 'propertychange' && e.originalEvent.propertyName !== 'value') {
            return;
          } // For some reason that I cannot fathom, on IE8 this handler fires for $passwordInput as well as $revealedText


          if (e.currentTarget.id !== $revealedText.prop('id')) {
            return;
          }

          $passwordInput.val($(this).val());
          $passwordInput.trigger('change');
        }); // For accessibility reasons, we want the clickable eye to be in the tab order, so
        // we're using a real control rather than a `<i>` element.
        // `type="button"` is necessary to prevent the button from being the default for
        // the form (that is, activated by pressing Enter).

        var $revealEye = $('<button type="button" class="password-input-reveal icon-eye1 btn btn-link"></button>');
        $passwordInput.after($revealedText, $revealEye);

        var reveal = function reveal() {
          $revealedText.val($passwordInput.val()).removeClass('hidden');
          $passwordInput.addClass('hidden');
          $revealEye.removeClass('icon-eye1').addClass('icon-eye-blocked');
        };

        var unreveal = function unreveal() {
          $revealedText.addClass('hidden');
          $passwordInput.val($revealedText.val()).removeClass('hidden');
          $revealEye.removeClass('icon-eye-blocked').addClass('icon-eye1');
        };

        $revealEye.on('click', function (e) {
          e.preventDefault();

          if ($revealedText.hasClass('hidden')) {
            reveal();
          } else {
            unreveal();
          }
        });
        $passwordInput.data(dataKey, {
          unreveal: unreveal
        });
      });
    } else if (command === 'set') {
      this.each(function () {
        var $passwordInput = $(this);
        var data = $passwordInput.data(dataKey);
        data.unreveal();
        $passwordInput.val(value);
        $passwordInput.trigger('change');
      });
    } else if (command === 'clear') {
      this.revealablePassword('set', '');
    }

    return this;
  };
})(jQuery);
/*
Datastore example:

const store = new Datastore({
   obj: {
     nestedObj: {
       a: 'a',
       b: 'b'
     }
   },
   integer: 123,
   array: [1, 2, 3]
});

const unsub = store.subscribe('obj.nestedObj.a', (path, data, type) => {
   console.log('deeper', path, data, type);
   // unsubscribe
   unsub();
});

store.subscribe('obj.nestedObj', (path, data, type) => {
   console.log('shallower', path, data, type);
});

store.set('obj.nestedObj.c', 'c'); // add a new property
// >> shallower obj.nestedObj {a: "a", b: "b", c: "c"} change

store.set('obj.nestedObj.a', 'aa'); // will trigger both subscribers
// >> deeper obj.nestedObj.a aa change
// >> shallower obj.nestedObj {a: "aa", b: "b", c: "c"} change

// Does nothing, as the value is unchanged.
store.set('obj.nestedObj.a', 'aa');

// Value is still unchanged, but we're specifying a `false` equality operator, so the
// subscriptions will fire.
store.set('obj.nestedObj.a', 'aa', false);
// >> shallower obj.nestedObj {a: "aa", b: "b", c: "c"} change
// The deeper subscription already unsubscribed itself.

// The underlying data can be accessed directly. But don't modify it, unless you're trying
// to bypass the immutability and pubsub!
console.dir(store.data);

This example has been left out of @example tags so as to not flood the pop-up help.
*/

/**
 * A state and data store featuring immutability of the underlying object and ability to
 * subscribe to changes.
 * For immutability this library is required: https://github.com/mariocasciaro/object-path-immutable
 * For object-path subscribing and access, lodash is required: https://lodash.com/docs/3.10.1#get
 * @param {object} data The initial data structure. For ease of code understandability,
 *                      this should be approximately the structure the data will always have.
 * @param {?string} name The name of the store; used only for help logging and debugging.
 */


function Datastore(data, name) {
  /**
   * The name of the datastore
   * @type {string}
   * @readonly
   */
  this.name = name || "Datastore-".concat(Math.random());
  /**
   * The underlying data object. This can be read directly to access the data, but should
   * NOT be modified (unless you're specifically trying to avoid immutability and
   * subscriber notifications).
   * @type {object}
   */

  this.data = data;
  /**
   * [{path: "a.b.c", func: callback, key: unique},...]
   */

  this._subscribers = [];
  /**
   * The possible event types.
   * TODO: Are types even necessary? Right now we only use 'change'.
   * @enum {string}
   * @readonly
   */

  this.EventTypes = {
    change: 'change'
  };
  /**
   * Send the specified change to the subscribers.
   * @param {string} path The object-path of the change.
   * @param {EventTypes} type The type of the change.
   */

  this._dispatch = function (path, type) {
    var _this = this;

    // this.data might change (immutably) during the timeout, so capture the current
    // object now.
    var data = this.data; // Ensure this is asynchronous

    setTimeout(function () {
      // When looking for subscribers, we look at prefixes of paths, as whole sub-trees
      // can be subscribed to.
      var subs = _.filter(_this._subscribers, function (s) {
        // We append '.' because we want "a.b" to match "a.b.c" but not "a.banana".
        return _.startsWith(path + '.', s.path + '.');
      });

      for (var i = 0; i < subs.length; i++) {
        var sub = subs[i]; // The amount of data (tree or specific value) that a subscriber gets depends on
        // what they are subscribed to.

        sub.func(sub.path, _.get(data, sub.path), type);
      }
    }, 0);
  };
  /**
   * Returns data wrapped with https://github.com/mariocasciaro/object-path-immutable
   * @param {object} data If falsy, this.data will be wrapped.
   * @returns {objectPathImmutable}
   */


  this._imm = function (data) {
    if (!_.isUndefined(data)) {
      return objectPathImmutable(data);
    }

    return objectPathImmutable(this.data);
  };
  /**
   * Check if val1 and val2 are equal, using equality or _.isEqual if not supplied.
   * @param {any} val1
   * @param {any} val2
   * @param {?(function|boolean)} equality The equality comparison function. If not
   *    supplied, _.isEqual (deep equality) will be used. If `false`, no equality comparison
   *    will be done. If supplied, it must take val1 and val2 and return a boolean.
   * @returns {boolean}
   */


  this._eq = function (val1, val2, equality) {
    if (equality === false) {
      // Caller doesn't want the equality check
      return false;
    }

    if (equality) {
      return equality(val1, val2);
    }

    return _.isEqual(val1, val2);
  };
}

Datastore.prototype = {
  /**
   * Add or modify a value in the data.
   * For path details see: https://github.com/mariocasciaro/object-path-immutable
   * However, the string form _must_ be used.
   * @param {string} path The object-path to set. Like 'a' or 'a.b'.
   * @param {any} value The value to set.
   * @param {?(function|boolean)} equality The equality comparison function. If not
   *    supplied, _.isEqual (deep equality) will be used. If `false`, no equality comparison
   *    will be done. If supplied, it must take old and new values and return a boolean.
   */
  set: function set(path, value, equality) {
    var newData = this._imm().set(path, value).value();

    var oldVal = _.get(this.data, path);

    var newVal = _.get(newData, path);

    if (this._eq(oldVal, newVal, equality)) {
      // No change; don't update or dispatch
      return;
    } // Update our data


    this.data = newData; // Tell the subscribers

    this._dispatch(path, this.EventTypes.change);
  },
  // TODO: Add needed API shims for https://github.com/mariocasciaro/object-path-immutable

  /**
   * Subscribe to changes in the data store, for changes at or below path; when changes
   * occur func will be called.
   * @param {string} path The object-path that will be subscribed to. This can be a leaf
   *    value or branch higher up. The dotted-string form of the path must be used.
   *    If falsy or '', the subscription will be to the whole data object.
   * @param {function} func The function that will be called whenever a value at or under
   *    the path is changed. It will be called like `func(path, data, type)`, where `path`
   *    is the same path passed in here, `data` is the data at that path (possibly a
   *    subtree if subscribe to a non-leaf), and `type` will be one of `EventTypes`,
   *    depending on the change type.
   * @returns {function} The unsubscribe function.
   */
  subscribe: function subscribe(path, func) {
    var _this2 = this;

    // Empty string works as subscribe-to-all because _.startsWith will always return true for it.
    path = path || '';
    var key = String(Math.random());

    var unsubscribe = function unsubscribe() {
      _.remove(_this2._subscribers, function (s) {
        return s.key === key;
      });
    };

    this._subscribers.push({
      func: func,
      path: path,
      key: key
    });

    return unsubscribe;
  }
};
/**
 * Internationalization helper. Must be initialized before use.
 * Should be accessed via `window.i18n`, with the exception of `I18n.localeBestMatch`.
 */

function I18n() {
  /**
   * Initializes this object.
   * @param {Object} translations The set of translations available to us.
   *    Must be of the form `{en: {translation:{key1:"string1",key2:"string2",...}}, ...}
   * @param {string} fallbackLocale The locale to use if a string key is missing from a translation,
   *    or if `setLocale` is passed a locale that it can't find a match for.
   */
  this.init = function (translations, fallbackLocale) {
    this.translations = translations;
    this.locales = Object.keys(translations);

    if (!this.translations[fallbackLocale]) {
      throw new Error("fallbackLocale '".concat(fallbackLocale, "' must exactly match a locale in translations"));
    }

    this.fallbackLocale = fallbackLocale;
    this.currentLocale = this.fallbackLocale;
  };
  /**
   * Sets the current locale of this object to a best match of `locale`. If UI l10n
   * update is also desired, `localizeUI` should be called after this.
   * @param {string} locale
   */


  this.setLocale = function (locale) {
    this.currentLocale = I18n.localeBestMatch(locale, this.locales) || this.fallbackLocale;
  };
  /**
   * Returns true if the current locale is RTL, false otherwise.
   * @returns {boolean}
   */


  this.isRTL = function () {
    // Factors to keep in mind:
    // - some languages are RTL in their default script; like Arabic and Hebrew
    // - some languages sometimes use Arabic script, but not by default; like Kazakh
    // - any locale can have the `Arab` script set and become RTL (probably other
    //   scripts as well, but that's the only one we'll check for)
    var defaultRTLLanguages = ['devrtl', 'fa', 'ar', 'ug', 'ur', 'he', 'ps', 'sd']; // This isn't all of them, but the ones we might reasonably encounter combined with an RTL language. We may need to expand this with time.

    var ltrScripts = ['Latn', 'Cyrl', 'Deva']; // We may need to expand this with time.

    var rtlScripts = ['Arab']; // Note that script names are always four characters, and language and country codes
    // are always 2 characters, so we're not going to accidentally match a script name.
    // Do we have an explicit RTL script?

    for (var i = 0; i < rtlScripts.length; i++) {
      if (this.currentLocale.toLowerCase().indexOf(rtlScripts[i].toLowerCase()) >= 0) {
        // eslint-disable-line
        return true;
      }
    } // Do we have an explicit LTR script?


    for (var _i = 0; _i < ltrScripts.length; _i++) {
      if (this.currentLocale.toLowerCase().indexOf(ltrScripts[_i].toLowerCase()) >= 0) {
        // eslint-disable-line
        return false;
      }
    } // Does the current language default to RTL?


    for (var _i2 = 0; _i2 < defaultRTLLanguages.length; _i2++) {
      if (this.currentLocale.toLowerCase().startsWith(defaultRTLLanguages[_i2].toLowerCase())) {
        return true;
      }
    }

    return false;
  };
  /**
   * Find the string corresponding to `key` for the current locale (falling back if
   * necessary). If the key can't be found, an exception will be thrown.
   * @param {string} key
   * @returns {string}
   */


  this.t = function (key) {
    var translation = this.translations[this.currentLocale].translation[key] || this.translations[this.fallbackLocale].translation[key];

    if (!translation) {
      throw new Error("failed to find translation for key '".concat(key, "' and locale '").concat(this.currentLocale, "'"));
    }

    return translation;
  };
  /**
   * Update the UI with translations for the current locale.
   */


  this.localizeUI = function () {
    var translatableElems = $('[data-i18n]');

    for (var i = 0; i < translatableElems.length; i++) {
      var elem = translatableElems.eq(i);
      var key = elem.data('i18n');

      if (key.startsWith('[html]')) {
        elem.html(this.t(key.slice('[html]'.length)));
      } else {
        elem.text(this.t(key));
      }
    }

    $('html').attr('lang', this.currentLocale);
    var rtl = this.isRTL();
    $('body').attr('dir', rtl ? 'rtl' : 'ltr').css('direction', rtl ? 'rtl' : 'ltr');
  };
  /**
   * Finds the best match for `desiredLocale` among `availableLocales`. Returns a locale
   * from `availableLocales`, or `null` if no acceptable match can be found.
   * @param {string} desiredLocale
   * @param {string[]} availableLocales
   * @returns {?string}
   */

  /* Adheres to this behaviour:
  [
    {
      _desc: 'Exact match',
      desiredLocale: 'zh-Hans-CN',
      availableLocales: ['en', 'zh-Hans-CN', 'zh'],
      want: 'zh-Hans-CN'
    },
    {
      _desc: 'Language-only match',
      desiredLocale: 'zh-Hans-CN',
      availableLocales: ['en', 'zh', 'de'],
      want: 'zh'
    },
    {
      _desc: 'Language+script match',
      desiredLocale: 'zh-Hans-CN',
      availableLocales: ['en', 'zh', 'zh-Hans', 'zh-Hant-TW', 'de'],
      want: 'zh-Hans'
    },
    {
      _desc: 'Language+country match',
      desiredLocale: 'zh-Hans-CN',
      availableLocales: ['en', 'zh', 'zh-CN', 'zh-Hant-TW', 'de'],
      want: 'zh-CN'
    },
    {
      _desc: 'Case-insensitive best match',
      desiredLocale: 'ZH-HANS-CN',
      availableLocales: ['en-US', 'zh-TW', 'de-DE'],
      want: 'zh-TW'
    },
    {
      _desc: 'Shortest best match',
      desiredLocale: 'zh-Hans-CN',
      availableLocales: ['en-US', 'zh-XX', 'zh', 'zh-TW', 'de-DE'],
      want: 'zh'
    },
    {
      _desc: 'Equally good matches will be determined by input order; part 1',
      desiredLocale: 'zh-XX-YY',
      availableLocales: ['en-US', 'zh-XX', 'zh', 'zh-YY', 'de-DE'],
      want: 'zh-XX'
    },
    {
      _desc: 'Equally good matches will be determined by input order; part 2',
      desiredLocale: 'zh-XX-YY',
      availableLocales: ['en-US', 'zh-YY', 'zh', 'zh-XX', 'de-DE'],
      want: 'zh-YY'
    },
  ].forEach((test) => {
    const got = I18n.localeBestMatch(test.desiredLocale, test.availableLocales);
    if (got !== test.want) {
      throw new Error(`test failed: "${test._desc}"; got "${got}"; wanted "${test.want}"`)
    }
    console.log(`pass: ${test._desc}`);
  });
  console.log('all tests passed');
  */


  I18n.localeBestMatch = function (desiredLocale, availableLocales) {
    // Our translation locales are like 'en', 'en-Latn', 'en-US', or 'en-Latn-US' (BCP 47 subset).
    // `localeID` can also be like any of those, but not necessarily the same.
    // We want to match intelligently.
    // First try to match exactly. This is case sensitive. If there's a match that
    // requires case-insensitivity, it will be found below.
    for (var i = 0; i < availableLocales.length; i++) {
      if (availableLocales[i] === desiredLocale) {
        return desiredLocale;
      }
    } // We'll break the IDs up into pieces and try to find a best match that must include
    // the first, language, part.


    var desiredLocaleParts = desiredLocale.toLowerCase().split('-');
    var maxMatchScore = 0,
        maxMatchLocale = null;

    for (var _i3 = 0; _i3 < availableLocales.length; _i3++) {
      var translationLocale = availableLocales[_i3];
      var translationLocaleParts = translationLocale.toLowerCase().split('-');

      if (translationLocaleParts[0] !== desiredLocaleParts[0]) {
        // The language part must match.
        continue;
      }

      var currentScore = 1;

      for (var j = 1; j < desiredLocaleParts.length; j++) {
        for (var k = 1; k < translationLocaleParts.length; k++) {
          if (desiredLocaleParts[j] === translationLocaleParts[k]) {
            currentScore += 1;
          }
        }
      }

      if (currentScore > maxMatchScore) {
        maxMatchScore = currentScore;
        maxMatchLocale = translationLocale;
      } else if (currentScore === maxMatchScore) {
        // We're going to break ties by preferring the shortest locale. This allows us,
        // for example, to prefer "zh" over "zh-TW" for "zh-CN" and "zh-Hans-CN".
        if (translationLocale.length < maxMatchLocale.length) {
          maxMatchScore = currentScore;
          maxMatchLocale = translationLocale;
        }
      }
    }

    return maxMatchLocale;
  };
}

window.i18n = new I18n();
//# sourceMappingURL=plugins.js.map
