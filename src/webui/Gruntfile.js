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
  require("load-grunt-tasks")(grunt);

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
          cssmin: false,
          uglify: false
        },
        src: 'main.html',
        dest: 'main-inline.html'
      },
      quick: {
        options:{
          cssmin: false,
          uglify: false
        },
        src: 'main.html',
        dest: 'main-inline.html'
      }
    },

    htmlmin: {
      default: {
        options: {
          removeComments: true,
          collapseWhitespace: true,
          minifyCSS: {
            compatibility: 'ie8'
          },
          minifyJS: {
            ie8: true,
            output: {
              beautify: true
            }
          }
        },
        files: {
          'main-inline.html': 'main-inline.html'
        }
      }
    },

    locales: {
      dist: {
        src: '_locales/',
        dest: 'js/locales.js'
      }
    },

    // When any of the main files change, rebuild everything. This could be
    // improved by splitting off what's changed and doing different things
    watch: {
        inline: {
            files: ['main.html', 'js/*.js', 'css/main.css', 'css/main.less', '_locales/**/*.json'],
            tasks: ['quick'],
            options: {},
        }
    },

    connect: {
        server: {
            options: {
                hostname: '0.0.0.0',
                port: 9000,
                base: {
                    path: '.',
                    options: {
                        index: 'main-inline.html'
                    }
                }
            }
        }
    },

    babel: {
      options: {
        sourceMap: true
      },
      dist: {
        files: {
          'dist/app.js': 'js/main.js',
          'dist/plugins.js': 'js/plugins.js'
        }
      }
    }
  });

  grunt.loadNpmTasks('grunt-contrib-concat');
  grunt.loadNpmTasks('grunt-contrib-less');
  grunt.loadNpmTasks('grunt-inline');
  grunt.loadNpmTasks('grunt-contrib-watch');
  grunt.loadNpmTasks('grunt-contrib-connect');
  grunt.loadNpmTasks('grunt-contrib-htmlmin');

  // Capture the node_modules directory at the time of building. This allows us to build
  // even if the dependencies disappear. (package-lock.json provides us protection against
  // dep replacement attacks.)
  grunt.registerTask('zip-modules', '', function () {
    var exec = require('child_process').execSync;
    var result = exec("7z a -mx5 node_modules.7z node_modules");
    grunt.log.writeln(result);
  });

  grunt.registerTask('default', ['zip-modules', 'babel', 'concat', 'less', 'locales', 'inline:dist', 'htmlmin']);
  grunt.registerTask('quick', ['babel', 'concat', 'less', 'locales', 'inline:quick']); // skips the slow zip step
  grunt.registerTask('serve', ['quick', 'connect', 'watch']);

  grunt.registerMultiTask(
    'locales',
    'Process locale files for use in dev and production',
    function() {
      var exec = require('child_process').execSync;
      var result = exec("node fake-translations.js", {cwd: './utils'});
      grunt.log.writeln(result);

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

