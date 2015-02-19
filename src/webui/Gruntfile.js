'use strict';

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
          'vendor/bootstrap-2.3.2/css/bootstrap-responsive.css': 'vendor/bootstrap-2.3.2/less/responsive.less'
        }
      }
    },
    inline: {
      dist: {
        options:{
            cssmin: true,
            uglify: true
        },
        src: 'bs.html',
        dest: 'bs-inline.html'
      }
    }
  });

  grunt.loadNpmTasks('grunt-contrib-concat');
  grunt.loadNpmTasks('grunt-contrib-less');
  grunt.loadNpmTasks('grunt-inline');

  grunt.registerTask('default', ['concat', 'less', 'inline']);
};
