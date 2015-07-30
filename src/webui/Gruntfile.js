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

"use strict";
/* jshint strict:true, node:true */
/* global grunt */

var path = require('path');


module.exports = function(grunt) {
  grunt.initConfig({

    concat: {
      options: {
        separator: ';'
      },
      dist: {
        // The order of these file is important and is derived from the Bootstrap Makefile.
        src: ['vendor/bootstrap-2.3.2/js/bootstrap-transition.js','vendor/bootstrap-2.3.2/js/bootstrap-alert.js','vendor/bootstrap-2.3.2/js/bootstrap-button.js','vendor/bootstrap-2.3.2/js/bootstrap-carousel.js','vendor/bootstrap-2.3.2/js/bootstrap-collapse.js','vendor/bootstrap-2.3.2/js/bootstrap-dropdown.js','vendor/bootstrap-2.3.2/js/bootstrap-modal.js','vendor/bootstrap-2.3.2/js/bootstrap-tooltip.js','vendor/bootstrap-2.3.2/js/bootstrap-popover.js','vendor/bootstrap-2.3.2/js/bootstrap-scrollspy.js','vendor/bootstrap-2.3.2/js/bootstrap-tab.js','vendor/bootstrap-2.3.2/js/bootstrap-typeahead.js','vendor/bootstrap-2.3.2/js/bootstrap-affix.js'],
        dest: 'vendor/bootstrap-2.3.2/js/bootstrap.js'
      }
    },

    less: {
      default: {
        options: {
          ieCompat: true
        },
        files: {
          // The Bootstrap files are already in the HTML or included in main.less.
          //'vendor/bootstrap-2.3.2/css/bootstrap.css': 'vendor/bootstrap-2.3.2/less/bootstrap.less',
          //'vendor/bootstrap-2.3.2/css/bootstrap-responsive.css': 'vendor/bootstrap-2.3.2/less/responsive.less',
          'css/main.css': 'css/main.less'
        }
      }
    },

    inline: {
      dist: {
        options:{
          //cssmin: true,
          //uglify: true
        },
        src: 'main.html',
        dest: 'main-inline.html'
      }
    },

    execute: {
        dist: {
            options: {
              cwd: './utils'
            },
            src: ['./utils/fake-translations.js']
        }
    },

    locales: {
      dist: {
        src: '_locales/',
        dest: 'js/locales.js'
      }
    }

  });

  grunt.loadNpmTasks('grunt-execute');
  grunt.loadNpmTasks('grunt-contrib-concat');
  grunt.loadNpmTasks('grunt-contrib-less');
  grunt.loadNpmTasks('grunt-inline');

  grunt.registerTask('default', ['execute', 'concat', 'less', 'locales', 'inline']);

  grunt.registerMultiTask(
    'locales',
    'Process locale files for use in dev and production',
    function() {
      var localeNames = grunt.file.readJSON(this.data.src + 'locale-names.json');
      var locales = {};
      grunt.file.recurse(this.data.src, function localesDirRecurse(abspath, rootdir, subdir, filename) {
        if (!subdir) {
          grunt.log.debug('Not a directory, skipping: ' + abspath);
          return;
        }
        else if (path.extname(abspath) !== '.json') {
          // This helps us skip .orig files.
          grunt.log.debug('Not JSON, skipping: ' + abspath);
          return;
        }

        var localeCode = subdir;
        grunt.log.debug('Starting: ' + localeCode);

        if (!localeNames[localeCode]) {
          grunt.fail.fatal('Missing locale name for "' + localeCode + '"');
        }

        grunt.log.debug('Reading: ' + abspath);
        var translation = grunt.file.readJSON(abspath);
        for (var key in translation) {
          translation[key] = translation[key].message;
        }
        locales[localeCode] = {
          name: localeNames[localeCode],
          translation: translation
        };
      });

      grunt.file.write(
        this.data.dest,
        '(window.PSIPHON || (window.PSIPHON={})).LOCALES = ' + JSON.stringify(locales, null, '  ') + ';');
      grunt.log.ok();
    });
};

