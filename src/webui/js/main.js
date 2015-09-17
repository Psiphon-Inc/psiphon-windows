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

// Fired on the window when the UI language changes
var LANGUAGE_CHANGE_EVENT = 'language-change';

// Fired on the window when the connected state changes
var CONNECTED_STATE_CHANGE_EVENT = 'connected-state-change';

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
    g_initObj.Config.ClientVersion = g_initObj.Config.ClientVersion || '99';
    g_initObj.Config.Language = g_initObj.Config.Language || 'en';
    g_initObj.Config.Banner = g_initObj.Config.Banner || 'banner.png';
    g_initObj.Config.InfoURL =
      g_initObj.Config.InfoURL || 'http://example.com/browser-InfoURL/index.html';
    g_initObj.Config.NewVersionEmail =
      g_initObj.Config.NewVersionEmail || 'browser-NewVersionEmail@example.com';
    g_initObj.Config.NewVersionURL =
      g_initObj.Config.NewVersionURL || 'http://example.com/browser-NewVersionURL/en/download.html#direct';
    g_initObj.Config.FaqURL = g_initObj.Config.FaqURL || 'http://example.com/browser-FaqURL/en/faq.html';
    g_initObj.Config.DataCollectionInfoURL =
      g_initObj.Config.DataCollectionInfoURL || 'http://example.com/browser-DataCollectionInfoURL/en/privacy.html#information-collected';
    g_initObj.Config.DpiScaling = 1.0;
    g_initObj.Config.Debug = g_initObj.Config.Debug || true;

    g_initObj.Cookies = JSON.stringify({
      AvailableEgressRegions: ['US', 'GB', 'JP', 'NL', 'DE']
    });

    console.log(g_initObj);
  }
})();

