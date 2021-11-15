/*
 * Copyright (c) 2019, Psiphon Inc.
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

// Due to the environment in which we operate (IE7+ in a web control), we are so
// restricted that even some polyfills won't work properly. We'll have to pre-patch
// some things to prevent (or catch) errors later.

(function(window) {
"use strict";

// To quote es6-shim.js: "Map and Set require a true ES5 environment". So we can't use them.
if (window.Map) {
  delete window.Map;
}
if (window.Set) {
  delete window.Set;
}

})(window);
