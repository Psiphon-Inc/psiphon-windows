"use strict";
/* jshint strict:true, node:true */
/* global grunt */


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
          'vendor/bootstrap-2.3.2/css/bootstrap.css': 'vendor/bootstrap-2.3.2/less/bootstrap.less',
          'vendor/bootstrap-2.3.2/css/bootstrap-responsive.css': 'vendor/bootstrap-2.3.2/less/responsive.less',
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

    locales: {
      dist: {
        src: '_locales/',
        dest: 'js/locales.js'
      }
    }

  });

  grunt.loadNpmTasks('grunt-contrib-concat');
  grunt.loadNpmTasks('grunt-contrib-less');
  grunt.loadNpmTasks('grunt-inline');

  grunt.registerTask('default', ['concat', 'less', 'locales', 'inline']);

  grunt.registerMultiTask(
    'locales',
    'Process locale files for use in dev and production',
    function() {
      var localeNames = grunt.file.readJSON(this.data.src + 'locale-names.json');
      var locales = {};
      grunt.file.recurse(this.data.src, function localesDirRecurse(abspath, rootdir, subdir, filename) {
        if (!subdir) {
          // Not a subdirectory.
          return;
        }

        var localeCode = subdir;

        if (!localeNames[localeCode]) {
          grunt.fail.fatal('Missing locale name for "' + localeCode + '"');
        }

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