$(function overallInit() {
  // Do some browser-version-dependent DOM pruning
  if (compareIEVersion('lte', 7, false)) {
    $('.ie7Remove').remove();
  }

  // Set the logo "info" link
  $('.logo a').attr('href', g_initObj.Config.InfoURL).attr('title', g_initObj.Config.InfoURL);

  // Update the logo when the connected state changes
  $window.on(CONNECTED_STATE_CHANGE_EVENT, updateLogoConnectState);
  updateLogoConnectState();

  // The banner image filename is parameterized.
  $('.banner img').attr('src', g_initObj.Config.Banner);
  // Let the C-code decide what should be opened when the banner is clicked.
  $('.banner a').on('click', function(e) {
    e.preventDefault();
    HtmlCtrlInterface_BannerClick();
  });

  // Links to the download site and email address are parameterized and need to
  // be updated when the language changes.
  var updateLinks = nextTickFn(function updateLinks() {
    // For some languages we alter the "download site" links to point directly
    // to that language. But the site has different available languages than
    // this application does, so we don't just do it blindly.
    var defaultLang = 'en';
    var siteLangs = ['fa', 'zh'];
    var currentLang = i18n.lng();
    var replaceLang = siteLangs.indexOf(currentLang) >= 0 ? currentLang : defaultLang;
    var url;

    // Note that we're using the function-as-replacement form for String.replace()
    // because we don't entirely control the content of the language names, and
    // we don't want to run into any issues with magic values:
    // https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/String/replace#Specifying_a_string_as_a_parameter

    var replaceFn = function(match, p1, p2) {
        return p1 + '/' + replaceLang + '/' + p2;
      };

    // This link may be to the redirect meta page (/index.html) or to a language-specific
    // page (/en/index.html). If it's to the meta page, we won't force to English, otherwise we will.
    // First change it to the meta page if it's not already
    url = g_initObj.Config.InfoURL.replace('/en/', '/');
    // Then force the language, but not to English
    if (replaceLang !== defaultLang) {
      // We're using the function form of
      url = g_initObj.Config.InfoURL.replace(/^([^?#]*)\/(.*)$/, replaceFn);
    }
    $('.InfoURL').attr('href', url).attr('title', url);

    var regex = /^([^?#]*)\/en\/(.*)$/;

    url = g_initObj.Config.NewVersionURL.replace(regex, replaceFn);
    $('.NewVersionURL').attr('href', url).attr('title', url);

    url = g_initObj.Config.FaqURL.replace(regex, replaceFn);
    $('.FaqURL').attr('href', url).attr('title', url);

    url = g_initObj.Config.DataCollectionInfoURL.replace(regex, replaceFn);
    $('.DataCollectionInfoURL').attr('href', url).attr('title', url);

    // No replacement on the email address
    $('.NewVersionEmail').attr('href', 'mailto:' + g_initObj.Config.NewVersionEmail)
                         .text(g_initObj.Config.NewVersionEmail)
                         .attr('title', g_initObj.Config.NewVersionEmail);

    $('.ClientVersion').text(g_initObj.Config.ClientVersion);
  });
  $window.on(LANGUAGE_CHANGE_EVENT, updateLinks);
  // ...and now.
  updateLinks();

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
      nextTick(resizeContent);
    }
  });
  // ...and when a tab is activated...
  $('a[data-toggle="tab"]').on('shown', function() {
    nextTick(resizeContent);
  });
  // ...and when the language changes...
  $window.on(LANGUAGE_CHANGE_EVENT, nextTickFn(resizeContent));
  // ...and now.
  resizeContent();

  // We don't want buggy scrolling behaviour (which can result from some click-drag selecting)
  initScrollFix();

  setTimeout(HtmlCtrlInterface_AppReady, 100);
});


function resizeContent() {
  // Do DPI scaling
  updateDpiScaling(g_initObj.Config.DpiScaling, false);

  // We want the content part of our window to fill the window, we don't want
  // excessive scroll bars, etc. It's difficult to do "fill the remaining height"
  // with just CSS, so we're going to do some on-resize height adjustment in JS.
  var fillHeight = $window.innerHeight() - $('.main-height').position().top;
  var footerHeight = $('.footer').outerHeight();
  $('.main-height').outerHeight((fillHeight - footerHeight) / g_initObj.Config.DpiScaling);
  $('.main-height').parentsUntil('body').add($('.main-height').siblings()).css('height', '100%');

  // Let the panes know that content resized
  $('.main-height').trigger('resize');

  doMatchHeight();
  doMatchWidth();

  // Adjust the banner to account for the logo space.
  $('.banner').css(g_isRTL ? 'margin-right' : 'margin-left', $('.header-nav-join').outerWidth())
              .css(!g_isRTL ? 'margin-right' : 'margin-left', 0);
}

function updateDpiScaling(dpiScaling, andResizeContent/*=true*/) {
  DEBUG_LOG('updateDpiScaling: ' + dpiScaling);
  g_initObj.Config.DpiScaling = dpiScaling;

  if (compareIEVersion('lt', 9, false)) {
    // We need IE9+ to support DPI scaling
    return;
  }

  var ltrTransformOrigin = '0 0 0',
      ltrBottomRightTransformOrigin = '100% 100% 0',
      rtlTransformOrigin = '0 0 0',
      rtlBottomRightTransformOrigin = '0 100% 0';

  if (getIEVersion() === false && g_isRTL) {
    // Non-IE in RTL need the origin on the right.
    rtlTransformOrigin = '100% 0 0';
  }

  var transformOrigin = g_isRTL ? rtlTransformOrigin : ltrTransformOrigin,
      bottomRightTransformOrigin = g_isRTL ? rtlBottomRightTransformOrigin : ltrBottomRightTransformOrigin;

  // Set the overall body scaling
  $('html').css({
    'transform-origin': transformOrigin,
    'transform': 'scale(' + dpiScaling + ')',
    'width': (100.0 / dpiScaling).toFixed(1) + '%',
    'height': (100.0 / dpiScaling).toFixed(1) + '%'
  });

  // For elements (like modals) outside the normal flow, additional changes are needed.
  if (getIEVersion() !== false) {
    // The left margin will vary depending on default value.
    // First reset an overridden left margin
    $('.modal').css('margin-left', '');
    // Get the default left margin
    var defaultLeftMargin = $('.modal').css('margin-left');
    // Create the left margin we want, based on the default
    var scaledLeftMargin = 'calc(' + defaultLeftMargin + ' * ' + dpiScaling + ')';
    // Now apply the styles.
    $('.modal').css({
      'transform-origin': transformOrigin,
      'transform': 'scale(' + dpiScaling + ')',
      'margin-left': scaledLeftMargin
    });
  }

  $('.global-alert').css({
    'transform-origin': bottomRightTransformOrigin,
    'transform': 'scale(' + dpiScaling + ')'
  });

  // Elements with the `affix` class are position:fixed and need to be adjusted separately
  if (getIEVersion() !== false) {
    // Reset previous position modification.
    $('.affix')
      .css({'top': '', 'left': ''})
      .each(function() {
        var basePosition = $(this).position();
        $(this).css({
          'transform-origin': transformOrigin,
          'transform': 'scale(' + dpiScaling + ')'
        });
        if (basePosition.top) {
          $(this).css({
            'top': (basePosition.top * dpiScaling) + 'px'
          });
        }
        if (basePosition.left) {
          $(this).css({
            'left': (basePosition.left * dpiScaling) + 'px'
          });
        }
      });
  }

  if (andResizeContent !== false) {
    // Need to resize everything.
    nextTick(resizeContent);
  }
}

// Ensures that elements that should not be scrolled are not scrolled.
// This should only be called once.
function initScrollFix() {
  // It would be much better to fix this correctly using CSS rather than
  // detecting a bad state and correcting it. But until we figure that out...
  $('body').scroll(function() {
    $('body').scrollTop(0);
  });
}


// Update the main connect button, as well as the connection indicator on the tab.
function updateLogoConnectState() {
  var newSrc, stoppedSrc, connectedSrc;
  stoppedSrc = $('.logo img').data('stopped-src');
  connectedSrc = $('.logo img').data('connected-src');

  if (g_lastState === 'connected') {
    newSrc = connectedSrc;
  }
  else {
    newSrc = stoppedSrc;
  }

  $('.logo img').prop('src', newSrc);
}


/* CONNECTION ****************************************************************/

// The current connected actual state of the application
var g_lastState = 'stopped';

// Used to monitor whether the current connection attempt is taking too long and
// so if a "download new version" message should be shown.
var g_connectingTooLongTimeout = null;

$(function connectionInit() {
  connectToggleSetup();
  egressRegionComboSetup();

  // Update the size of our elements when the tab content element resizes...
  $('.main-height').on('resize', function() {
    // Only if this tab is active
    if ($('#connection-pane').hasClass('active')) {
      nextTick(resizeConnectContent);
    }
  });
  // ...and when the tab is activated...
  $('a[href="#connection-pane"][data-toggle="tab"]').on('shown', function() {
    nextTick(resizeConnectContent);
  });
  // ...and now.
  resizeConnectContent();
});

function resizeConnectContent() {
  // Resize the text in the button
  $('.textfill-container').textfill({ maxFontPixels: -1 });

  // Set the outer connect button div to the correct height
  $('#connect-toggle').height($('#connect-toggle > *').outerHeight());

  // Center the connect button div
  var parentWidth = $('#connect-toggle').parent().innerWidth() -
                      (parseFloat($('#connect-toggle').parent().css('padding-left')) || 0) -
                      (parseFloat($('#connect-toggle').parent().css('padding-right')) || 0);
  if (g_isRTL) {
    $('#connect-toggle').css({
      right: ((parentWidth - $('#connect-toggle').outerWidth()) / 2.0)+'px',
      left: ''
    });
  }
  else {
    $('#connect-toggle').css({
      left: ((parentWidth - $('#connect-toggle').outerWidth()) / 2.0)+'px',
      right: ''
    });
  }
}

function connectToggleSetup() {
  var opts = {
    lines: 10, // The number of lines to draw
    length: 6, // The length of each line
    width: 2, // The line thickness
    radius: 6, // The radius of the inner circle
    corners: 1, // Corner roundness (0..1)
    rotate: 50, // The rotation offset
    direction: 1, // 1: clockwise, -1: counterclockwise
    color: ['#000', '#888', '#FFF'], // #rgb or #rrggbb or array of colors // TODO: Pick better colours
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

  $('.textfill-container').textfill({ maxFontPixels: -1 });

  // Update the button when the back-end tells us the state has changed.
  $window.on(CONNECTED_STATE_CHANGE_EVENT, nextTickFn(updateConnectToggle));
}

// Update the main connect button, as well as the connection indicator on the tab.
function updateConnectToggle() {
  $('.connect-toggle-content').each(function() {
    $(this).toggleClass('z-behind', $(this).data('connect-state') !== g_lastState);
  });

  $('a[href="#connection-pane"][data-toggle="tab"] [data-connect-state]').each(function() {
    $(this).toggleClass('z-behind', $(this).data('connect-state') !== g_lastState);
  });

  if (g_lastState === 'starting') {
    cycleToggleClass(
      $('.connect-toggle-content[data-connect-state="starting"] .icon-spin, .connect-toggle-content[data-connect-state="starting"] .state-word'),
      'in-progress',
      g_lastState);
  }
  else if (g_lastState === 'connected') {
  }
  else if (g_lastState === 'stopping') {
    cycleToggleClass(
      $('.connect-toggle-content[data-connect-state="stopping"] .icon-spin, .connect-toggle-content[data-connect-state="stopping"] .state-word'),
      'in-progress',
      g_lastState);
  }
  else if (g_lastState === 'stopped') {
  }

  updateConnectAttemptTooLong();
}

// Keeps track of how long the current connection attempt is taking and whether
// a message should be shown to the user indicating how to get a new version.
function updateConnectAttemptTooLong() {
  if (g_lastState === 'connected' || g_lastState === 'stopped') {
    // Clear the too-long timeout
    if (g_connectingTooLongTimeout !== null) {
      clearTimeout(g_connectingTooLongTimeout);
      g_connectingTooLongTimeout = null;
      connectAttemptTooLongReset();
    }
  }
  else {
    // Start the too-long timeout
    if (g_connectingTooLongTimeout === null) {
      g_connectingTooLongTimeout = setTimeout(connectAttemptTooLong, 60000);
    }
  }

  function connectAttemptTooLong() {
    $('.long-connecting-hide').addClass('hidden');
    $('.long-connecting-show').removeClass('hidden');
  }

  function connectAttemptTooLongReset() {
    $('.long-connecting-hide').removeClass('hidden');
    $('.long-connecting-show').addClass('hidden');
  }
}

function cycleToggleClass(elem, cls, untilStateChangeFrom) {
  $(elem).toggleClass(cls, 1000, function() {
    if (g_lastState === untilStateChangeFrom) {
      nextTick(function cycleToggleClass_inner() {
        cycleToggleClass(elem, cls, untilStateChangeFrom);
      });
    }
  });
}

function egressRegionComboSetup() {
  // Rather than duplicating the markup of the settings' egress region list,
  // we're going to copy it now.
  // IE7: Don't use $().clone().
  $('#EgressRegionCombo ul').html($('ul#EgressRegion').html());

  // When an item in the combo is clicked, make the settings code do the work.
  $('#EgressRegionCombo ul a').click(function egressRegionComboSetup_EgressRegionCombo_click(e) {
    e.preventDefault();
    var region = $(this).parents('[data-region]').data('region');
    if (!region) {
      return;
    }

    refreshSettings({
      EgressRegion: region
    });

    applySettings();
  });

  // Have the combobox track the state of the control in the settings pane.
  $('#EgressRegion').on('change', function egressRegionComboSetup_EgressRegion_change() {
    var $activeItem;
    // Copy the relevant classes to the combo items from the settings items.
    var $regionItems = $('#EgressRegion li');
    for (var i = 0; i < $regionItems.length; i++) {
      var $regionItem = $regionItems.eq(i);
      var region = $regionItem.data('region');
      var hidden = $regionItem.hasClass('hidden');
      var active = $regionItem.hasClass('active');

      $('#EgressRegionCombo li[data-region="' + region + '"]')
        .toggleClass('hidden', hidden)
        .toggleClass('active', active);

      if (active) {
        $activeItem = $regionItem;
      }
    }

    // Update the button
    if ($activeItem) {
      $('#EgressRegionCombo .btn span.flag')
        .attr('data-i18n', $activeItem.find('a').data('i18n'))
        .attr('class', $activeItem.find('a').attr('class'))
        .text($activeItem.find('a').text());
    }
  });

  // If the label is clicked, jump to the Egress Region settings section
  $('.egress-region-combo-container label a').click(function(e) {
    e.preventDefault();
    showSettingsSection('#settings-accordion-egress-region');
    return false;
  });
}


/* SETTINGS ******************************************************************/

// Event fired on #settings-pane when a setting is changed.
var SETTING_CHANGED_EVENT = 'setting-changed';

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
      SystrayMinimize: 0,
      defaults: {
        SplitTunnel: 0,
        VPN: 0,
        LocalHttpProxyPort: '',
        LocalSocksProxyPort: '',
        SkipUpstreamProxy: 0,
        UpstreamProxyHostname: '',
        UpstreamProxyPort: '',
        EgressRegion: '',
        SystrayMinimize: 0
      }
    };
  }

  // Event handlers
  $('#settings-pane').on(SETTING_CHANGED_EVENT, onSettingChanged);
  $('.settings-buttons .reset-settings').click(onSettingsReset);
  $('.settings-buttons .apply-settings').click(onSettingsApply);
  $('a[href="#settings-pane"][data-toggle="tab"]').on('shown', onSettingsTabShown);
  $('a[data-toggle="tab"]').on('show', function(e) {
    if ($('#settings-tab').hasClass('active') &&    // we were active
        !$(this).parent().is($('#settings-tab'))) { // we won't be active
      onSettingsTabHiding(e);
    }
  });

  // Change the accordion heading icon on expand/collapse
  $('.accordion-body')
    .on('show', function() {
      var headingSelector = '.accordion-toggle[href="#' + this.id + '"]';
      $(headingSelector).addClass('accordion-expanded');
      var $expandIcon = $(headingSelector).find('.accordion-expand-icon');
      $expandIcon.removeClass($expandIcon.data('icon-closed'))
                 .addClass($expandIcon.data('icon-opened'));

      // Remove focus from the heading to clear the text-decoration. (It's too
      // ham-fisted to do it in CSS.)
      $(headingSelector).blur();
    })
    .on('hide', function() {
      var headingSelector = '.accordion-toggle[href="#' + this.id + '"]';
      $(headingSelector).removeClass('accordion-expanded');
      var $expandIcon = $(headingSelector).find('.accordion-expand-icon');
      $expandIcon.removeClass($expandIcon.data('icon-opened'))
                 .addClass($expandIcon.data('icon-closed'));

      // Remove focus from the heading to clear the text-decoration. (It's too
      // ham-fisted to do it in CSS.)
      $(headingSelector).blur();
    });

  systrayMinimizeSetup();
  splitTunnelSetup();
  egressRegionSetup();
  localProxySetup();
  upstreamProxySetup();
  vpnModeSetup();

  updateAvailableEgressRegions(false); // don't force valid -- haven't filled in settings yet

  // Fill in the settings.
  refreshSettings();
});

