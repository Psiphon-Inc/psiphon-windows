'use strict';
/*jshint node:true */

var fs = require('fs');
var pseudo = require('./pseudo').PSEUDO;

var english = JSON.parse(fs.readFileSync(
                          '../_locales/en/messages.json',
                          { encoding: 'utf8' }));

var keys = Object.keys(english).sort();

var devltr = {}, devrtl = {};

// NOTE: We put square brackets around devltr translations but not around devrtl.
// This is because for devltr we want to clearly demarcate the length of the string
// so that it's easier to tell by eye that it's not being cut off; but for devrtl
// we don't want to screw up the right-to-left-ness of it with ASCII characters.

for (var i = 0; i < keys.length; i++) {
  devltr[keys[i]] = {
    message: '[' + pseudo['qps-ploc'].translate((english[keys[i]].message)) + ']',
    description: english[keys[i]].description
  };

  devrtl[keys[i]] = {
    message: pseudo['qps-plocm'].translate((english[keys[i]].message)),
    description: english[keys[i]].description
  };
}

fs.writeFileSync(
  '../_locales/devltr/messages.json',
  JSON.stringify(devltr, null, '  '),
  { encoding: 'utf8' });

fs.writeFileSync(
  '../_locales/devrtl/messages.json',
  JSON.stringify(devrtl, null, '  '),
  { encoding: 'utf8' });
