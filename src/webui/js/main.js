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

;(function(window) {
"use strict";
/* jshint strict:true, newcap:false */

/* GENERAL */

var $window = $(window);

// Fired when the UI language changes
var LANGUAGE_CHANGE_EVENT = 'language-change';

// We often test in-browser and need to behave a bit differently
var IS_BROWSER = true;

// Parse whatever JSON parameters were passed by the application.
var g_initObj = {};
(function() {
  var uriHash = location.hash;
  if (uriHash && uriHash.length > 1) {
    g_initObj = JSON.parse(decodeURIComponent(uriHash.slice(1)));
    IS_BROWSER = false;
  }

  // For browser debugging
  if (IS_BROWSER) {
    g_initObj = g_initObj || {};
    g_initObj.Config = g_initObj.Config || {};
    g_initObj.Config.Language = g_initObj.Config.Language || 'en';
    g_initObj.Config.Banner = g_initObj.Config.Banner || 'banner.png';
    g_initObj.Config.InfoURL = g_initObj.Config.InfoURL || 'http://example.com/psiphon3/index.html';
    g_initObj.Config.GetNewVersionEmail = g_initObj.Config.GetNewVersionEmail || 'psiget@example.com';
    g_initObj.Config.Debug = g_initObj.Config.Debug || true;

    g_initObj.Cookies = JSON.stringify({
      AvailableEgressRegions: ['US', 'GB', 'JP', 'NL', 'DE']
    });

    console.log(g_initObj);
  }
})();

$(function() {
  setTimeout(HtmlCtrlInterface_AppReady, 100);

  // Set the logo "info" link
  $('.logo a').attr('href', g_initObj.Config.InfoURL).attr('title', g_initObj.Config.InfoURL);

  // The banner image filename is parameterized.
  $('.banner img').attr('src', g_initObj.Config.Banner);
  // Let the C-code decide what should be opened when the banner is clicked.
  $('.banner a').on('click', function(e) {
    e.preventDefault();
    HtmlCtrlInterface_BannerClick();
  });

  // Update the size of our tab content element when the window resizes...
  var lastWindowHeight = $window.height();
  var lastWindowWidth = $window.width();
  $window.smartresize(function() {
    // Only go through the resize logic if the window actually changed size.
    // This helps with the constant resize events we get with IE7.
    if (lastWindowHeight !== $window.height() ||
        lastWindowWidth !== $window.width()) {
      lastWindowHeight = $window.height();
      lastWindowWidth = $window.width();
      setTimeout(resizeContent, 1);
    }
  });
  // ...and when a tab is activated...
  $('a[data-toggle="tab"]').on('shown', function() {
    setTimeout(resizeContent, 1);
  });
  // ...and when the language changes...
  $window.on(LANGUAGE_CHANGE_EVENT, function() {
    setTimeout(resizeContent, 1);
  });
  // ...and now.
  resizeContent();
});

function resizeContent() {
  // We want the content part of our window to fill the window, we don't want
  // excessive scroll bars, etc. It's difficult to do "fill the remaining height"
  // with just CSS, so we're going to do some on-resize height adjustment in JS.
  var fillHeight = $window.innerHeight() - $('.main-height').position().top;
  var footerHeight = $('.footer').outerHeight();
  $('.main-height').outerHeight(fillHeight - footerHeight);
  $('.main-height').parentsUntil('.body').add($('.main-height').siblings()).css('height', '100%');

  // Let the panes know that content resized
  $('.main-height').trigger('resize');

  doMatchHeight();
  doMatchWidth();
}


/* CONNECTION ****************************************************************/

var g_lastState = 'stopped';

$(function() {
  $('#start').click(function(e) {
    e.preventDefault();
    if ($(this).hasClass('disabled')) {
      return;
    }
    HtmlCtrlInterface_Start();
  });
  $('#stop').click(function(e) {
    e.preventDefault();
    if ($(this).hasClass('disabled')) {
      return;
    }
    HtmlCtrlInterface_Stop();
  });

  setupConnectToggle();

  // Update the size of our elements when the tab content element resizes...
  $('.main-height').on('resize', function() {
    // Only if this tab is active
    if ($('#connection-pane').hasClass('active')) {
      setTimeout(resizeConnectContent, 1);
    }
  });
  // ...and when the tab is activated...
  $('a[href="#connection-pane"][data-toggle="tab"]').on('shown', function() {
    setTimeout(resizeConnectContent, 1);
  });
  // ...and now.
  resizeConnectContent();
});

function resizeConnectContent() {
  // Resize the text in the button
  var i;
  var $slabs = $('.slabtext-container');
  for (i = 0; i < $slabs.length; i++) {
    if ($slabs.eq(i).data('slabText')) {
      $slabs.eq(i).data('slabText').resizeSlabs();
    }
  }

  // Set the outer box to the correct height
  //$('#connect-toggle').height($('#connect-toggle > *').outerHeight());
}

function setupConnectToggle() {
  var opts = {
    lines: 10, // The number of lines to draw
    length: 6, // The length of each line
    width: 2, // The line thickness
    radius: 6, // The radius of the inner circle
    corners: 1, // Corner roundness (0..1)
    rotate: 50, // The rotation offset
    direction: 1, // 1: clockwise, -1: counterclockwise
    color: ['#000', '#888', '#FFF'], // #rgb or #rrggbb or array of colors
    speed: 0.8, // Rounds per second
    trail: 100, // Afterglow percentage
    shadow: false, // Whether to render a shadow
    hwaccel: true, // Whether to use hardware acceleration
    className: 'spinner', // The CSS class to assign to the spinner
    zIndex: 2e9, // The z-index (defaults to 2000000000)
    top: '50%', // Top position relative to parent
    left: '50%' // Left position relative to parent
  };
  var spinner = new Spinner(opts).spin($('#connect-toggle .wait-spinner')[0]);

  $('#connect-toggle a').click(function(e) {
    e.preventDefault();
    var buttonConnectState = $(this).parents('.connect-toggle-content').data('connect-state');
    if (buttonConnectState === 'stopped') {
      HtmlCtrlInterface_Start();
    }
    else if (buttonConnectState === 'starting' || buttonConnectState === 'connected') {
      HtmlCtrlInterface_Stop();
    }
    // the stopping button is disabled
  });
  updateConnectToggle();

  $('.slabtext-container').slabText({noResizeEvent: true});
}

// Update the main connect button, as well as the connection indicator on the tab.
function updateConnectToggle() {
  $('.connect-toggle-content').each(function() {
    $(this).toggleClass('z-behind', $(this).data('connect-state') !== g_lastState);
  });

  $('a[href="#connection-pane"][data-toggle="tab"] .label').each(function() {
    $(this).toggleClass('hidden', $(this).data('connect-state') !== g_lastState);
  });

  if (g_lastState === 'starting') {
    cycleToggleClass($('.connect-toggle-content[data-connect-state="starting"]'), 'alert-success', g_lastState);
  }
  else if (g_lastState === 'connected') {
  }
  else if (g_lastState === 'stopping') {
    cycleToggleClass($('.connect-toggle-content[data-connect-state="stopping"]'), 'alert-danger', g_lastState);
  }
  else if (g_lastState === 'stopped') {
  }
}

function cycleToggleClass(elem, cls, untilStateChangeFrom) {
  $(elem).toggleClass(cls, 1000, function() {
    if (g_lastState === untilStateChangeFrom) {
      setTimeout(function() {
        cycleToggleClass(elem, cls, untilStateChangeFrom);
      }, 1);
    }
  });
}


/* SETTINGS ******************************************************************/

// We will use this later to check if any settings have changed.
var g_initialSettingsJSON;

var BEST_REGION_VALUE = 'BEST';

$(function settingsInit() {
  // This is merely to help with testing
  if (!g_initObj.Settings)
  {
    g_initObj.Settings = { 
      SplitTunnel: 0, 
      VPN: 0, 
      LocalHttpProxyPort: 7771, 
      LocalSocksProxyPort: 7770, 
      SkipUpstreamProxy: 1, 
      UpstreamProxyHostname: 'upstreamhost', 
      UpstreamProxyPort: 234, 
      EgressRegion: 'GB', 
      defaults: { 
        SplitTunnel: 0, 
        VPN: 0, 
        LocalHttpProxyPort: '', 
        LocalSocksProxyPort: '', 
        SkipUpstreamProxy: 0, 
        UpstreamProxyHostname: '', 
        UpstreamProxyPort: '', 
        EgressRegion: ''  
      } 
    };
  }

  $('.settings-reset a').click(onSettingsReset);

  // Change the accordion heading icon on expand/collapse
  $('.accordion-body')
    .on('show', function() {
      var headingSelector = '.accordion-toggle[href="#' + this.id + '"]';
      var $expandIcon = $(headingSelector).find('.accordion-expand-icon');
      $expandIcon.removeClass($expandIcon.data('icon-closed'))
                 .addClass($expandIcon.data('icon-opened'));

      // Remove focus from the heading to clear the text-decoration. (It's too ham-fisted to do it in CSS.)
      $(headingSelector).blur();
    })
    .on('hide', function() {
      var headingSelector = '.accordion-toggle[href="#' + this.id + '"]';
      var $expandIcon = $(headingSelector).find('.accordion-expand-icon');
      $expandIcon.removeClass($expandIcon.data('icon-opened'))
                 .addClass($expandIcon.data('icon-closed'));

      // Remove focus from the heading to clear the text-decoration. (It's too ham-fisted to do it in CSS.)
      $(headingSelector).blur();
    });

  // ...But the expand/collapse icon doesn't display properly on IE7, so just delete it.
  if (browserCheck('lt-ie8')) {
    $('.accordion-expand-icon').remove();
  }

  // Some fields are disabled in VPN mode
  $('#VPN').change(vpnModeUpdate);
  vpnModeUpdate();

  egressRegionSetup();
  localProxySetup();
  upstreamProxySetup();

  // Capture the settings as the tab is entered, to check for changes later.
  $('a[href="#settings-pane"][data-toggle="tab"]').on('shown', function(e) {
    g_initialSettingsJSON = settingsToJSON();
  });

  // The settings are saved (and applied) when the user navigates away from the
  // Settings tab.
  $('a[data-toggle="tab"]').on('show', function(e) {
    // If we are navigating away from our tab, then we need to apply our settings
    // (if changed).
    if ($('#settings-tab').hasClass('active') &&    // we were active
        !$(this).parent().is($('#settings-tab'))) { // we won't be active
      var settingsJSON = settingsToJSON();
      if (settingsJSON === false) {
        // Settings are invalid. Expand the error sections and prevent switching tabs.
        e.preventDefault();
        showSettingsSection(
          $('#settings-accordion .collapse .error').parents('.collapse').eq(0),
          $('#settings-pane .error input').eq(0).focus());
        return;
      }
      else if (settingsJSON !== g_initialSettingsJSON) {
        // Settings have changed -- update them in the application (and trigger a reconnect).
        HtmlCtrlInterface_SaveSettings(settingsJSON);
      }
    }
  });

  updateAvailableEgressRegions(false); // don't force valid -- haven't filled in settings yet
  refreshSettings();
});

// Refresh the current settings. If newSettings is truthy, it will become the 
// the new current settings, otherwise the existing current settings will be 
// refreshed in the UI.
function refreshSettings(newSettings) {
  newSettings = newSettings || g_initObj.Settings;
  var oldDefaults = g_initObj.Settings.defaults;
  g_initObj.Settings = newSettings;
  if (!g_initObj.Settings.defaults) {
    g_initObj.Settings.defaults = oldDefaults;
  }
  fillSettingsValues(g_initObj.Settings);

  // When the settings change, we need to check the current egress region choice.
  forceEgressRegionValid();
  // NOTE: If more checks like this are added, we'll need to chain them (somehow),
  // otherwise we'll have a mess of modals.
}

function fillSettingsValues(obj) {
  if (typeof(obj.SplitTunnel) !== 'undefined') {
    $('#SplitTunnel').prop('checked', !!obj.SplitTunnel);
  }

  if (typeof(obj.VPN) !== 'undefined') {
    $('#VPN').prop('checked', obj.VPN);
  }
  $('#VPN').change(vpnModeUpdate);
  vpnModeUpdate();

  if (typeof(obj.LocalHttpProxyPort) !== 'undefined') {
    $('#LocalHttpProxyPort').val(obj.LocalHttpProxyPort > 0 ? obj.LocalHttpProxyPort : '');
  }
  $('#LocalHttpProxyPort').trigger('keyup');

  if (typeof(obj.LocalSocksProxyPort) !== 'undefined') {
    $('#LocalSocksProxyPort').val(obj.LocalSocksProxyPort > 0 ? obj.LocalSocksProxyPort : '');
  }
  $('#LocalSocksProxyPort').trigger('keyup');

  if (typeof(obj.UpstreamProxyHostname) !== 'undefined') {
    $('#UpstreamProxyHostname').val(obj.UpstreamProxyHostname);
  }

  if (typeof(obj.UpstreamProxyPort) !== 'undefined') {
    $('#UpstreamProxyPort').val(obj.UpstreamProxyPort > 0 ? obj.UpstreamProxyPort : '');
  }
  $('#UpstreamProxyPort').trigger('keyup');

  if (typeof(obj.SkipUpstreamProxy) !== 'undefined') {
    $('#SkipUpstreamProxy').prop('checked', obj.SkipUpstreamProxy);
  }
  $('#SkipUpstreamProxy').change(skipUpstreamProxyUpdate);
  skipUpstreamProxyUpdate();

  if (typeof(obj.EgressRegion) !== 'undefined') {
    var region = obj.EgressRegion || BEST_REGION_VALUE;
    $('#EgressRegion [data-region]').removeClass('active');
    $('#EgressRegion').find('[data-region="' + region + '"]').addClass('active');
  }
}

function onSettingsReset(e) {
  e.preventDefault();
  refreshSettings(g_initObj.Settings.defaults);
}

// Packages the current settings into JSON string. Returns if invalid value found.
function settingsToJSON() {
  var valid = true;

  valid = valid && egressRegionValid(true);
  valid = valid && localProxyValid(true);
  valid = valid && upstreamProxyValid(true);

  if (!valid) {
    return false;
  }

  var egressRegion = $('#EgressRegion li.active').data('region');
  if (!egressRegion || egressRegion === BEST_REGION_VALUE) {
    egressRegion = '';
  }

  var returnValue = {
    VPN: $('#VPN').prop('checked') ? 1 : 0,
    SplitTunnel: $('#SplitTunnel').prop('checked') ? 1 : 0,
    LocalHttpProxyPort: validatePort($('#LocalHttpProxyPort').val()),
    LocalSocksProxyPort: validatePort($('#LocalSocksProxyPort').val()),
    UpstreamProxyHostname: $('#UpstreamProxyHostname').val(),
    UpstreamProxyPort: validatePort($('#UpstreamProxyPort').val()),
    SkipUpstreamProxy: $('#SkipUpstreamProxy').prop('checked') ? 1 : 0,
    EgressRegion: egressRegion
  };

  return JSON.stringify(returnValue);
}

function updateErrorAlert() {
  // Show/hide the error alert depending on whether we have an erroneous field
  $('#settings-pane .value-error-alert').toggleClass(
    'hidden', $('#settings-pane .control-group.error').length === 0);
}

// Returns the numeric port if valid, otherwise false. Note that 0 is a valid
// return value, and falsy, so use `=== false`.
function validatePort(val) {
  if (val.length === 0) {
    return 0;
  }

  val = parseInt(val);
  if (isNaN(val) || val < 1 || val > 65535) {
    return false;
  }

  return val;
}

// Will be called exactly once. Set up event listeners, etc.
function egressRegionSetup() {
  // Handle changes to the Egress Region
  $('#EgressRegion a').click(function(e) {
    e.preventDefault();
    $('#EgressRegion [data-region]').removeClass('active');
    $(this).parents('[data-region]').addClass('active');
    egressRegionValid(false);
  });
}

// Returns true if the egress region value is valid, otherwise false.
// Shows/hides an error message as appropriate.
function egressRegionValid(finalCheck) {
  var $currRegionElem = $('#EgressRegion li.active');

  // Check to make sure the currently selected egress region is one of the 
  // available regions.
  var valid = $currRegionElem.length > 0 && !$currRegionElem.hasClass('hidden');

  $('#EgressRegion').toggleClass('error', !valid);
  $('#settings-accordion-egress-region .egress-region-invalid').toggleClass('hidden', valid);

  updateErrorAlert();
  return valid;
}

// Check to make sure the currently selected egress region is one of the available
// regions. If not, inform the user and get them to pick another.
// A mismatch may occur either as a result of a change in the available egress
// regions, or a change in the current selection (i.e., a settings change).
function forceEgressRegionValid() {
  if (egressRegionValid()) {
    // Valid, nothing to do
    return;
  }

  // Put up the modal
  $('#EgressRegionUnavailableModal').modal({
    show: true,
    backdrop: 'static'
  });

  showSettingsSection('#settings-accordion-egress-region');
}

// Update the egress region options we show in the UI.
// If `forceValid` is true, then if the currently selected region is no longer 
// available, the user will be prompted to pick a new one.
function updateAvailableEgressRegions(forceValid) {
  var regions = getCookie('AvailableEgressRegions');
  // On first run there will be no such cookie.
  regions = regions || [];

  $('#EgressRegion li').each(function() {
    var elemRegion = $(this).data('region');

    // If no region, this is a divider
    if (!elemRegion) {
      return;
    }

    if (regions.indexOf(elemRegion) >= 0 || elemRegion === BEST_REGION_VALUE) {
      $(this).removeClass('hidden');
    }
    else {
      $(this).addClass('hidden');
    }
  });

  if (forceValid) {
    forceEgressRegionValid();
  }
}

// Will be called exactly once. Set up event listeners, etc.
function localProxySetup() {
  $('#LocalHttpProxyPort, #LocalSocksProxyPort').on(
      'keyup change blur',
      function(event) {
        localProxyValid(false);
      });

  $('#LocalHttpProxyPort, #LocalSocksProxyPort').each(function() {
    localProxyValid(false);
  });
}

// Returns true if the local proxy values are valid, otherwise false.
// Shows/hides an error message as appropriate.
function localProxyValid(finalCheck) {
  // This check always shows an error while the user is typing, so finalCheck is ignored.

  var httpPort = validatePort($('#LocalHttpProxyPort').val());
  var socksPort = validatePort($('#LocalSocksProxyPort').val());
  var unique = (httpPort !== socksPort) || (httpPort === 0) || (socksPort === 0);

  if (httpPort !== false && unique) {
    // Remove HTTP port error state
    $('#LocalHttpProxyPort').parents('.control-group').removeClass('error');
  }

  if (socksPort !== false && unique) {
    // Remove SOCKS port error state
    $('#LocalSocksProxyPort').parents('.control-group').removeClass('error');
  }

  if (httpPort !== false) {
    // Remove port value error message
    $('.help-inline.LocalHttpProxyPort').addClass('hidden');
  }

  if (socksPort !== false) {
    // Remove port value error message
    $('.help-inline.LocalSocksProxyPort').addClass('hidden');
  }

  if (unique) {
    // Remove uniqueness error message
    $('.help-block.local-port-unique').addClass('hidden');
  }

  if (httpPort === false) {
    // Add HTTP port error state
    $('#LocalHttpProxyPort').parents('.control-group').addClass('error');
    $('.help-inline.LocalHttpProxyPort').removeClass('hidden');
  }

  if (socksPort === false) {
    // Add SOCKS port error state
    $('#LocalSocksProxyPort').parents('.control-group').addClass('error');
    $('.help-inline.LocalSocksProxyPort').removeClass('hidden');
  }

  if (!unique) {
    // Add error state on both ports
    $('#LocalHttpProxyPort, #LocalSocksProxyPort')
      .parents('.control-group').addClass('error');
    // Show error message
    $('.help-block.local-port-unique').removeClass('hidden');
  }

  updateErrorAlert();

  return httpPort !== false && socksPort !== false && unique;
}

// Will be called exactly once. Set up event listeners, etc.
function upstreamProxySetup() {
  // Check for upstream proxy validity
  $('#UpstreamProxyHostname, #UpstreamProxyPort').on(
    'keyup change blur',
    function(event) {
      upstreamProxyValid(false);
    });

  $('#UpstreamProxyHostname, #UpstreamProxyPort').each(
    function() {upstreamProxyValid(false);
  });

  // Disable the other upstream proxy settings if skipping
  $('#SkipUpstreamProxy').change(skipUpstreamProxyUpdate);
  skipUpstreamProxyUpdate();
}

// Returns true if the upstream proxy values are valid, otherwise false.
// Shows/hides an error message as appropriate.
function upstreamProxyValid(finalCheck) {
  // Either the hostname and port have to both be set, or neither.
  // Unless 'skip' is checked.
  var setMatch =
      $('#SkipUpstreamProxy').prop('checked') ||
      Boolean($('#UpstreamProxyHostname').val()) === Boolean($('#UpstreamProxyPort').val());

  var portOK = validatePort($('#UpstreamProxyPort').val()) !== false;

  if (portOK) {
    // Hide the port-specific message
    $('.help-inline.UpstreamProxyPort').addClass('hidden');
  }

  if (setMatch) {
    // Hide the set-match-specific message
    $('.upstream-proxy-set-match').addClass('hidden');
    // And remove error state from hostname (but not from the port value... yet)
    $('#UpstreamProxyHostname')
      .parents('.control-group').removeClass('error');
  }

  if (portOK && setMatch) {
    // No error at all, remove error state
    $('#UpstreamProxyHostname, #UpstreamProxyPort')
      .parents('.control-group').removeClass('error');
  }

  if (!portOK) {
    // Port value bad. Show error while typing
    $('.help-inline.UpstreamProxyPort')
      .removeClass('hidden')
      .parents('.control-group').addClass('error');
  }

  if (!setMatch && finalCheck) {
    // Value mismatch. Only show error on final check (so as to not prematurely
    // show the error while the user is typing).
    $('#UpstreamProxyHostname, #UpstreamProxyPort')
      .parents('.control-group').addClass('error');
    $('.upstream-proxy-set-match').removeClass('hidden');
  }

  updateErrorAlert();

  return setMatch && portOK;
}

// Some of the settings are incompatible with VPN mode. We'll modify the display
// depending on the choice of VPN mode.
function vpnModeUpdate() {
  var vpn = $('#VPN').prop('checked');
  $('input.vpn-incompatible:not(.perma-disabled), .vpn-incompatible:not(.perma-disabled) input, '+
    'select.vpn-incompatible:not(.perma-disabled), .vpn-incompatible:not(.perma-disabled) select')
      .prop('disabled', vpn).toggleClass('disabled', vpn);
  $('.vpn-incompatible-msg').toggleClass('hidden', !vpn);
  $('.vpn-incompatible').toggleClass('disabled-text', vpn);

  // The fancy msDropDown controls require more work to disable.
  $('body select').each(function() {
    if ($(this).data('dd')) {
      $(this).data('dd').set('disabled', this.disabled);
    }
  });
}

// The other upstream proxy settings should be disabled if skip-upstream-proxy is set.
function skipUpstreamProxyUpdate() {
  var skipUpstreamProxy = $('#SkipUpstreamProxy').prop('checked');
  $('.skip-upstream-proxy-incompatible input').prop('disabled', skipUpstreamProxy);
  $('.skip-upstream-proxy-incompatible').toggleClass('disabled-text', skipUpstreamProxy);
}

// Show the Settings tab and expand the target section.
// If focusElem is optional; if set, focus will be put in that element.
function showSettingsSection(section, focusElem) {
  // We can only expand the section after the tab is shown
  function onTabShown() {
    // Hack: The collapse-show doesn't seem to work unless we wait a bit
    setTimeout(function() {
      $(section).collapse('show');

      // Scroll to the section, after allowing the section to expand
      setTimeout(function() {
        $('#settings-pane').scrollTo(
          $(section).parents('.accordion-group').eq(0),
          {
            duration: 500, // animation time
            offset: -50,   // leave some space for the alert
            onAfter: function() {
              if (focusElem) {
                $(focusElem).eq(0).focus();
              }
            }
          });
      }, 200);
    }, 500);
  }

  if ($('#settings-tab').hasClass('active')) {
    // Settings tab already showing. Just expand and scroll.
    onTabShown();
  }
  else {
    // Settings tab not already showing. Switch to it before expanding and scrolling.
    $('.main-nav a[href="#settings-pane"]').one('show', onTabShown);
    $('.main-nav a[href="#settings-pane"]').tab('show');
  }
}

function upstreamProxyErrorNotice(errorMessage) {
  // Show/hide the appropriate message depending on the state of the settings
  $('#UpstreamProxyErrorModal .upstream-proxy-default')
    .toggleClass('hidden', !!g_initObj.Settings.UpstreamProxyHostname);
  $('#UpstreamProxyErrorModal .upstream-proxy-configured')
    .toggleClass('hidden', !g_initObj.Settings.UpstreamProxyHostname);

  // Show the "technical error message" if we have one
  if (!errorMessage) {
    $('#UpstreamProxyErrorModal')
      .find('.notice-error-pre-message, .notice-error-message')
      .addClass('hidden');
  }
  else {
    $('#UpstreamProxyErrorModal')
      .find('.notice-error-pre-message, .notice-error-message')
      .removeClass('hidden');

    $('#UpstreamProxyErrorModal .notice-error-message')
      .text(errorMessage);
  }

  // Put up the modal
  $('#UpstreamProxyErrorModal').modal({
    show: true,
    backdrop: 'static'
  }).on('hidden', function() {
    $('#UpstreamProxyHostname').focus();
  });

  // Switch to the appropriate settings section
  showSettingsSection('#settings-accordion-upstream-proxy');
}

function localProxyPortConflictNotice(noticeType) {
  // Show/hide the appropriate message depending on the error
  $('#LocalProxyPortErrorModal .local-proxy-port-http')
    .toggleClass('hidden', noticeType !== 'HttpProxyPortInUse');
  $('#LocalProxyPortErrorModal .local-proxy-port-socks')
    .toggleClass('hidden', noticeType !== 'SocksProxyPortInUse');

  // Put up the modal
  $('#LocalProxyPortErrorModal').modal({
    show: true,
    backdrop: 'static'
  }).on('hidden', function() {
    if (noticeType === 'HttpProxyPortInUse') {
      $('#LocalHttpProxyPort').focus();
    }
    else {
      $('#LocalSocksProxyPort').focus();
    }
  });

  // Switch to the appropriate settings section
  showSettingsSection(
    '#settings-accordion-local-proxy-ports',
    noticeType === 'HttpProxyPortInUse' ? '#LocalHttpProxyPort' : '#LocalSocksProxyPort');
}


/* FEEDBACK ******************************************************************/

$(function() {
  // This is to help with testing
  if (!g_initObj.Feedback)
  {
    g_initObj.Feedback = {
      "NewVersionURL": "http://www.example.com/en/download.html",
      "NewVersionEmail": "get@example.com",
      "FaqURL": "http://www.example.com/en/faq.html",
      "DataCollectionInfoURL": "http://example.com/en/faq.html#information-collected"
    };
  }

  // Add click listener to the happy/sad choices
  $('.feedback-smiley .feedback-choice').click(function(e) {
    e.preventDefault();
    $('.feedback-smiley .feedback-choice').removeClass('selected');
    $(this).addClass('selected');
  });

  // Values in the feedback text need to be replaced when the text changes -- i.e., on language switch.
  $window.on(LANGUAGE_CHANGE_EVENT, fillFeedbackValues);
  fillFeedbackValues();

  $('#feedback-submit').click(function(e) {
    e.preventDefault();
    sendFeedback();
  });
});

// Some of the text in the feedback form has values that need to be replaced at runtime.
// They also need to be replaced when switching values.
function fillFeedbackValues() {
  if (g_initObj.Feedback.NewVersionURL && g_initObj.Feedback.NewVersionEmail) {
    // Fill in the links and addresses that are specific to this client
    $('.NewVersionURL').attr('href', g_initObj.Feedback.NewVersionURL);
    $('.NewVersionEmail').attr('href', 'mailto:'+g_initObj.Feedback.NewVersionEmail)
                         .text(g_initObj.Feedback.NewVersionEmail);
    $('.NewVersionURL').parent().removeClass('hidden');
  }

  if (g_initObj.Feedback.FaqURL) {
    $('.FaqURL').attr('href', g_initObj.Feedback.FaqURL);
  }

  if (g_initObj.Feedback.DataCollectionInfoURL) {
    $('.DataCollectionInfoURL').attr('href', g_initObj.Feedback.DataCollectionInfoURL);
  }
}

function sendFeedback() {
  var smileyResponses = [];
  $('.feedback-choice.selected').each(function() {
    smileyResponses.push({
      title: $(this).data('feedback-choice-title'),
      question: $(this).data('feedback-choice-hash'),
      answer: $(this).data('feedback-choice-value')
    });
  });

  var fields = {
    responses: smileyResponses,
    feedback: $('#feedback-comments').val(),
    email: $('#feedback-email').val(),
    sendDiagnosticInfo: !!$('#feedback-send-diagnostic').prop('checked')
  };

  // Switch to the connection tab
  $('.main-nav a:first').tab('show');

  // Clear the feedback form
  $('.feedback-choice.selected').removeClass('selected');
  $('#feedback-comments').val('');
  $('#feedback-email').val('');
  $('#feedback-send-diagnostic').prop('checked', true);

  // Actually send the feedback
  HtmlCtrlInterface_SendFeedback(JSON.stringify(fields));

  // Show (and hide) the success alert
  displayCornerAlert($('#feedback-success-alert'));
}

/* LOG MESSAGES **************************************************************/

$(function() {
  $('#show-debug-messages').click(showDebugMessagesClicked);

  // Set the initial show-debug state
  var show = $('#show-debug-messages').prop('checked');
  $('.log-messages')
    .toggleClass('showing-priority-0', show)
    .toggleClass('hiding-priority-0', !show);
});

function showDebugMessagesClicked(e) {
  /*jshint validthis:true */
  var show = $(this).prop('checked');
  // We use both a showing and a hiding class to try to deal with IE7's CSS insanity.
  $('.log-messages')
    .toggleClass('showing-priority-0', show)
    .toggleClass('hiding-priority-0', !show);
}

function addLogMessage(obj) {
  $('.log-messages .placeholder').remove();

  $('.log-messages').loadTemplate(
    $("#message-template"),
    { 
      message: obj.message,
      priority: 'priority-' + obj.priority 
    },
    { 
      prepend:true 
    });

  // The "Show Debug Messages" checkbox is hidden until we actually get a debug
  // message.
  if (obj.priority < 1) {
    $('#messages-pane .invisible').removeClass('invisible');
  }
}

/* LANGUAGE ******************************************************************/

var RTL_LOCALES = ['devrtl', 'fa', 'ar'];

$(function() {
  var fallbackLanguage = 'en';

  // Language priority: cookie, system locale, fallback
  var lang = getCookie('language') ||
              (g_initObj.Config && g_initObj.Config.Language) ||
              fallbackLanguage;

  i18n.init(
    {
      lang: lang,
      fallbackLng: fallbackLanguage,
      resStore: window.PSIPHON.LOCALES
    },
    function(t) {
      switchLocale(lang, true);
    });

  // Populate the list of language choices
  populateLocales();
});

// We only want to show the success/welcome message once.
var g_languageSuccessAlertShown = false;

function switchLocale(locale, initial) {
  i18n.setLng(locale, function() {
    // This callback does not seem to called asynchronously (probably because
    // we're loading from an object and not a remote resource). But we want this
    // code to run after everything else is done, so we'll force it to be async.

    setTimeout(function() {
      $('body').i18n();

      // The content of elements will have changed, so trigger custom event that can
      // be listened for to take additional actions.
      $window.trigger(LANGUAGE_CHANGE_EVENT);

      if (!initial && !g_languageSuccessAlertShown) {
        // Show (and hide) the success alert
        g_languageSuccessAlertShown = true;
        displayCornerAlert($('#language-success-alert'));
      }

      // Remember the user's choice
      setCookie('language', locale);
    }, 1);
  });

  //
  // Right-to-left languages need special consideration.
  //

  var rtl = RTL_LOCALES.indexOf(locale) >= 0;

  $('body').attr('dir', rtl ? 'rtl' : 'ltr')
           .css('direction', rtl ? 'rtl' : 'ltr');

  // We'll use a data attribute to store classes which should only be used
  // for RTL and not LTR, and vice-versa.
  $('[data-i18n-rtl-classes]').each(function() {
      $(this).toggleClass($(this).data('i18n-ltr-classes'), !rtl)
             .toggleClass($(this).data('i18n-rtl-classes'), rtl);
  });
}

function populateLocales() {
  var localePriorityGuide = ['en', 'fa', 'zh', 'zh_CN', 'zh_TW'];
  var locales = $.map(window.PSIPHON.LOCALES, function(val, key) { return key; });
  // Sort the locales according to the priority guide
  locales.sort(function(a, b) {
    var localePriority_a = localePriorityGuide.indexOf(a);
    var localePriority_b = localePriorityGuide.indexOf(b);
    localePriority_a = (localePriority_a < 0) ? 999 : localePriority_a;
    localePriority_b = (localePriority_b < 0) ? 999 : localePriority_b;

    if (localePriority_a < localePriority_b) {
      return -1;
    }
    else if (localePriority_a > localePriority_b) {
      return 1;
    }
    else if (a < b) {
      return -1;
    }
    else if (a > b) {
      return 1;
    }
    return 0;
  });

  var $localeListElem = $('#language-pane ul');

  for (var i = 0; i < locales.length; i++) {
    // If we're not in debug mode, don't output the dev locales
    if (!g_initObj.Config.Debug && locales[i].indexOf('dev') === 0) {
      continue;
    }

    $localeListElem.loadTemplate(
      $("#locale-template"),
      { localeCode: locales[i],
        localeName: window.PSIPHON.LOCALES[locales[i]].name},
      { append:true });
  }

  // Set up the click handlers
  $('.language-choice').click(function(e) {
    e.preventDefault();
    switchLocale($(this).attr('value'));
  });
}


/* ABOUT *********************************************************************/

// Note that this is called before the DOM is fully loaded.
(function() {
  $window.on(LANGUAGE_CHANGE_EVENT, function() {
    $('#about-info-url').attr('href', g_initObj.Config.InfoURL);
    $('#about-email').attr('href', 'mailto:' + g_initObj.Config.GetNewVersionEmail)
                     .text(g_initObj.Config.GetNewVersionEmail);
  });
})();


/* HELPERS *******************************************************************/

// Support the `data-match-height` feature
function doMatchHeight() {
  var matchSelectors = [];
  $('[data-match-height]').each(function() {
    var $this = $(this);

    // Store the original padding, if we don't already have it.
    if (typeof($this.data('match-height-orig-padding-top')) === 'undefined') {
      $this.data('match-height-orig-padding-top', parseInt($this.css('padding-top')))
           .data('match-height-orig-padding-bottom', parseInt($this.css('padding-bottom')));
    }

    var matchSelector = $this.data('match-height');
    if (matchSelectors.indexOf(matchSelector) < 0) {
      matchSelectors.push(matchSelector);
    }
  });

  for (var i = 0; i < matchSelectors.length; i++) {
    matchHeights(matchSelectors[i]);
  }

  function matchHeights(matchSelector) {
    var maxHeight = 0;
    $(matchSelector).each(function() {
      var $this = $(this);
      // Reset the padding to its original state
      $this.css('padding-top', $this.data('match-height-orig-padding-top'))
           .css('padding-bottom', $this.data('match-height-orig-padding-bottom'));

      maxHeight = Math.max(maxHeight, $this.height());
    });

    $(matchSelector).each(function() {
      var $this = $(this);

      var heightDiff = maxHeight - $this.height();
      if (heightDiff <= 0) {
        // Don't adjust the largest
        return;
      }

      var align = $this.data('match-height-align');
      if (align === 'top') {
        $this.css('padding-bottom', heightDiff + $this.data('match-height-orig-padding-bottom'));
      }
      else if (align === 'bottom') {
        $this.css('padding-top', heightDiff + $this.data('match-height-orig-padding-top'));
      }
      else { // center
        $this.css('padding-top', heightDiff/2 + $this.data('match-height-orig-padding-top'))
             .css('padding-bottom', heightDiff/2 + $this.data('match-height-orig-padding-bottom'));
      }
    });
  }
}

// Support the `data-match-width` feature
function doMatchWidth() {
  var matchSelectors = [];
  $('[data-match-width]').each(function() {
    var $this = $(this);

    var matchSelector = $this.data('match-width');
    if (matchSelectors.indexOf(matchSelector) < 0) {
      matchSelectors.push(matchSelector);
    }
  });

  for (var i = 0; i < matchSelectors.length; i++) {
    matchWidths(matchSelectors[i]);
  }

  function matchWidths(matchSelector) {
    var maxWidth = 0;
    $(matchSelector).each(function() {
      var $this = $(this);
      // Reset the width to its original state
      $this.width('');

      maxWidth = Math.max(maxWidth, $this.width());
    });

    $(matchSelector).each(function() {
      var $this = $(this);

      var widthDiff = maxWidth - $this.width();
      if (widthDiff <= 0) {
        // Don't adjust the largest
        return;
      }

      $this.width(maxWidth);
    });
  }
}

function displayCornerAlert(elem) {
  // Show -- and then hide -- the alert
  $(elem).toggle('fold', {horizFirst: true}, function() {
    setTimeout(function() {
      $(elem).toggle('fold', {horizFirst: true}, 1000);
    }, 5000);
  });
}

// Check the current browser version number. `version` must be something like "lt-ie8".
// Return value is boolean.
// Note that this isn't super flexible yet. It will need to be improved as it's used.
function browserCheck(version) {
  return $('html').hasClass(version);
}

//
// We don't have the ability to use real cookies or DOM storage, so we'll store
// persistent stuff in the registry via the win32 code.
//

var g_cookies = g_initObj.Cookies ? JSON.parse(g_initObj.Cookies) : {};

function getCookie(name) {
  return g_cookies[name];
}

function setCookie(name, value) {
  g_cookies[name] = value;
  HtmlCtrlInterface_SetCookies(JSON.stringify(g_cookies));
}


/* INTERFACE METHODS *********************************************************/

var PSIPHON_LINK_PREFIX = 'psi:';

/* Calls from C code to JS code. */

// Add new status message.
function HtmlCtrlInterface_AddMessage(jsonArgs) {
  setTimeout(function() {
    addLogMessage(JSON.parse(jsonArgs));
  }, 1);
}

// Add new notice. This may be interpreted and acted upon.
function HtmlCtrlInterface_AddNotice(jsonArgs) {
  setTimeout(function() {
    var args = JSON.parse(jsonArgs);
    if (args.noticeType === 'UpstreamProxyError') {
      upstreamProxyErrorNotice(args.data.message);
    }
    else if (args.noticeType === 'HttpProxyPortInUse' ||
             args.noticeType === 'SocksProxyPortInUse') {
      localProxyPortConflictNotice(args.noticeType);
    }
    else if (args.noticeType === 'AvailableEgressRegions') {
      // Store the value in a cookie so that it's available at the next startup.
      setCookie('AvailableEgressRegions', args.data.regions);
      // Update the UI.
      updateAvailableEgressRegions(true);
    }
  }, 1);
}

// Set the connected state.
function HtmlCtrlInterface_SetState(jsonArgs) {
  setTimeout(function() {
    var args = JSON.parse(jsonArgs);
    g_lastState = args.state;
    updateConnectToggle();
  }, 1);
}

// Refresh the current settings values.
function HtmlCtrlInterface_RefreshSettings(jsonArgs) {
  setTimeout(function() {
    var args = JSON.parse(jsonArgs);
    refreshSettings(args);
  }, 1);
}

/* Calls from JS code to C code. */

// Let the C code know that the UI is ready.
function HtmlCtrlInterface_AppReady() {
  setTimeout(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'ready';
    if (IS_BROWSER) {
      console.log(appURL);
    }
    else {
      window.location = appURL;
    }
  }, 1);
}

// Connection should start.
function HtmlCtrlInterface_Start() {
  // Prevent duplicate state change attempts
  if (g_lastState === 'starting' || g_lastState === 'connected') {
    return;
  }
  setTimeout(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'start';
    if (IS_BROWSER) {
      console.log(appURL);
    }
    else {
      window.location = appURL;
    }
  }, 1);
}