//
// Overall event handlers
//

// Settings tab has been navigated to and is shown
function onSettingsTabShown() {
  // Reset the Apply button
  enableSettingsApplyButton(false);
}

// Settings tab is showing, but is being navigated away from.
// Call e.preventDefault() to prevent navigation away.
function onSettingsTabHiding(e) {
  // If the settings have been changed and not applied, prevent navigating away
  // and prompt the user for how it should be resolved.
  if (getSettingsApplyButtonEnabled()) {
    e.preventDefault();

    var $modal = $('#SettingsUnappliedChangesPrompt');

    $modal.modal({
      show: true,
      backdrop: 'static'
    });

    // Note: If we don't first remove existing click handlers (with .off), then
    // the handler that wasn't clicked last time will still be present.

    $modal.find('.apply-button').off('click').one('click', function() {
      $modal.modal('hide');
      if (applySettings()) {
        enableSettingsApplyButton(false);
        $(e.target).tab('show');
      }
      else {
        showSettingsErrorModal();
      }
    });

    $modal.find('.discard-button').off('click').one('click', function() {
      $modal.modal('hide');
      refreshSettings(g_initObj.Settings);
      enableSettingsApplyButton(false);
      $(e.target).tab('show');
    });
  }
}

// A setting value has been changed.
function onSettingChanged(e, id) {
  DEBUG_LOG('onSettingChanged: ' + id);

  var settingsValues = getSettingsTabValues();

  if (settingsValues === false) {
    // Settings values are invalid
    enableSettingsApplyButton(false);
  }
  else {
    var settingsChanged = settingsObjectChanged(settingsValues);
    enableSettingsApplyButton(settingsChanged);
  }
}

// Handler for the Reset Settings button
function onSettingsReset(e) {
  /*jshint validthis:true */

  e.preventDefault();
  $(this).blur();

  refreshSettings(g_initObj.Settings.defaults, false);

  // Force the Apply button to be enabled. Otherwise the user might be confused
  // about how to make the Reset take effect.
  enableSettingsApplyButton(true);
}

// Handler for the Apply Settings button
function onSettingsApply(e) {
  /*jshint validthis:true */

  e.preventDefault();
  $(this).blur();

  if (!getSettingsApplyButtonEnabled()) {
    return;
  }

  if (applySettings()) {
    // Reset the Apply button
    enableSettingsApplyButton(false);

    // Switch to Connection tab
    switchToTab('#connection-tab');
  }
  else {
    showSettingsErrorModal();
  }
}

//
// General settings functions
//

// Returns true if newSettings differs from the current canonical settings.
function settingsObjectChanged(newSettings) {
  // This does not take into account more/fewer keys in the object, since that
  // should not happen.

  var key, i;
  var keys = _.keys(g_initObj.Settings);
  for (i = 0; i < keys.length; i++) {
    key = keys[i];
    if (key === 'defaults') {
      // The defaults object doesn't count in the comparison.
      continue;
    }

    if (newSettings[key] !== g_initObj.Settings[key]) {
      DEBUG_LOG('settingsObjectChanged: detected change: ' + key);
      return true;
    }
  }

  DEBUG_LOG('settingsObjectChanged: no change');
  return false;
}

function enableSettingsApplyButton(enable) {
  var $applyButton = $('#settings-pane .apply-settings');
  var currentlyEnabled = getSettingsApplyButtonEnabled();

  if (enable && !currentlyEnabled) {
    $applyButton.removeClass('disabled').removeAttr('disabled');
    drawAttentionToButton($applyButton);
  }
  else if (!enable && currentlyEnabled) {
    $applyButton.addClass('disabled').attr('disabled', true);
  }
}

function getSettingsApplyButtonEnabled() {
  var $applyButton = $('#settings-pane .apply-settings');
  return !$applyButton.hasClass('disabled');
}

// Save the current settings (and possibly reconnect).
function applySettings() {
  var settingsValues = getSettingsTabValues();
  if (settingsValues === false) {
    // Settings are invalid.
    return false;
  }

  // Update settings in the application (and trigger a reconnect, if
  // necessary).
  HtmlCtrlInterface_SaveSettings(JSON.stringify(settingsValues));

  return true;
}

// Refresh the current settings. If newSettings is truthy, it will become the
// the new current settings, otherwise the existing current settings will be
// refreshed in the UI.
// newSettings can be a partial settings object (like, just {egressRegion: "US"} or whatever).
// If forceCurrent is true, the new settings will be become canonical (rather than just displayed).
function refreshSettings(newSettings, forceCurrent) {
  var fullNewSettings = $.extend(g_initObj.Settings, newSettings || {});

  if (forceCurrent) {
    g_initObj.Settings = fullNewSettings;
  }

  fillSettingsValues(fullNewSettings);

  // When the settings change, we need to check the current egress region choice.
  forceEgressRegionValid();
  // NOTE: If more checks like this are added, we'll need to chain them (somehow),
  // otherwise we'll have a mess of modals.
}

