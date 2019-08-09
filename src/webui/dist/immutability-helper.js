"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var invariant = require("invariant");

var hasOwnProperty = Object.prototype.hasOwnProperty;
var splice = Array.prototype.splice;
var toString = Object.prototype.toString;

function type(obj) {
  return toString.call(obj).slice(8, -1);
}

var assign = Object.assign ||
/* istanbul ignore next */
function (target, source) {
  getAllKeys(source).forEach(function (key) {
    if (hasOwnProperty.call(source, key)) {
      target[key] = source[key];
    }
  });
  return target;
};

var getAllKeys = typeof Object.getOwnPropertySymbols === 'function' ? function (obj) {
  return Object.keys(obj).concat(Object.getOwnPropertySymbols(obj));
}
/* istanbul ignore next */
: function (obj) {
  return Object.keys(obj);
};

function copy(object) {
  return Array.isArray(object) ? assign(object.constructor(object.length), object) : type(object) === 'Map' ? new Map(object) : type(object) === 'Set' ? new Set(object) : object && typeof object === 'object' ? assign(Object.create(Object.getPrototypeOf(object)), object)
  /* istanbul ignore next */
  : object;
}

var Context =
/** @class */
function () {
  function Context() {
    this.commands = assign({}, defaultCommands);
    this.update = this.update.bind(this); // Deprecated: update.extend, update.isEquals and update.newContext

    this.update.extend = this.extend = this.extend.bind(this);

    this.update.isEquals = function (x, y) {
      return x === y;
    };

    this.update.newContext = function () {
      return new Context().update;
    };
  }

  Object.defineProperty(Context.prototype, "isEquals", {
    get: function () {
      return this.update.isEquals;
    },
    set: function (value) {
      this.update.isEquals = value;
    },
    enumerable: true,
    configurable: true
  });

  Context.prototype.extend = function (directive, fn) {
    this.commands[directive] = fn;
  };

  Context.prototype.update = function (object, $spec) {
    var _this = this;

    var spec = typeof $spec === 'function' ? {
      $apply: $spec
    } : $spec;

    if (!(Array.isArray(object) && Array.isArray(spec))) {
      invariant(!Array.isArray(spec), 'update(): You provided an invalid spec to update(). The spec may ' + 'not contain an array except as the value of $set, $push, $unshift, ' + '$splice or any custom command allowing an array value.');
    }

    invariant(typeof spec === 'object' && spec !== null, 'update(): You provided an invalid spec to update(). The spec and ' + 'every included key path must be plain objects containing one of the ' + 'following commands: %s.', Object.keys(this.commands).join(', '));
    var nextObject = object;
    getAllKeys(spec).forEach(function (key) {
      if (hasOwnProperty.call(_this.commands, key)) {
        var objectWasNextObject = object === nextObject;
        nextObject = _this.commands[key](spec[key], nextObject, spec, object);

        if (objectWasNextObject && _this.isEquals(nextObject, object)) {
          nextObject = object;
        }
      } else {
        var nextValueForKey = type(object) === 'Map' ? _this.update(object.get(key), spec[key]) : _this.update(object[key], spec[key]);
        var nextObjectValue = type(nextObject) === 'Map' ? nextObject.get(key) : nextObject[key];

        if (!_this.isEquals(nextValueForKey, nextObjectValue) || typeof nextValueForKey === 'undefined' && !hasOwnProperty.call(object, key)) {
          if (nextObject === object) {
            nextObject = copy(object);
          }

          if (type(nextObject) === 'Map') {
            nextObject.set(key, nextValueForKey);
          } else {
            nextObject[key] = nextValueForKey;
          }
        }
      }
    });
    return nextObject;
  };

  return Context;
}();