// Connection should stop.
function HtmlCtrlInterface_Stop() {
  // Prevent duplicate state change attempts
  if (g_lastState === 'stopping' || g_lastState === 'disconnected') {
    return;
  }
  setTimeout(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'stop';
    if (IS_BROWSER) {
      console.log(appURL);
    }
    else {
      window.location = appURL;
    }
  }, 1);
}

// Settings should be saved.
function HtmlCtrlInterface_SaveSettings(settingsJSON) {
  setTimeout(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'savesettings?' + encodeURIComponent(settingsJSON);
    if (IS_BROWSER) {
      console.log(decodeURIComponent(appURL));
      HtmlCtrlInterface_RefreshSettings(settingsJSON);
    }
    else {
      window.location = appURL;
    }
  }, 1);
}

// Feedback should be sent.
function HtmlCtrlInterface_SendFeedback(feedbackJSON) {
  setTimeout(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'sendfeedback?' + encodeURIComponent(feedbackJSON);
    if (IS_BROWSER) {
      console.log(decodeURIComponent(appURL));
    }
    else {
      window.location = appURL;
    }
  }, 1);
}

// Cookies (i.e., UI settings) should be saved.
function HtmlCtrlInterface_SetCookies(cookiesJSON) {
  setTimeout(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'setcookies?' + encodeURIComponent(cookiesJSON);
    if (IS_BROWSER) {
      console.log(decodeURIComponent(appURL));
    }
    else {
      window.location = appURL;
    }
  }, 1);
}

// Banner was clicked.
function HtmlCtrlInterface_BannerClick() {
  setTimeout(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'bannerclick';
    if (IS_BROWSER) {
      console.log(decodeURIComponent(appURL));
    }
    else {
      window.location = appURL;
    }
  }, 1);
}


/* EXPORTS */

// The C interface code is unable to access functions that are members of objects,
// so we'll need to directly expose our exports.

window.HtmlCtrlInterface_AddMessage = HtmlCtrlInterface_AddMessage;
window.HtmlCtrlInterface_SetState = HtmlCtrlInterface_SetState;
window.HtmlCtrlInterface_AddNotice = HtmlCtrlInterface_AddNotice;
window.HtmlCtrlInterface_RefreshSettings = HtmlCtrlInterface_RefreshSettings;

})(window);
