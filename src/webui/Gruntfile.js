'use strict';

module.exports = function(grunt) {
  grunt.initConfig({
    inline: {
      dist: {
        options:{
            cssmin: true,
            uglify: true
        },
        src: 'settings.html',
        dest: 'settings-inline.html'
      }
    }
  });

  grunt.loadNpmTasks('grunt-inline');
  grunt.registerTask('default', ['inline']);
};