exports.Context = Context;
var defaultCommands = {
  $push: function (value, nextObject, spec) {
    invariantPushAndUnshift(nextObject, spec, '$push');
    return value.length ? nextObject.concat(value) : nextObject;
  },
  $unshift: function (value, nextObject, spec) {
    invariantPushAndUnshift(nextObject, spec, '$unshift');
    return value.length ? value.concat(nextObject) : nextObject;
  },
  $splice: function (value, nextObject, spec, originalObject) {
    invariantSplices(nextObject, spec);
    value.forEach(function (args) {
      invariantSplice(args);

      if (nextObject === originalObject && args.length) {
        nextObject = copy(originalObject);
      }

      splice.apply(nextObject, args);
    });
    return nextObject;
  },
  $set: function (value, _nextObject, spec) {
    invariantSet(spec);
    return value;
  },
  $toggle: function (targets, nextObject) {
    invariantSpecArray(targets, '$toggle');
    var nextObjectCopy = targets.length ? copy(nextObject) : nextObject;
    targets.forEach(function (target) {
      nextObjectCopy[target] = !nextObject[target];
    });
    return nextObjectCopy;
  },
  $unset: function (value, nextObject, _spec, originalObject) {
    invariantSpecArray(value, '$unset');
    value.forEach(function (key) {
      if (Object.hasOwnProperty.call(nextObject, key)) {
        if (nextObject === originalObject) {
          nextObject = copy(originalObject);
        }

        delete nextObject[key];
      }
    });
    return nextObject;
  },
  $add: function (values, nextObject, _spec, originalObject) {
    invariantMapOrSet(nextObject, '$add');
    invariantSpecArray(values, '$add');

    if (type(nextObject) === 'Map') {
      values.forEach(function (_a) {
        var key = _a[0],
            value = _a[1];

        if (nextObject === originalObject && nextObject.get(key) !== value) {
          nextObject = copy(originalObject);
        }

        nextObject.set(key, value);
      });
    } else {
      values.forEach(function (value) {
        if (nextObject === originalObject && !nextObject.has(value)) {
          nextObject = copy(originalObject);
        }

        nextObject.add(value);
      });
    }

    return nextObject;
  },
  $remove: function (value, nextObject, _spec, originalObject) {
    invariantMapOrSet(nextObject, '$remove');
    invariantSpecArray(value, '$remove');
    value.forEach(function (key) {
      if (nextObject === originalObject && nextObject.has(key)) {
        nextObject = copy(originalObject);
      }

      nextObject.delete(key);
    });
    return nextObject;
  },
  $merge: function (value, nextObject, _spec, originalObject) {
    invariantMerge(nextObject, value);
    getAllKeys(value).forEach(function (key) {
      if (value[key] !== nextObject[key]) {
        if (nextObject === originalObject) {
          nextObject = copy(originalObject);
        }

        nextObject[key] = value[key];
      }
    });
    return nextObject;
  },
  $apply: function (value, original) {
    invariantApply(value);
    return value(original);
  }
};
var defaultContext = new Context();
exports.isEquals = defaultContext.update.isEquals;
exports.extend = defaultContext.extend;
exports.default = defaultContext.update; // @ts-ignore

exports.default.default = module.exports = assign(exports.default, exports); // invariants

function invariantPushAndUnshift(value, spec, command) {
  invariant(Array.isArray(value), 'update(): expected target of %s to be an array; got %s.', command, value);
  invariantSpecArray(spec[command], command);
}

function invariantSpecArray(spec, command) {
  invariant(Array.isArray(spec), 'update(): expected spec of %s to be an array; got %s. ' + 'Did you forget to wrap your parameter in an array?', command, spec);
}

function invariantSplices(value, spec) {
  invariant(Array.isArray(value), 'Expected $splice target to be an array; got %s', value);
  invariantSplice(spec.$splice);
}

function invariantSplice(value) {
  invariant(Array.isArray(value), 'update(): expected spec of $splice to be an array of arrays; got %s. ' + 'Did you forget to wrap your parameters in an array?', value);
}

function invariantApply(fn) {
  invariant(typeof fn === 'function', 'update(): expected spec of $apply to be a function; got %s.', fn);
}

function invariantSet(spec) {
  invariant(Object.keys(spec).length === 1, 'Cannot have more than one key in an object with $set');
}

function invariantMerge(target, specValue) {
  invariant(specValue && typeof specValue === 'object', 'update(): $merge expects a spec of type \'object\'; got %s', specValue);
  invariant(target && typeof target === 'object', 'update(): $merge expects a target of type \'object\'; got %s', target);
}

function invariantMapOrSet(target, command) {
  var typeOfTarget = type(target);
  invariant(typeOfTarget === 'Map' || typeOfTarget === 'Set', 'update(): %s expects a target of type Set or Map; got %s', command, typeOfTarget);
}
//# sourceMappingURL=immutability-helper.js.map
