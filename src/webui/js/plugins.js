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
(function() {
    var method;
    var noop = function () {};
    var methods = [
        'assert', 'clear', 'count', 'debug', 'dir', 'dirxml', 'error',
        'exception', 'group', 'groupCollapsed', 'groupEnd', 'info', 'log',
        'markTimeline', 'profile', 'profileEnd', 'table', 'time', 'timeEnd',
        'timeline', 'timelineEnd', 'timeStamp', 'trace', 'warn'
    ];
    var length = methods.length;
    var console = (window.console = window.console || {});

    while (length--) {
        method = methods[length];

        // Only stub undefined methods.
        if (!console[method]) {
            console[method] = noop;
        }
    }
}());

// Place any jQuery/helper plugins in here.


/* JQUERY.SCROLLTO */
/* https://github.com/flesler/jquery.scrollTo */
/**
 * Copyright (c) 2007-2015 Ariel Flesler - aflesler<a>gmail<d>com | http://flesler.blogspot.com
 * Licensed under MIT
 * @author Ariel Flesler
 * @version 2.1.1
 */
(function(f){"use strict";"function"===typeof define&&define.amd?define(["jquery"],f):"undefined"!==typeof module&&module.exports?module.exports=f(require("jquery")):f(jQuery)})(function($){"use strict";function n(a){return!a.nodeName||-1!==$.inArray(a.nodeName.toLowerCase(),["iframe","#document","html","body"])}function h(a){return $.isFunction(a)||$.isPlainObject(a)?a:{top:a,left:a}}var p=$.scrollTo=function(a,d,b){return $(window).scrollTo(a,d,b)};p.defaults={axis:"xy",duration:0,limit:!0};$.fn.scrollTo=function(a,d,b){"object"=== typeof d&&(b=d,d=0);"function"===typeof b&&(b={onAfter:b});"max"===a&&(a=9E9);b=$.extend({},p.defaults,b);d=d||b.duration;var u=b.queue&&1<b.axis.length;u&&(d/=2);b.offset=h(b.offset);b.over=h(b.over);return this.each(function(){function k(a){var k=$.extend({},b,{queue:!0,duration:d,complete:a&&function(){a.call(q,e,b)}});r.animate(f,k)}if(null!==a){var l=n(this),q=l?this.contentWindow||window:this,r=$(q),e=a,f={},t;switch(typeof e){case "number":case "string":if(/^([+-]=?)?\d+(\.\d+)?(px|%)?$/.test(e)){e= h(e);break}e=l?$(e):$(e,q);if(!e.length)return;case "object":if(e.is||e.style)t=(e=$(e)).offset()}var v=$.isFunction(b.offset)&&b.offset(q,e)||b.offset;$.each(b.axis.split(""),function(a,c){var d="x"===c?"Left":"Top",m=d.toLowerCase(),g="scroll"+d,h=r[g](),n=p.max(q,c);t?(f[g]=t[m]+(l?0:h-r.offset()[m]),b.margin&&(f[g]-=parseInt(e.css("margin"+d),10)||0,f[g]-=parseInt(e.css("border"+d+"Width"),10)||0),f[g]+=v[m]||0,b.over[m]&&(f[g]+=e["x"===c?"width":"height"]()*b.over[m])):(d=e[m],f[g]=d.slice&& "%"===d.slice(-1)?parseFloat(d)/100*n:d);b.limit&&/^\d+$/.test(f[g])&&(f[g]=0>=f[g]?0:Math.min(f[g],n));!a&&1<b.axis.length&&(h===f[g]?f={}:u&&(k(b.onAfterFirst),f={}))});k(b.onAfter)}})};p.max=function(a,d){var b="x"===d?"Width":"Height",h="scroll"+b;if(!n(a))return a[h]-$(a)[b.toLowerCase()]();var b="client"+b,k=a.ownerDocument||a.document,l=k.documentElement,k=k.body;return Math.max(l[h],k[h])-Math.min(l[b],k[b])};$.Tween.propHooks.scrollLeft=$.Tween.propHooks.scrollTop={get:function(a){return $(a.elem)[a.prop]()}, set:function(a){var d=this.get(a);if(a.options.interrupt&&a._last&&a._last!==d)return $(a.elem).stop();var b=Math.round(a.now);d!==b&&($(a.elem)[a.prop](b),a._last=this.get(a))}};return p});

// Smarter resize event
// http://www.paulirish.com/2009/throttled-smartresize-jquery-event-handler/
(function($,sr){
  // debouncing function from John Hann
  // http://unscriptable.com/index.php/2009/03/20/debouncing-javascript-methods/
  var debounce = function (func, threshold, execAsap) {
      var timeout;

      return function debounced () {
          var obj = this, args = arguments;
          function delayed () {
              if (!execAsap)
                  func.apply(obj, args);
              timeout = null;
          }

          if (timeout)
              clearTimeout(timeout);
          else if (execAsap)
              func.apply(obj, args);

          timeout = setTimeout(delayed, threshold || 100);
      };
  };
  // smartresize
  jQuery.fn[sr] = function(fn){  return fn ? this.bind('resize', debounce(fn)) : this.trigger(sr); };

})(jQuery,'smartresize');


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
  this._dispatch = function(path, type) {
    // this.data might change (immutably) during the timeout, so capture the current
    // object now.
    const data = this.data;

    // Ensure this is asynchronous
    setTimeout(()=> {
      // When looking for subscribers, we look at prefixes of paths, as whole sub-trees
      // can be subscribed to.
      const subs = _.filter(this._subscribers, (s) => {
        // We append '.' because we want "a.b" to match "a.b.c" but not "a.banana".
        return _.startsWith(path+'.', s.path+'.');
      });

      for (let i = 0; i < subs.length; i++) {
        let sub = subs[i];
        // The amount of data (tree or specific value) that a subscriber gets depends on
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
  this._imm = function(data) {
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
  this._eq = function(val1, val2, equality) {
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
  set: function(path, value, equality) {
    const newData = this._imm().set(path, value).value();
    const oldVal = _.get(this.data, path);
    const newVal = _.get(newData, path);

    if (this._eq(oldVal, newVal, equality)) {
      // No change; don't update or dispatch
      return;
    }

    // Update our data
    this.data = newData;
    // Tell the subscribers
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
  subscribe: function(path, func) {
    // Empty string works as subscribe-to-all because _.startsWith will always return true for it.
    path = path || '';

    const key = String(Math.random());
    const unsubscribe = () => {
      _.remove(this._subscribers, (s) => { return s.key === key; });
    };
    this._subscribers.push({func, path, key});
    return unsubscribe;
  }
};