// Fill in the settings controls with the values in `obj`.
function fillSettingsValues(obj) {
  // Bit of a hack: Unhook the setting-changed event while we fill in the
  // values, then hook it up again after.
  $('#settings-pane').off(SETTING_CHANGED_EVENT, onSettingChanged);

  if (typeof(obj.SplitTunnel) !== 'undefined') {
    $('#SplitTunnel').prop('checked', !!obj.SplitTunnel);
  }

  if (typeof(obj.VPN) !== 'undefined') {
    $('#VPN').prop('checked', obj.VPN);
  }
  vpnModeUpdate();

  if (typeof(obj.LocalHttpProxyPort) !== 'undefined') {
    $('#LocalHttpProxyPort').val(obj.LocalHttpProxyPort > 0 ? obj.LocalHttpProxyPort : '');
  }

  if (typeof(obj.LocalSocksProxyPort) !== 'undefined') {
    $('#LocalSocksProxyPort').val(obj.LocalSocksProxyPort > 0 ? obj.LocalSocksProxyPort : '');
  }

  localProxyValid(false);

  if (typeof(obj.UpstreamProxyHostname) !== 'undefined') {
    $('#UpstreamProxyHostname').val(obj.UpstreamProxyHostname);
  }

  if (typeof(obj.UpstreamProxyPort) !== 'undefined') {
    $('#UpstreamProxyPort').val(obj.UpstreamProxyPort > 0 ? obj.UpstreamProxyPort : '');
  }

  if (typeof(obj.SkipUpstreamProxy) !== 'undefined') {
    $('#SkipUpstreamProxy').prop('checked', obj.SkipUpstreamProxy);
  }
  skipUpstreamProxyUpdate();

  upstreamProxyValid(false);

  if (typeof(obj.EgressRegion) !== 'undefined') {
    var region = obj.EgressRegion || BEST_REGION_VALUE;
    $('#EgressRegion [data-region]').removeClass('active');
    $('#EgressRegion').find('[data-region="' + region + '"] a').trigger(
      'click',
      { ignoreDisabled: true });
  }

  if (typeof(obj.SystrayMinimize) !== 'undefined') {
    $('#SystrayMinimize').prop('checked', !!obj.SystrayMinimize);
  }

  // Re-hook the setting-changed event
  $('#settings-pane').on(SETTING_CHANGED_EVENT, onSettingChanged);
}

// Extracts the values current set in the tab and returns and object with them.
// Returns false if the settings are invalid.
function getSettingsTabValues() {
  var valid = true;

  valid = valid && egressRegionValid(true);
  valid = valid && localProxyValid(true);
  valid = valid && upstreamProxyValid(true);

  if (!valid) {
    return false;
  }

  var egressRegion = $('#EgressRegion li.active').data('region');

  var returnValue = {
    VPN: $('#VPN').prop('checked') ? 1 : 0,
    SplitTunnel: $('#SplitTunnel').prop('checked') ? 1 : 0,
    LocalHttpProxyPort: validatePort($('#LocalHttpProxyPort').val()),
    LocalSocksProxyPort: validatePort($('#LocalSocksProxyPort').val()),
    UpstreamProxyHostname: $('#UpstreamProxyHostname').val(),
    UpstreamProxyPort: validatePort($('#UpstreamProxyPort').val()),
    SkipUpstreamProxy: $('#SkipUpstreamProxy').prop('checked') ? 1 : 0,
    EgressRegion: egressRegion === BEST_REGION_VALUE ? '' : egressRegion,
    SystrayMinimize: $('#SystrayMinimize').prop('checked') ? 1 : 0
  };

  return returnValue;
}

function showSettingsErrorModal() {
  showNoticeModal(
    'settings#error-modal#title',
    'settings#error-modal#body',
    null, null,
    function() {
      showSettingErrorSection();
    }
  );
}

function showSettingErrorSection() {
  showSettingsSection(
    $('#settings-accordion .collapse .error').parents('.collapse').eq(0),
    $('#settings-pane .error input').eq(0).focus());
}

//
// Systray Minimize
//

// Will be called exactly once. Set up event listeners, etc.
function systrayMinimizeSetup() {
  $('#SystrayMinimize').change(function() {
    // Tell the settings pane a change was made.
    $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
  });
}

//
// Split Tunnel
//

// Will be called exactly once. Set up event listeners, etc.
function splitTunnelSetup() {
  $('#SplitTunnel').change(function() {
    // Tell the settings pane a change was made.
    $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
  });
}

//
// Egress Region (Psiphon Server Region)
//

// Will be called exactly once. Set up event listeners, etc.
function egressRegionSetup() {
  // Handle changes to the Egress Region
  $('#EgressRegion a').click(function(e, extraArgs) {
    e.preventDefault();

    // Do nothing if we're disabled, unless we're forcing a disabled bypass
    if ($('#EgressRegion').hasClass('disabled') &&
        (!extraArgs || !extraArgs.ignoreDisabled)) {
      $(this).blur();
      return;
    }

    // Check if this target item is already active. Return if so.
    if ($(this).parents('[data-region]').hasClass('active')) {
      return;
    }

    $('#EgressRegion [data-region]').removeClass('active');
    $(this).parents('[data-region]').addClass('active');
    egressRegionValid(false);

    // This event helps the combobox on the connect pane stay in sync.
    $('#EgressRegion').trigger('change');

    // Tell the settings pane a change was made.
    $('#settings-pane').trigger(SETTING_CHANGED_EVENT, 'EgressRegion');
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

  // Put up the modal message
  showNoticeModal(
    'settings#egress-region#error-modal-title',
    'settings#egress-region#error-modal-body-http',
    null, null, null);

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

  $('#EgressRegion').trigger('change');
}

//
// Local Proxy Ports
//

// Will be called exactly once. Set up event listeners, etc.
function localProxySetup() {
  // Handle change events
  $('#LocalHttpProxyPort, #LocalSocksProxyPort').on(
      'keyup keydown keypress change blur',
      function(event) {
        // We need to delay this processing so that the change to the text has
        // had a chance to take effect. Otherwise this.val() will return the old
        // value.
        _.delay(_.bind(function(event) {
          // Tell the settings pane a change was made.
          $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);

          // Check for validity.
          localProxyValid(false);
        }, this, event), 100);
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

// Show an error modal telling the user there is a local port conflict problem
function localProxyPortConflictNotice(noticeType) {
  // Show the appropriate message depending on the error
  var bodyKey = noticeType === 'HttpProxyPortInUse' ?
                'settings#local-proxy-ports#error-modal-body-http' :
                'settings#local-proxy-ports#error-modal-body-socks';

  showNoticeModal(
    'settings#local-proxy-ports#error-modal-title',
    bodyKey,
    null, null, null);

  // Switch to the appropriate settings section
  showSettingsSection(
    '#settings-accordion-local-proxy-ports',
    noticeType === 'HttpProxyPortInUse' ? '#LocalHttpProxyPort' : '#LocalSocksProxyPort');
}

//
// Upstream Proxy
//

// Will be called exactly once. Set up event listeners, etc.
function upstreamProxySetup() {
  // Handle change events
  $('#UpstreamProxyHostname, #UpstreamProxyPort').on(
      'keyup keydown keypress change blur',
      function(event) {
        // We need to delay this processing so that the change to the text has
        // had a chance to take effect. Otherwise this.val() will return the old
        // value.
        _.delay(_.bind(function(event) {
          // Tell the settings pane a change was made.
          $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);

          // Check for validity.
          upstreamProxyValid(false);
        }, this, event), 100);
      });

  // Add the "skip" checkbox handler.
  $('#SkipUpstreamProxy').change(function(e) {
    // Trigger overall change event
    $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);

    skipUpstreamProxyUpdate();
  });
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

// The other upstream proxy settings should be disabled if skip-upstream-proxy
// is set.
function skipUpstreamProxyUpdate() {
  var skipUpstreamProxy = $('#SkipUpstreamProxy').prop('checked');
  $('.skip-upstream-proxy-incompatible input').prop('disabled', skipUpstreamProxy);
  $('.skip-upstream-proxy-incompatible').toggleClass('disabled-text', skipUpstreamProxy);
}

// The occurrence of an upstream proxy error might mean that a tunnel cannot
// ever be established, but not necessarily -- it might just be, for example,
// that the upstream proxy doesn't allow the port needed for one of our
// servers, but not all of them.
// So instead of showing an error immediately, we'll remember that the upstream
// proxy error occurred, wait a while to see if we connect successfully, and
// show it if we haven't connected.
// Potential enhancement: If the error modal is showing and the connection
// succeeds, dismiss the modal. This would be good behaviour, but probably too
// fringe to be worthwhile.
var g_upstreamProxyErrorNoticeTimer = null;

// When the connected state changes, we clear the timer.
$window.on(CONNECTED_STATE_CHANGE_EVENT, function() {
  if (g_upstreamProxyErrorNoticeTimer) {
    clearTimeout(g_upstreamProxyErrorNoticeTimer);
    g_upstreamProxyErrorNoticeTimer = null;
  }
});

function upstreamProxyErrorNotice(errorMessage) {
  if (g_upstreamProxyErrorNoticeTimer) {
    // We've already received an upstream proxy error and we're waiting to show it.
    return;
  }

  // This is the first upstream proxy error we've received, so start waiting to
  // show a message for it.
  g_upstreamProxyErrorNoticeTimer = setTimeout(function() {
    // It can happen that we receive a notice about an upstream error *after*
    // we have successfully connected -- it will have been received from one
    // of the parallel unsuccessful connection attempts. We should not show
    // such an error. So, as a general statement: Do not show upstream errors
    // if we're already successfully connected.
    if (g_lastState === 'connected') {
      if (g_upstreamProxyErrorNoticeTimer) {
        g_upstreamProxyErrorNoticeTimer = null;
      }
      return;
    }

    // There are two slightly different error messages shown depending on whether
    // there is an upstream proxy explicitly configured, or it's the default
    // empty value, which means the pre-existing system proxy will be used.
    var bodyKey = 'settings#upstream-proxy#error-modal-body-default';
    if (g_initObj.Settings.UpstreamProxyHostname) {
      bodyKey = 'settings#upstream-proxy#error-modal-body-configured';
    }

    showNoticeModal(
      'settings#upstream-proxy#error-modal-title',
      bodyKey,
      'general#notice-modal-tech-preamble',
      errorMessage,
      function() {
        $('#UpstreamProxyHostname').focus();
      });

    // Switch to the appropriate settings section
    showSettingsSection('#settings-accordion-upstream-proxy');

    // We are not going to set the timer to null here. We only want the error
    // to show once per connection attempt sequence. It will be cleared when
    // the client transistions to 'stopped' or 'connected'.
  }, 60000);
}

//
// VPN Mode (Transport Mode)
//

// Will be called exactly once. Set up event listeners, etc.
function vpnModeSetup() {
  // Some fields are disabled in VPN mode
  $('#VPN').change(function() {
    // Tell the settings pane a change was made.
    $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);

    vpnModeUpdate();
  });
}

// Some of the settings are incompatible with VPN mode. We'll modify the
// display depending on the choice of VPN mode.
function vpnModeUpdate() {
  var vpn = $('#VPN').prop('checked');
  $('input.vpn-incompatible, .vpn-incompatible input, '+
    'select.vpn-incompatible, .vpn-incompatible select, .vpn-incompatible .dropdown-menu')
      .prop('disabled', vpn).toggleClass('disabled', vpn);
  $('.vpn-incompatible-msg').toggleClass('hidden', !vpn);
  $('.vpn-incompatible').toggleClass('disabled-text', vpn);
  $('.vpn-incompatible-hide').toggleClass('hidden', vpn);
}

//
// Helpers
//

// Show/hide the error alert depending on whether we have an erroneous field
function updateErrorAlert() {
  $('#settings-pane .value-error-alert').toggleClass(
    'invisible z-behind', $('#settings-pane .control-group.error').length === 0);
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

// Show the Settings tab and expand the target section.
// If focusElem is optional; if set, focus will be put in that element.
function showSettingsSection(section, focusElem) {
  // We can only expand the section after the tab is shown
  function onTabShown() {
    // Hack: The collapse-show doesn't seem to work unless we wait a bit
    setTimeout(function() {
      // Expand the section
      $(section).collapse('show');

      // Collapse any other sections
      $('#settings-accordion .in.collapse').not(section).collapse('hide');

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

  // Make sure the settings tab is showing.
  switchToTab('#settings-tab', onTabShown);
}


/* FEEDBACK ******************************************************************/

$(function() {
  // Add click listener to the happy/sad choices
  $('.feedback-smiley .feedback-choice').click(function(e) {
    e.preventDefault();
    $('.feedback-smiley .feedback-choice').removeClass('selected');
    $(this).addClass('selected');
  });

  $('#feedback-submit').click(function(e) {
    e.preventDefault();
    sendFeedback();
  });

  // The sponsor-specific links in the text will be set correctly elsewhere.
});

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
  switchToTab('#connection-tab');

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


/* LOGS **********************************************************************/

$(function() {
  $('#show-debug-logs').click(showDebugLogsClicked);

  // Set the initial show-debug state
  var show = $('#show-debug-logs').prop('checked');
  $('.log-messages')
    .toggleClass('showing-priority-0', show)
    .toggleClass('hiding-priority-0', !show);
});

function showDebugLogsClicked(e) {
  /*jshint validthis:true */
  var show = $(this).prop('checked');
  // We use both a showing and a hiding class to try to deal with IE7's CSS insanity.
  $('.log-messages')
    .toggleClass('showing-priority-0', show)
    .toggleClass('hiding-priority-0', !show);
}

// Expects obj to be of the form {priority: 0|1|2, message: string}
function addLog(obj) {
  $('.log-messages .placeholder').remove();

  $('.log-messages').loadTemplate(
    $("#log-template"),
    {
      timestamp: new Date().toLocaleTimeString(),
      message: obj.message,
      priority: 'priority-' + obj.priority
    },
    {
      prepend:true
    });

  // The "Show Debug Logs" checkbox is hidden until we actually get a debug
  // message.
  if (obj.priority < 1) {
    $('#logs-pane .invisible').removeClass('invisible');
  }

  // Record the priorities we've seen.
  // We'll keep them in a (fake) Set.
  addLog.priorities = addLog.priorities || {};
  addLog.priorities[obj.priority] = true;

  // Don't allow the log messages list to grow forever!
  // We will keep 50 of each priority. (Arbitrarily. May need to revisit.)
  var MAX_LOGS_PER_PRIORITY = 50;
  var priorities = _.keys(addLog.priorities);
  for (var i = 0; i < priorities.length; i++) {
    $('.log-messages .priority-'+priorities[i]).slice(MAX_LOGS_PER_PRIORITY).remove();
  }
}

// Used for temporary debugging messages.
function DEBUG_LOG(msg) {
  if (!g_initObj.Config.Debug) {
    return;
  }

  msg = 'DEBUG: ' + JSON.stringify(msg);

  addLog({priority: 0, message: msg});

  if (IS_BROWSER) {
    console.log(msg);
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

// Other functions may need to know if we're currently using a RTL locale or not
var g_isRTL = false;

function switchLocale(locale, initial) {
  i18n.setLng(locale, function() {
    // This callback does not seem to called asynchronously (probably because
    // we're loading from an object and not a remote resource). But we want this
    // code to run after everything else is done, so we'll force it to be async.

    nextTick(function() {
      $('html').attr('lang', locale);
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
    });
  });

  //
  // Right-to-left languages need special consideration.
  //

  var rtl = RTL_LOCALES.indexOf(locale) >= 0;
  g_isRTL = rtl;

  $('body').attr('dir', rtl ? 'rtl' : 'ltr')
           .css('direction', rtl ? 'rtl' : 'ltr');

  // We'll use a data attribute to store classes which should only be used
  // for RTL and not LTR, and vice-versa.
  $('[data-i18n-rtl-classes]').each(function() {
    var ltrClasses = $(this).data('i18n-ltr-classes');
    var rtlClasses = $(this).data('i18n-rtl-classes');

    if (ltrClasses) {
      $(this).toggleClass(ltrClasses, !rtl);
    }

    if (rtlClasses) {
      $(this).toggleClass(rtlClasses, rtl);
    }
  });

  //
  // Update C code string table with new language values
  //

  // Iterate through the English keys, since we know it will be complete.
  var translation = window.PSIPHON.LOCALES.en.translation;
  var appBackendStringTable = {};
  for (var key in translation) {
    if (!translation.hasOwnProperty(key)) {
      continue;
    }

    if (key.indexOf('appbackend#') === 0) {
      appBackendStringTable[key] = i18n.t(key);
    }
  }

  HtmlCtrlInterface_AddStringTableItem(appBackendStringTable);
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

  var $localeListElem = $('#language-pane');

  for (var i = 0; i < locales.length; i++) {
    // If we're not in debug mode, don't output the dev locales
    if (!g_initObj.Config.Debug && locales[i].indexOf('dev') === 0) {
      continue;
    }

    $localeListElem.loadTemplate(
      $("#locale-template"),
      { localeCode: locales[i],
        localeName: window.PSIPHON.LOCALES[locales[i]].name},
      { append: true });
  }

  // Set up the click handlers
  $('.language-choice').click(function(e) {
    e.preventDefault();
    switchLocale($(this).attr('value'));
  });
}


/* ABOUT *********************************************************************/

// No special code. Sponsor-specific links are set elsewhere.


/* UI HELPERS ****************************************************************/

function displayCornerAlert(elem) {
  // Show -- and then hide -- the alert
  var appearAnimationTime = 300;
  var showingTime = 4000;
  var disappearAnimationTime = 1000;
  $(elem).toggle('fold', {horizFirst: true}, appearAnimationTime, function() {
    setTimeout(function() {
      $(elem).toggle('fold', {horizFirst: true}, disappearAnimationTime);
    }, showingTime);
  });
}

// Make the given tab visible. `tab` may be a selector, a DOM element, or a
// jQuery object. If `callback` is provided, it will be invoked when tab is shown.
function switchToTab(tab, callback) {
  var $tab = $(tab);
  if ($tab.hasClass('active')) {
    // Tab already showing.
    if (callback) {
      nextTick(callback);
    }
  }
  else {
    // Settings tab not already showing. Switch to it before expanding and scrolling.
    if (callback) {
      $tab.find('[data-toggle="tab"]').one('show', callback);
    }
    $tab.find('[data-toggle="tab"]').tab('show');
  }
}

// Shows a modal box.
// String table keys will be used for filling in the content. The "tech" values
// are optional. `techInfoString` is an explicit string.
// `closedCallback` is optional and will be called when the modal is closed.
function showNoticeModal(titleKey, bodyKey, techPreambleKey, techInfoString, closedCallback) {
  DEBUG_ASSERT(titleKey && bodyKey);

  var $modal = $('#NoticeModal');

  $modal.find('.modal-title').html(i18n.t(titleKey));
  $modal.find('.notice-modal-body').html(i18n.t(bodyKey));

  if (techPreambleKey && techInfoString) {
    $modal.find('.notice-modal-tech-preamble').html(i18n.t(techPreambleKey));
    $modal.find('.notice-modal-tech-info').text(techInfoString);
    $modal.find('.notice-modal-tech').removeClass('hidden');
  }
  else {
    $modal.find('.notice-modal-tech').addClass('hidden');
  }

  // Put up the modal
  $modal.modal({
    show: true,
    backdrop: 'static'
  }).one('hidden', function() {
    if (closedCallback) {
      closedCallback();
    }
  });
}

/* MISC HELPERS AND UTILITIES ************************************************/

// Support the `data-match-height` feature
function doMatchHeight() {
  var i, j, $elem, matchSelector, matchSelectorsToMaxHeight = {}, $matchSelectorMatches;
  var $elemsToChange = $('[data-match-height]');

  //
  // Reset previously adjusted heights; record the match selectors.
  //
  for (i = 0; i < $elemsToChange.length; i++) {
    $elem = $elemsToChange.eq(i);

    // Store the original padding, if we don't already have it.
    if (typeof($elem.data('match-height-orig-padding-top')) === 'undefined') {
      $elem.data('match-height-orig-padding-top', parseInt($elem.css('padding-top')))
           .data('match-height-orig-padding-bottom', parseInt($elem.css('padding-bottom')));
    }

    // Reset the padding to its original state
    $elem.css('padding-top', $elem.data('match-height-orig-padding-top'))
         .css('padding-bottom', $elem.data('match-height-orig-padding-bottom'));

    matchSelector = $elem.data('match-height');
    matchSelectorsToMaxHeight[matchSelector] = null;
  }

  //
  // Alter the heights.
  //
  for (i = 0; i < $elemsToChange.length; i++) {
    $elem = $elemsToChange.eq(i);
    matchSelector = $elem.data('match-height');

    // If we haven't already determined the max for this selector, calculate it
    if (matchSelectorsToMaxHeight[matchSelector] === null) {
      $matchSelectorMatches  = $(matchSelector);
      for (j = 0; j < $matchSelectorMatches.length; j++) {
        matchSelectorsToMaxHeight[matchSelector] = Math.max(
          matchSelectorsToMaxHeight[matchSelector],
          $matchSelectorMatches.eq(j).height());
      }
    }

    // Alter the height.
    matchHeight($elem, matchSelectorsToMaxHeight[matchSelector]);
  }

  function matchHeight($elem, height) {
    var heightDiff = height - $elem.height();
    if (heightDiff <= 0) {
      // Already at least big enough
      return;
    }

    var align = $elem.data('match-height-align');
    if (align === 'top') {
      $elem.css('padding-bottom', heightDiff + $elem.data('match-height-orig-padding-bottom'));
    }
    else if (align === 'bottom') {
      $elem.css('padding-top', heightDiff + $elem.data('match-height-orig-padding-top'));
    }
    else { // center
      $elem.css('padding-top', heightDiff/2 + $elem.data('match-height-orig-padding-top'))
           .css('padding-bottom', heightDiff/2 + $elem.data('match-height-orig-padding-bottom'));
    }
  }
}

// Support the `data-match-width` feature
function doMatchWidth() {
  var matchSelectors = [], $dataMatchElems, $elem,
      matchSelector, $matchMaxElems, matchSelectorMaxWidth, i, j;

  $dataMatchElems = $('[data-match-width]');

  // First reset the widths of all the elements we will be adjusting. Otherwise
  // the find-max-width will be tainted if an element-to-adjust is also part of
  // of the find-max-width selector.
  for (i = 0; i < $dataMatchElems.length; i++) {
    $elem = $dataMatchElems.eq(i);
    $elem.css('width', '');

    // Build up a list of the selectors used, with dependent ones at the end.
    matchSelector = $elem.data('match-width');
    if (matchSelectors.indexOf(matchSelector) < 0) {
      if ($elem.data('match-width-dependent') === 'true') {
        matchSelectors.push(matchSelector);
      }
      else {
        matchSelectors.unshift(matchSelector);
      }
    }
  }

  // Then collect the maximums for each of the selectors provided by data-match-width
  for (i = 0; i < matchSelectors.length; i++) {
    matchSelector = matchSelectors[i];

    matchSelectorMaxWidth = 0.0;
    $matchMaxElems = $(matchSelector);
    for (j = 0; j < $matchMaxElems.length; j++) {
      matchSelectorMaxWidth = Math.max(
                                matchSelectorMaxWidth,
                                $matchMaxElems.eq(j).outerWidth());
    }

    // Apply the max width to the target elements.
    $dataMatchElems = $('[data-match-width="' + matchSelector + '"]');
    for (j = 0; j < $dataMatchElems.length; j++) {
      $elem = $dataMatchElems.eq(j);
      $elem.width(getNetWidth($elem, matchSelectorMaxWidth));
    }
  }

  function getNetWidth($elem, grossWidth) {
    return grossWidth -
            (parseFloat($elem.css('padding-right')) || 0) -
            (parseFloat($elem.css('padding-left')) || 0) -
            (parseFloat($elem.css('border-right-width')) || 0) -
            (parseFloat($elem.css('border-left-width')) || 0) -
            (parseFloat($elem.css('margin-right')) || 0) -
            (parseFloat($elem.css('margin-left')) || 0);
  }
}

function getIEVersion() {
  // This is complicated by the fact that the MSHTML control uses a different
  // user agent string than browsers do.

  var ie7_10Match, ie11Match, edgeMatch, tridentMatch, msieMatch;

  var ieVersion = false;

  // User agents differ between browser and actual application, so we need
  // different checks

  if (IS_BROWSER) {
    // Some care needs to be taken to work with IE11+'s old-version test mode.
    // We can't just use Trident versions.

    // This will match IEv7-10.
    ie7_10Match = window.navigator.userAgent.match(/^Mozilla\/\d\.0 \(compatible; MSIE (\d+)/);
    // This will match IE11.
    ie11Match = window.navigator.userAgent.match(/Trident\/(\d+)/);
    // This will match Edgev1 (which we will consider IE12).
    edgeMatch = window.navigator.userAgent.match(/Edge\/(\d+)/);

    if (ie7_10Match) {
      ieVersion = parseInt(ie7_10Match[1]);
    }
    else if (ie11Match) {
      // Trident version is 4 versions behind IE version.
      ieVersion = parseInt(ie11Match[1]) + 4;
    }
    else if (edgeMatch) {
      ieVersion = parseInt(edgeMatch[1]);
    }
  }
  else {
    // This will match MSHTMLv8-11.
    tridentMatch = window.navigator.userAgent.match(/MSIE \d+.*Trident\/(\d+)/);
    // This will match MSHTMLv7. Note that it must be checked after the Trident match.
    msieMatch = window.navigator.userAgent.match(/MSIE (\d+)/);

    // This will match Edgev1 (which we will consider IE12).
    // Note that this hasn't been seen in the Wild. MSHTML on Win10 uses Trident/8,
    // which hits the above regex and returns version 12.
    edgeMatch = window.navigator.userAgent.match(/Edge\/(\d+)/);

    if (tridentMatch) {
      // Trident version is 4 versions behind IE version.
      ieVersion = parseInt(tridentMatch[1]) + 4;
    }
    else if (msieMatch) {
      ieVersion = parseInt(msieMatch[1]);
    }
    else if (edgeMatch) {
      ieVersion = parseInt(edgeMatch[1]);
    }
  }

  return ieVersion;
}

// `comparison` may be: 'eq', 'lt', 'gt', 'lte', 'gte'
function compareIEVersion(comparison, targetVersion, acceptNonIE) {
  var ieVersion = getIEVersion();

  if (ieVersion === false) {
    return acceptNonIE;
  }

  if (comparison === 'eq') {
    return ieVersion == targetVersion;
  }
  else if (comparison === 'lt') {
    return ieVersion < targetVersion;
  }
  else if (comparison === 'gt') {
    return ieVersion > targetVersion;
  }
  else if (comparison === 'lte') {
    return ieVersion <= targetVersion;
  }
  else if (comparison === 'gte') {
    return ieVersion >= targetVersion;
  }

  return false;
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


// Sets a short timeout and then calls `callback`
// `context` is optional. Will be `this` when `callback` is called.
function nextTick(callback, context) {
  nextTickFn(callback, context)();
}

// Creates a function that, when called, sets a short timeout and then calls `callback`
// `context` is optional. Will be `this` when `callback` is called.
function nextTickFn(callback, context) {
  return function nextTickFnInner() {
    var args = arguments;
    var runner = function() {
      callback.apply(context, args);
    };

    setTimeout(runner, 0);
  };
}

// Convert hex-style color (#FFFFFF or #FFF) to RGB style (rgb(255, 255, 255)).
// From http://stackoverflow.com/a/5624139/729729
function hexToRgb(hex) {
  // Expand shorthand form (e.g. "03F") to full form (e.g. "0033FF")
  var shorthandRegex = /^#?([a-f\d])([a-f\d])([a-f\d])$/i;
  hex = hex.replace(shorthandRegex, function(m, r, g, b) {
    return r + r + g + g + b + b;
  });

  var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return result ? {
    r: parseInt(result[1], 16),
    g: parseInt(result[2], 16),
    b: parseInt(result[3], 16)
  } : null;
}

// Draws attention to a button. Note that this may require some extra styling
// to fully work (so test accordingly).
function drawAttentionToButton(elem) {
  // We will draw attention by temporarily flaring a box-shadow, and then
  // wiggling the button.

  var backgroundColor, backgroundColorRGB, shadowColor;
  var $elem = $(elem);

  var originalBoxShadow = $elem.data('drawAttentionToButton-originalBoxShadow');
  if (!originalBoxShadow) {
    originalBoxShadow = $elem.css('box-shadow');
    $elem.data('drawAttentionToButton-originalBoxShadow', originalBoxShadow);
  }

  // The box shadow will be same colour as the button background, with reduced
  // opacity.

  backgroundColor = $elem.css('background-color');
  if (!backgroundColor) {
    backgroundColor = '#999';
  }

  if (backgroundColor[0] === '#') {
    // hex style; convert to rgb
    backgroundColorRGB = hexToRgb(backgroundColor);
    backgroundColor = 'rgb(' + backgroundColorRGB.r + ', ' +
                               backgroundColorRGB.g + ', ' +
                               backgroundColorRGB.b + ')';
  }

  // Only add alpha if this is a rgb(..) colour, and not rgba(...) or anything else.
  if (backgroundColor.slice(0, 4) === 'rgb(') {
    shadowColor = 'rgba(' + backgroundColor.slice(4, -1) + ', 0.7)';
  }
  else {
    shadowColor = backgroundColor;
  }

  // The effect we're going for is the color draining from outside into the button.
  var animationTimeSecs = 3;

  // Add the initial internal and external shadow
  $elem.css({
    'transition': '',
    'box-shadow': 'inset 0 0 100px #FFF, 0 0 10px ' + shadowColor
  });

  // After giving that CSS change a moment to take effect...
  _.delay(function() {
    // ...add a transition and remove the box shadow. It will disappear slowly.
    $elem.css({
      'transition': 'box-shadow ' + animationTimeSecs + 's',
      'box-shadow': 'none'
    });

    // When the big box shadow is gone, reset to the original value
    $elem.one('transitionend', function() {
      $elem.css({
        'transition': '',
        'box-shadow': originalBoxShadow
      });
    });
  }, 10); // if this is too low, the effect seems to fail sometimes (like when a port number field is changed in settings)
}

function DEBUG_ASSERT(check) {
  if (g_initObj.Config.Debug) {
    if (typeof(check) === 'function') {
      check = check();
    }

    if (!check) {
      throw new Exception('DEBUG_ASSERT failed: ' + check);
    }
  }
}


/* DEBUGGING *****************************************************************/

// Some functionality to help us debug (and demo) in browser.

$(function debugInit() {
  if (!g_initObj.Config.Debug) {
    $('#debug-tab, #debug-pane').remove();
    return;
  }

  // Make the connect button "work" in browser mode
  $('#connect-toggle a').click(function(e) {
    if (!IS_BROWSER) {
      return;
    }

    e.preventDefault();
    var buttonConnectState = $(this).parents('.connect-toggle-content').data('connect-state');
    if (buttonConnectState === 'stopped') {
      console.log('DEBUG: connection starting');
      HtmlCtrlInterface_SetState({state: 'starting'});
      setTimeout(function() {
        HtmlCtrlInterface_SetState({state: 'connected'});
      }, 5000);
    }
    else if (buttonConnectState === 'starting' || buttonConnectState === 'connected') {
      console.log('DEBUG: connection stopping');
      HtmlCtrlInterface_SetState({state: 'stopping'});
      setTimeout(function() {
        HtmlCtrlInterface_SetState({state: 'stopped'});
      }, 5000);
    }
    // the stopping button is disabled
  });

  // Keep state combo up-to-date
  function updateStateCombo() {
    $('#debug-state').val(g_lastState);
  }
  $window.on(CONNECTED_STATE_CHANGE_EVENT, updateStateCombo);
  updateStateCombo();
  // Change state when combo changes
  $('#debug-state').on('change', function() {
    HtmlCtrlInterface_SetState({state: $('#debug-state').val()});
  });

  // Wire up AddLog
  $('#debug-log a').on('click', function() {
    HtmlCtrlInterface_AddLog({
      message: $('#debug-log input').val(),
      priority: parseInt($('#debug-log select').val())
    });
  });

  // Wire up the AvailableEgressRegions notice
  $('#debug-AvailableEgressRegions a').on('click', function() {
    var regions = [], regionCheckboxes = $('#debug-AvailableEgressRegions input');
    for (var i = 0; i < regionCheckboxes.length; i++) {
      if (regionCheckboxes.eq(i).prop('checked')) {
        regions.push(regionCheckboxes.eq(i).val());
      }
    }
    HtmlCtrlInterface_AddNotice({
      noticeType: 'AvailableEgressRegions',
      data: { regions: regions }
    });
  });

  // Wire up the UpstreamProxyError notice
  $('#debug-UpstreamProxyError a').on('click', function() {
    HtmlCtrlInterface_AddNotice({
      noticeType: 'UpstreamProxyError',
      data: { message: $('#debug-UpstreamProxyError input').val() }
    });
  });

  // Wire up the HttpProxyPortInUse notice
  $('#debug-HttpProxyPortInUse a').on('click', function() {
    HtmlCtrlInterface_AddNotice({
      noticeType: 'HttpProxyPortInUse'
    });
  });

  // Wire up the SocksProxyPortInUse notice
  $('#debug-SocksProxyPortInUse a').on('click', function() {
    HtmlCtrlInterface_AddNotice({
      noticeType: 'SocksProxyPortInUse'
    });
  });

  // Wire up the SystemProxySettings::SetProxyError notice
  $('#debug-SetProxyError a').on('click', function() {
    HtmlCtrlInterface_AddNotice({
      noticeType: 'SystemProxySettings::SetProxyError'
    });
  });

  // Wire up the SystemProxySettings::SetProxyWarning notice
  $('#debug-SetProxyWarning a').on('click', function() {
    HtmlCtrlInterface_AddNotice({
      noticeType: 'SystemProxySettings::SetProxyWarning',
      data: '[CONN NAME]'
    });
  });

  // Wire up the SocksProxyPortInUse notice
  $('#debug-UpdateDpiScaling a').on('click', function() {
    HtmlCtrlInterface_UpdateDpiScaling({
      dpiScaling: $('#debug-UpdateDpiScaling input').val()
    });
  });
});


/* INTERFACE METHODS *********************************************************/

var PSIPHON_LINK_PREFIX = 'psi:';

/* Calls from C code to JS code. */

// Add new status message.
function HtmlCtrlInterface_AddLog(jsonArgs) {
  nextTick(function() {
    // Allow object as input to assist with debugging
    var args = (typeof(jsonArgs) === 'object') ? jsonArgs : JSON.parse(jsonArgs);
    addLog(args);
  });
}

// Add new notice. This may be interpreted and acted upon.
function HtmlCtrlInterface_AddNotice(jsonArgs) {
  nextTick(function() {
    // Allow object as input to assist with debugging
    var args = (typeof(jsonArgs) === 'object') ? jsonArgs : JSON.parse(jsonArgs);
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
    else if (args.noticeType === 'SystemProxySettings::SetProxyError') {
      showNoticeModal(
        'notice#systemproxysettings-setproxy-error-title',
        'notice#systemproxysettings-setproxy-error-body',
        null, null, null);
    }
    else if (args.noticeType === 'SystemProxySettings::SetProxyWarning') {
      var setProxyWarningTemplate = i18n.t('notice#systemproxysettings-setproxy-warning-template');
      addLog({
        priority: 2, // high
        message: _.template(setProxyWarningTemplate)({data: args.data})
      });
    }
  });
}

// Set the connected state.
// We will de-bounce the state change messages.
var g_timeoutSetState = null;
function HtmlCtrlInterface_SetState(jsonArgs) {
  // Clear any queued state changes.
  if (g_timeoutSetState !== null) {
    clearTimeout(g_timeoutSetState);
    g_timeoutSetState = null;
  }

  g_timeoutSetState = setTimeout(function() {
    g_timeoutSetState = null;

    // Allow object as input to assist with debugging
    var args = (typeof(jsonArgs) === 'object') ? jsonArgs : JSON.parse(jsonArgs);
    g_lastState = args.state;
    $window.trigger(CONNECTED_STATE_CHANGE_EVENT);
  }, 100);
}

// Refresh the current settings values.
function HtmlCtrlInterface_RefreshSettings(jsonArgs) {
  nextTick(function() {
    var args = JSON.parse(jsonArgs);
    refreshSettings(args.settings, true);

    if (args.success){
      if (args.reconnectRequired) {
        // backend is reconnecting to apply the settings
        displayCornerAlert($('#settings-apply-alert'));
      }
      else {
        displayCornerAlert($('#settings-save-alert'));
      }
    }
    // else an error occurred when saving settings... TODO: tell user?
  });
}

// Indicate a DPI-based scaling change.
function HtmlCtrlInterface_UpdateDpiScaling(jsonArgs) {
  // Allow object as input to assist with debugging
  var args = (typeof(jsonArgs) === 'object') ? jsonArgs : JSON.parse(jsonArgs);
  nextTick(function() {
    updateDpiScaling(args.dpiScaling);
  });
}

/* Calls from JS code to C code. */

// Let the C code know that the UI is ready.
function HtmlCtrlInterface_AppReady() {
  nextTick(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'ready';
    if (IS_BROWSER) {
      console.log(appURL);
    }
    else {
      window.location = appURL;
    }
  });
}

// Give the C code a string table entry in the appropriate language.
// The `stringtable` can and should be a full set of key:string mappings, but
// the strings will be sent to the C code one at a time, to prevent URL size
// overflow.
function HtmlCtrlInterface_AddStringTableItem(stringtable) {
  for (var key in stringtable) {
    if (!stringtable.hasOwnProperty(key)) {
      continue;
    }

    var item = {
      key: key,
      string: stringtable[key]
    };
    sendStringTableItem(item);
  }

  function sendStringTableItem(itemObj) {
    nextTick(function() {
      var appURL = PSIPHON_LINK_PREFIX + 'stringtable?' + encodeURIComponent(JSON.stringify(itemObj));
      if (IS_BROWSER) {
        console.log(decodeURIComponent(appURL));
      }
      else {
        window.location = appURL;
      }
    });
  }
}

// Connection should start.
function HtmlCtrlInterface_Start() {
  // Prevent duplicate state change attempts
  if (g_lastState === 'starting' || g_lastState === 'connected') {
    return;
  }
  nextTick(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'start';
    if (IS_BROWSER) {
      console.log(appURL);
    }
    else {
      window.location = appURL;
    }
  });
}

// Connection should stop.
function HtmlCtrlInterface_Stop() {
  // Prevent duplicate state change attempts
  if (g_lastState === 'stopping' || g_lastState === 'disconnected') {
    return;
  }
  nextTick(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'stop';
    if (IS_BROWSER) {
      console.log(appURL);
    }
    else {
      window.location = appURL;
    }
  });
}

// Settings should be saved.
function HtmlCtrlInterface_SaveSettings(settingsJSON) {
  nextTick(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'savesettings?' + encodeURIComponent(settingsJSON);
    if (IS_BROWSER) {
      console.log(decodeURIComponent(appURL));

      // DEBUG: Make it appear to behave like a real client
      _.delay(HtmlCtrlInterface_RefreshSettings, 100, JSON.stringify({
        settings: JSON.parse(settingsJSON),
        success: true,
        reconnectRequired: g_lastState === 'connected' || g_lastState === 'starting'
      }));
    }
    else {
      window.location = appURL;
    }
  });
}

// Feedback should be sent.
function HtmlCtrlInterface_SendFeedback(feedbackJSON) {
  nextTick(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'sendfeedback?' + encodeURIComponent(feedbackJSON);
    if (IS_BROWSER) {
      console.log(decodeURIComponent(appURL));
    }
    else {
      window.location = appURL;
    }
  });
}

// Cookies (i.e., UI settings) should be saved.
function HtmlCtrlInterface_SetCookies(cookiesJSON) {
  nextTick(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'setcookies?' + encodeURIComponent(cookiesJSON);
    if (IS_BROWSER) {
      console.log(decodeURIComponent(appURL));
    }
    else {
      window.location = appURL;
    }
  });
}

// Banner was clicked.
function HtmlCtrlInterface_BannerClick() {
  nextTick(function() {
    var appURL = PSIPHON_LINK_PREFIX + 'bannerclick';
    if (IS_BROWSER) {
      console.log(decodeURIComponent(appURL));
      alert('Call from JS to C to launch banner URL');
    }
    else {
      window.location = appURL;
    }
  });
}


/* EXPORTS */

// The C interface code is unable to access functions that are members of objects,
// so we'll need to directly expose our exports.

window.HtmlCtrlInterface_AddLog = HtmlCtrlInterface_AddLog;
window.HtmlCtrlInterface_SetState = HtmlCtrlInterface_SetState;
window.HtmlCtrlInterface_AddNotice = HtmlCtrlInterface_AddNotice;
window.HtmlCtrlInterface_RefreshSettings = HtmlCtrlInterface_RefreshSettings;
window.HtmlCtrlInterface_UpdateDpiScaling = HtmlCtrlInterface_UpdateDpiScaling;

})(window);
