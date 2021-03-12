"use strict";

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
(function (window) {
  "use strict";
  /* jshint strict:true, newcap:false */

  /* GENERAL */

  var $window = $(window); // Fired on the window when the UI language changes

  var LANGUAGE_CHANGE_EVENT = 'language-change'; // Fired on the window when HTML interface declares itself ready.

  var UI_READY_EVENT = 'ui-ready'; // Fired on the window when the connected state changes

  var CONNECTED_STATE_CHANGE_EVENT = 'connected-state-change'; // We often test in-browser and need to behave a bit differently

  var IS_BROWSER = true; // Parse whatever JSON parameters were passed by the application.

  var g_initObj = {
    Cookies: {}
  };

  (function () {
    var uriHash = location.hash;

    if (uriHash && uriHash.length > 1) {
      g_initObj = JSON.parse(decodeURIComponent(uriHash.slice(1)));
      IS_BROWSER = false;
    } // For browser debugging


    if (IS_BROWSER) {
      g_initObj = g_initObj || {};
      g_initObj.Config = g_initObj.Config || {};
      g_initObj.Config.ClientVersion = g_initObj.Config.ClientVersion || '99';
      g_initObj.Config.Language = g_initObj.Config.Language || 'en';
      g_initObj.Config.Banner = g_initObj.Config.Banner || 'banner.png';
      g_initObj.Config.InfoURL = g_initObj.Config.InfoURL || 'http://example.com/browser-InfoURL/index.html';
      g_initObj.Config.NewVersionEmail = g_initObj.Config.NewVersionEmail || 'browser-NewVersionEmail@example.com';
      g_initObj.Config.NewVersionURL = g_initObj.Config.NewVersionURL || 'http://example.com/browser-NewVersionURL/en/download.html#direct';
      g_initObj.Config.FaqURL = g_initObj.Config.FaqURL || 'http://example.com/browser-FaqURL/en/faq.html';
      g_initObj.Config.DataCollectionInfoURL = g_initObj.Config.DataCollectionInfoURL || 'http://example.com/browser-DataCollectionInfoURL/en/privacy.html#information-collected';
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
    } // Set the logo "info" link


    $('.logo a').attr('href', g_initObj.Config.InfoURL).attr('title', g_initObj.Config.InfoURL); // Update the logo when the connected state changes

    $window.on(CONNECTED_STATE_CHANGE_EVENT, updateLogoConnectState);
    updateLogoConnectState(); // The banner image filename is parameterized.

    $('.banner img').attr('src', g_initObj.Config.Banner); // Let the C-code decide what should be opened when the banner is clicked.

    $('.banner a').click(function (e) {
      e.preventDefault();
      HtmlCtrlInterface_BannerClick();
    }); // Links to the download site and email address are parameterized and need to
    // be updated when the language changes.

    var updateLinks = nextTickFn(function updateLinks() {
      // For some languages we alter the "download site" links to point directly
      // to that language. But the site has different available languages than
      // this application does, so we don't just do it blindly.
      var defaultLang = 'en';
      var siteLangs = ['fa', 'zh'];
      var currentLang = i18n.lng();
      var replaceLang = _.includes(siteLangs, currentLang) ? currentLang : defaultLang;
      var url; // Note that we're using the function-as-replacement form for String.replace()
      // because we don't entirely control the content of the language names, and
      // we don't want to run into any issues with magic values:
      // https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/String/replace#Specifying_a_string_as_a_parameter

      var replaceFn = function replaceFn(match, p1, p2) {
        return p1 + '/' + replaceLang + '/' + p2;
      }; // This link may be to the redirect meta page (/index.html) or to a language-specific
      // page (/en/index.html). If it's to the meta page, we won't force to English, otherwise we will.
      // First change it to the meta page if it's not already


      url = g_initObj.Config.InfoURL.replace('/en/', '/'); // Then force the language, but not to English

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
      $('.DataCollectionInfoURL').attr('href', url).attr('title', url); // No replacement on the email address

      $('.NewVersionEmail').attr('href', 'mailto:' + g_initObj.Config.NewVersionEmail).text(g_initObj.Config.NewVersionEmail).attr('title', g_initObj.Config.NewVersionEmail);
      $('.ClientVersion').text(g_initObj.Config.ClientVersion);
    });
    $window.on(LANGUAGE_CHANGE_EVENT, updateLinks); // ...and now.

    updateLinks(); // Update the size of our tab content element when the window resizes...

    var lastWindowHeight = $window.height();
    var lastWindowWidth = $window.width();
    $window.smartresize(function () {
      // Only go through the resize logic if the window actually changed size.
      // This helps with the constant resize events we get with IE7.
      if (lastWindowHeight !== $window.height() || lastWindowWidth !== $window.width()) {
        lastWindowHeight = $window.height();
        lastWindowWidth = $window.width();
        nextTick(resizeContent);
      }
    }); // ...and when a tab is activated...

    $('a[data-toggle="tab"]').on('shown', function () {
      nextTick(resizeContent);
    }); // ...and when the language changes...

    $window.on(LANGUAGE_CHANGE_EVENT, nextTickFn(resizeContent)); // ...and now.

    resizeContent(); // We don't want buggy scrolling behaviour (which can result from some click-drag selecting)

    initScrollFix();
    setTimeout(HtmlCtrlInterface_AppReady, 100);
  });

  function resizeContent() {
    DEBUG_LOG('resizeContent called'); // Do DPI scaling

    updateDpiScaling(g_initObj.Config.DpiScaling, false); // We want the content part of our window to fill the window, we don't want
    // excessive scroll bars, etc. It's difficult to do "fill the remaining height"
    // with just CSS, so we're going to do some on-resize height adjustment in JS.

    var fillHeight = $window.innerHeight() - $('.main-height').position().top;
    var footerHeight = $('.footer').outerHeight();
    $('.main-height').outerHeight((fillHeight - footerHeight) / g_initObj.Config.DpiScaling);
    $('.main-height').parentsUntil('body').add($('.main-height').siblings()).css('height', '100%'); // Let the panes know that content resized

    $('.main-height').trigger('resize');
    doMatchHeight();
    doMatchWidth(); // Adjust the banner to account for the logo space.

    $('.banner').css(g_isRTL ? 'margin-right' : 'margin-left', $('.header-nav-join').outerWidth()).css(!g_isRTL ? 'margin-right' : 'margin-left', 0);
  }

  function updateDpiScaling(dpiScaling, andResizeContent
  /*=true*/
  ) {
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
        bottomRightTransformOrigin = g_isRTL ? rtlBottomRightTransformOrigin : ltrBottomRightTransformOrigin; // Set the overall body scaling

    $('html').css({
      'transform-origin': transformOrigin,
      'transform': 'scale(' + dpiScaling + ')',
      'width': (100.0 / dpiScaling).toFixed(1) + '%',
      'height': (100.0 / dpiScaling).toFixed(1) + '%'
    }); // For elements (like modals) outside the normal flow, additional changes are needed.

    if (getIEVersion() !== false) {
      // The left margin will vary depending on default value.
      // First reset an overridden left margin
      $('.modal').css('margin-left', ''); // Get the default left margin

      var defaultLeftMargin = $('.modal').css('margin-left'); // Create the left margin we want, based on the default

      var scaledLeftMargin = 'calc(' + defaultLeftMargin + ' * ' + dpiScaling + ')'; // Now apply the styles.

      $('.modal').css({
        'transform-origin': transformOrigin,
        'transform': 'scale(' + dpiScaling + ')',
        'margin-left': scaledLeftMargin
      });
    }

    $('.global-alert').css({
      'transform-origin': bottomRightTransformOrigin,
      'transform': 'scale(' + dpiScaling + ')'
    }); // Elements with the `affix` class are position:fixed and need to be adjusted separately

    if (getIEVersion() !== false) {
      // Reset previous position modification.
      $('.affix').css({
        'top': '',
        'left': ''
      }).each(function () {
        var basePosition = $(this).position();
        $(this).css({
          'transform-origin': transformOrigin,
          'transform': 'scale(' + dpiScaling + ')'
        });

        if (basePosition.top) {
          $(this).css({
            'top': basePosition.top * dpiScaling + 'px'
          });
        }

        if (basePosition.left) {
          $(this).css({
            'left': basePosition.left * dpiScaling + 'px'
          });
        }
      });
    }

    if (andResizeContent !== false) {
      // Need to resize everything.
      nextTick(resizeContent);
    }
  } // Ensures that elements that should not be scrolled are not scrolled.
  // This should only be called once.


  function initScrollFix() {
    // It would be much better to fix this correctly using CSS rather than
    // detecting a bad state and correcting it. But until we figure that out...
    $('body').scroll(function () {
      $('body').scrollTop(0);
    });
  } // Update the main connect button, as well as the connection indicator on the tab.


  function updateLogoConnectState() {
    var newSrc, stoppedSrc, connectedSrc;
    stoppedSrc = $('.logo img').data('stopped-src');
    connectedSrc = $('.logo img').data('connected-src');

    if (g_lastState === 'connected') {
      newSrc = connectedSrc;
    } else {
      newSrc = stoppedSrc;
    }

    $('.logo img').prop('src', newSrc);
  }
  /* CONNECTION ****************************************************************/
  // The current connected actual state of the application


  var g_lastState = 'stopped'; // Used to monitor whether the current connection attempt is taking too long and
  // so if a "download new version" message should be shown.

  var g_connectingTooLongTimeout = null;
  $(function connectionInit() {
    connectToggleSetup();
    egressRegionComboSetup(); // Update the size of our elements when the tab content element resizes...

    $('.main-height').on('resize', function () {
      // Only if this tab is active
      if ($('#connection-pane').hasClass('active')) {
        nextTick(resizeConnectContent);
      }
    }); // ...and when the tab is activated...

    $('a[href="#connection-pane"][data-toggle="tab"]').on('shown', function () {
      nextTick(resizeConnectContent);
    }); // ...and now.

    resizeConnectContent();
  });

  function resizeConnectContent() {
    // Resize the text in the button
    $('.textfill-container').textfill({
      maxFontPixels: -1
    }); // Set the outer connect button div to the correct height

    $('#connect-toggle').height($('#connect-toggle > *').outerHeight()); // Center the connect button div

    var parentWidth = $('#connect-toggle').parent().innerWidth() - (parseFloat($('#connect-toggle').parent().css('padding-left')) || 0) - (parseFloat($('#connect-toggle').parent().css('padding-right')) || 0);

    if (g_isRTL) {
      $('#connect-toggle').css({
        right: (parentWidth - $('#connect-toggle').outerWidth()) / 2.0 + 'px',
        left: ''
      });
    } else {
      $('#connect-toggle').css({
        left: (parentWidth - $('#connect-toggle').outerWidth()) / 2.0 + 'px',
        right: ''
      });
    }
  }

  function connectToggleSetup() {
    $('#connect-toggle a').click(function (e) {
      e.preventDefault();
      var buttonConnectState = $(this).parents('.connect-toggle-content').data('connect-state');

      if (buttonConnectState === 'stopped') {
        HtmlCtrlInterface_StartTunnel();
      } else if (buttonConnectState === 'starting' || buttonConnectState === 'connected') {
        HtmlCtrlInterface_StopTunnel();
      } // the stopping button is disabled

    });
    updateConnectToggle();
    $('.textfill-container').textfill({
      maxFontPixels: -1
    }); // Update the button when the back-end tells us the state has changed.

    $window.on(CONNECTED_STATE_CHANGE_EVENT, nextTickFn(updateConnectToggle));
  } // Update the main connect button, as well as the connection indicator on the tab.


  function updateConnectToggle() {
    $('.connect-toggle-content').each(function () {
      $(this).toggleClass('z-behind', $(this).data('connect-state') !== g_lastState);
    });
    $('a[href="#connection-pane"][data-toggle="tab"] [data-connect-state]').each(function () {
      $(this).toggleClass('z-behind', $(this).data('connect-state') !== g_lastState);
    });

    if (g_lastState === 'starting') {
      cycleToggleClass($('.connect-toggle-content[data-connect-state="starting"] .icon-spin, .connect-toggle-content[data-connect-state="starting"] .state-word'), 'in-progress', g_lastState);
    } else if (g_lastState === 'connected') {// No additional work
    } else if (g_lastState === 'stopping') {
      cycleToggleClass($('.connect-toggle-content[data-connect-state="stopping"] .icon-spin, .connect-toggle-content[data-connect-state="stopping"] .state-word'), 'in-progress', g_lastState);
    } else if (g_lastState === 'stopped') {// No additional work
    }

    updateConnectAttemptTooLong();
  } // Keeps track of how long the current connection attempt is taking and whether
  // a message should be shown to the user indicating how to get a new version.


  function updateConnectAttemptTooLong() {
    if (g_lastState === 'connected' || g_lastState === 'stopped') {
      // Clear the too-long timeout
      if (g_connectingTooLongTimeout !== null) {
        clearTimeout(g_connectingTooLongTimeout);
        g_connectingTooLongTimeout = null;
        connectAttemptTooLongReset();
      }
    } else {
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
    $(elem).toggleClass(cls, 1000, function () {
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
    $('#EgressRegionCombo ul').html($('ul#EgressRegion').html()); // When an item in the combo is clicked, make the settings code do the work.

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
    }); // Have the combobox track the state of the control in the settings pane.

    $('#EgressRegion').on('change', function egressRegionComboSetup_EgressRegion_change() {
      var $activeItem; // Copy the relevant classes to the combo items from the settings items.

      var $regionItems = $('#EgressRegion li');

      for (var i = 0; i < $regionItems.length; i++) {
        var $regionItem = $regionItems.eq(i);
        var region = $regionItem.data('region');
        var hidden = $regionItem.hasClass('hidden');
        var active = $regionItem.hasClass('active');
        $('#EgressRegionCombo li[data-region="' + region + '"]').toggleClass('hidden', hidden).toggleClass('active', active);

        if (active) {
          $activeItem = $regionItem;
        }
      } // Update the button


      if ($activeItem) {
        $('#EgressRegionCombo .btn span.flag').attr('data-i18n', $activeItem.find('a').data('i18n')).attr('class', $activeItem.find('a').attr('class')).text($activeItem.find('a').text());
      }
    }); // If the label is clicked, jump to the Egress Region settings section

    $('.egress-region-combo-container label a').click(function (e) {
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
    if (!g_initObj.Settings) {
      g_initObj.Settings = {
        SplitTunnel: 0,
        DisableTimeouts: 0,
        VPN: 0,
        LocalHttpProxyPort: 7771,
        LocalSocksProxyPort: 7770,
        ExposeLocalProxiesToLAN: 1,
        SkipUpstreamProxy: 1,
        UpstreamProxyHostname: 'upstreamhost',
        UpstreamProxyPort: 234,
        UpstreamProxyUsername: 'user',
        UpstreamProxyPassword: 'password',
        UpstreamProxyDomain: 'domain',
        EgressRegion: 'GB',
        SystrayMinimize: 0,
        DisableDisallowedTrafficAlert: 0,
        defaults: {
          SplitTunnel: 0,
          DisableTimeouts: 0,
          VPN: 0,
          LocalHttpProxyPort: '',
          LocalSocksProxyPort: '',
          ExposeLocalProxiesToLAN: 0,
          SkipUpstreamProxy: 0,
          UpstreamProxyHostname: '',
          UpstreamProxyPort: '',
          UpstreamProxyUsername: '',
          UpstreamProxyPassword: '',
          UpstreamProxyDomain: '',
          EgressRegion: '',
          SystrayMinimize: 0,
          DisableDisallowedTrafficAlert: 0
        }
      };
    } // Event handlers


    $('#settings-pane').on(SETTING_CHANGED_EVENT, onSettingChanged);
    $('.settings-buttons .reset-settings').click(onSettingsReset);
    $('.settings-buttons .apply-settings').click(onSettingsApply);
    $('a[href="#settings-pane"][data-toggle="tab"]').on('shown', onSettingsTabShown);
    $('a[data-toggle="tab"]').on('show', function (e) {
      if ($('#settings-tab').hasClass('active') && // we were active
      !$(this).parent().is($('#settings-tab'))) {
        // we won't be active
        onSettingsTabHiding(e);
      }
    }); // Change the accordion heading icon on expand/collapse

    $('.accordion-body').on('show', function () {
      var headingSelector = '.accordion-toggle[href="#' + this.id + '"]';
      $(headingSelector).addClass('accordion-expanded');
      var $expandIcon = $(headingSelector).find('.accordion-expand-icon');
      $expandIcon.removeClass($expandIcon.data('icon-closed')).addClass($expandIcon.data('icon-opened')); // Remove focus from the heading to clear the text-decoration. (It's too
      // ham-fisted to do it in CSS.)

      $(headingSelector).blur();
    }).on('hide', function () {
      var headingSelector = '.accordion-toggle[href="#' + this.id + '"]';
      $(headingSelector).removeClass('accordion-expanded');
      var $expandIcon = $(headingSelector).find('.accordion-expand-icon');
      $expandIcon.removeClass($expandIcon.data('icon-opened')).addClass($expandIcon.data('icon-closed')); // Remove focus from the heading to clear the text-decoration. (It's too
      // ham-fisted to do it in CSS.)

      $(headingSelector).blur();
    });
    systrayMinimizeSetup();
    disableDisallowedTrafficSetup();
    splitTunnelSetup();
    disableTimeoutsSetup();
    egressRegionSetup();
    localProxySetup();
    upstreamProxySetup();
    vpnModeSetup();
    updateAvailableEgressRegions(false); // don't force valid -- haven't filled in settings yet

    $window.one(UI_READY_EVENT, function () {
      // Fill in the settings. This must be done after the UI is ready, since it might
      // show an error, requiring i18n to be initialized.
      refreshSettings();
    });
  }); //
  // Overall event handlers
  //
  // Settings tab has been navigated to and is shown

  function onSettingsTabShown() {
    // Reset the Apply button
    enableSettingsApplyButton(false);
  } // Settings tab is showing, but is being navigated away from.
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
      }); // Note: If we don't first remove existing click handlers (with .off), then
      // the handler that wasn't clicked last time will still be present.

      $modal.find('.apply-button').off('click').one('click', function () {
        $modal.modal('hide');

        if (applySettings()) {
          enableSettingsApplyButton(false);
          $(e.target).tab('show');
        } else {
          showSettingsErrorModal();
        }
      });
      $modal.find('.discard-button').off('click').one('click', function () {
        $modal.modal('hide');
        refreshSettings(g_initObj.Settings);
        enableSettingsApplyButton(false);
        $(e.target).tab('show');
      });
    }
  } // A setting value has been changed.


  function onSettingChanged(e, id) {
    DEBUG_LOG('onSettingChanged: ' + id);
    var settingsValues = getSettingsTabValues();

    if (settingsValues === false) {
      // Settings values are invalid
      enableSettingsApplyButton(false);
    } else {
      var settingsChanged = settingsObjectChanged(settingsValues);
      enableSettingsApplyButton(settingsChanged);
    }
  } // Handler for the Reset Settings button


  function onSettingsReset(e) {
    /*jshint validthis:true */
    e.preventDefault();
    $(this).blur();
    refreshSettings(g_initObj.Settings.defaults, false); // Force the Apply button to be enabled. Otherwise the user might be confused
    // about how to make the Reset take effect.

    enableSettingsApplyButton(true);
  } // Handler for the Apply Settings button


  function onSettingsApply(e) {
    /*jshint validthis:true */
    e.preventDefault();
    $(this).blur();

    if (!getSettingsApplyButtonEnabled()) {
      return;
    }

    if (applySettings()) {
      // Reset the Apply button
      enableSettingsApplyButton(false); // Switch to Connection tab

      switchToTab('#connection-tab');
    } else {
      showSettingsErrorModal();
    }
  } //
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
    } else if (!enable && currentlyEnabled) {
      $applyButton.addClass('disabled').attr('disabled', true);
    }
  }

  function getSettingsApplyButtonEnabled() {
    var $applyButton = $('#settings-pane .apply-settings');
    return !$applyButton.hasClass('disabled');
  } // Save the current settings (and possibly reconnect).


  function applySettings() {
    var settingsValues = getSettingsTabValues();

    if (settingsValues === false) {
      // Settings are invalid.
      return false;
    } // Update settings in the application (and trigger a reconnect, if
    // necessary).


    HtmlCtrlInterface_SaveSettings(JSON.stringify(settingsValues));
    return true;
  } // Refresh the current settings. If newSettings is truthy, it will become the
  // the new current settings, otherwise the existing current settings will be
  // refreshed in the UI.
  // newSettings can be a partial settings object (like, just {egressRegion: "US"} or whatever).
  // If forceCurrent is true, the new settings will be become canonical (rather than just displayed).


  function refreshSettings(newSettings, forceCurrent) {
    var fullNewSettings = $.extend(g_initObj.Settings, newSettings || {});

    if (forceCurrent) {
      g_initObj.Settings = fullNewSettings;
    }

    fillSettingsValues(fullNewSettings); // When the settings change, we need to check the current egress region choice.

    forceEgressRegionValid(); // NOTE: If more checks like this are added, we'll need to chain them (somehow),
    // otherwise we'll have a mess of modals.
  } // Fill in the settings controls with the values in `obj`.


  function fillSettingsValues(obj) {
    // Bit of a hack: Unhook the setting-changed event while we fill in the
    // values, then hook it up again after.
    $('#settings-pane').off(SETTING_CHANGED_EVENT, onSettingChanged);

    if (!_.isUndefined(obj.SplitTunnel)) {
      $('#SplitTunnel').prop('checked', !!obj.SplitTunnel);
    }

    if (!_.isUndefined(obj.DisableTimeouts)) {
      $('#DisableTimeouts').prop('checked', !!obj.DisableTimeouts);
    }

    if (!_.isUndefined(obj.VPN)) {
      $('#VPN').prop('checked', obj.VPN);
    }

    vpnModeUpdate();

    if (!_.isUndefined(obj.LocalHttpProxyPort)) {
      $('#LocalHttpProxyPort').val(obj.LocalHttpProxyPort > 0 ? obj.LocalHttpProxyPort : '');
    }

    if (!_.isUndefined(obj.LocalSocksProxyPort)) {
      $('#LocalSocksProxyPort').val(obj.LocalSocksProxyPort > 0 ? obj.LocalSocksProxyPort : '');
    }

    localProxyValid(false);

    if (!_.isUndefined(obj.ExposeLocalProxiesToLAN)) {
      $('#ExposeLocalProxiesToLAN').prop('checked', !!obj.ExposeLocalProxiesToLAN);
    }

    if (!_.isUndefined(obj.UpstreamProxyHostname)) {
      $('#UpstreamProxyHostname').val(obj.UpstreamProxyHostname);
    }

    if (!_.isUndefined(obj.UpstreamProxyPort)) {
      $('#UpstreamProxyPort').val(obj.UpstreamProxyPort > 0 ? obj.UpstreamProxyPort : '');
    }

    if (!_.isUndefined(obj.UpstreamProxyUsername)) {
      $('#UpstreamProxyUsername').val(obj.UpstreamProxyUsername);
    }

    if (!_.isUndefined(obj.UpstreamProxyPassword)) {
      $('#UpstreamProxyPassword').val(obj.UpstreamProxyPassword);
    }

    if (!_.isUndefined(obj.UpstreamProxyDomain)) {
      $('#UpstreamProxyDomain').val(obj.UpstreamProxyDomain);
    }

    if (!_.isUndefined(obj.SkipUpstreamProxy)) {
      $('#SkipUpstreamProxy').prop('checked', obj.SkipUpstreamProxy);
    }

    skipUpstreamProxyUpdate();
    upstreamProxyValid(false);

    if (!_.isUndefined(obj.EgressRegion)) {
      var region = obj.EgressRegion || BEST_REGION_VALUE;
      $('#EgressRegion [data-region]').removeClass('active');
      $('#EgressRegion').find('[data-region="' + region + '"] a').trigger('click', {
        ignoreDisabled: true
      });
    }

    if (!_.isUndefined(obj.SystrayMinimize)) {
      $('#SystrayMinimize').prop('checked', !!obj.SystrayMinimize);
    }

    if (!_.isUndefined(obj.DisableDisallowedTrafficAlert)) {
      $('#DisableDisallowedTrafficAlert').prop('checked', !!obj.DisableDisallowedTrafficAlert);
    } // Re-hook the setting-changed event


    $('#settings-pane').on(SETTING_CHANGED_EVENT, onSettingChanged);
  } // Extracts the values current set in the tab and returns and object with them.
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
      DisableTimeouts: $('#DisableTimeouts').prop('checked') ? 1 : 0,
      LocalHttpProxyPort: validatePort($('#LocalHttpProxyPort').val()),
      LocalSocksProxyPort: validatePort($('#LocalSocksProxyPort').val()),
      ExposeLocalProxiesToLAN: $('#ExposeLocalProxiesToLAN').prop('checked') ? 1 : 0,
      UpstreamProxyHostname: $('#UpstreamProxyHostname').val(),
      UpstreamProxyPort: validatePort($('#UpstreamProxyPort').val()),
      UpstreamProxyUsername: $('#UpstreamProxyUsername').val(),
      UpstreamProxyPassword: $('#UpstreamProxyPassword').val(),
      UpstreamProxyDomain: $('#UpstreamProxyDomain').val(),
      SkipUpstreamProxy: $('#SkipUpstreamProxy').prop('checked') ? 1 : 0,
      EgressRegion: egressRegion === BEST_REGION_VALUE ? '' : egressRegion,
      SystrayMinimize: $('#SystrayMinimize').prop('checked') ? 1 : 0,
      DisableDisallowedTrafficAlert: $('#DisableDisallowedTrafficAlert').prop('checked') ? 1 : 0
    };
    return returnValue;
  }

  function showSettingsErrorModal() {
    showNoticeModal('settings#error-modal#title', 'settings#error-modal#body', null, null, function () {
      showSettingErrorSection();
    });
  }

  function showSettingErrorSection() {
    showSettingsSection($('#settings-accordion .collapse .error').parents('.collapse').eq(0), $('#settings-pane .error input').eq(0).focus());
  } //
  // Systray Minimize
  //
  // Will be called exactly once. Set up event listeners, etc.


  function systrayMinimizeSetup() {
    $('#SystrayMinimize').change(function () {
      // Tell the settings pane a change was made.
      $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
    });
  } //
  // Disable disallowed traffic alert
  //
  // Will be called exactly once. Set up event listeners, etc.


  function disableDisallowedTrafficSetup() {
    $('#DisableDisallowedTrafficAlert').change(function () {
      // Tell the settings pane a change was made.
      $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
    });
  } //
  // Split Tunnel
  //
  // Will be called exactly once. Set up event listeners, etc.


  function splitTunnelSetup() {
    $('#SplitTunnel').change(function () {
      // Tell the settings pane a change was made.
      $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
    });
  } //
  // Disable Timeouts
  //
  // Will be called exactly once. Set up event listeners, etc.


  function disableTimeoutsSetup() {
    $('#DisableTimeouts').change(function () {
      // Tell the settings pane a change was made.
      $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
    });
  } //
  // Egress Region (Psiphon Server Region)
  //
  // Will be called exactly once. Set up event listeners, etc.


  function egressRegionSetup() {
    // Handle changes to the Egress Region
    $('#EgressRegion a').click(function (e, extraArgs) {
      e.preventDefault(); // Do nothing if we're disabled, unless we're forcing a disabled bypass

      if ($('#EgressRegion').hasClass('disabled') && (!extraArgs || !extraArgs.ignoreDisabled)) {
        $(this).blur();
        return;
      } // Check if this target item is already active. Return if so.


      if ($(this).parents('[data-region]').hasClass('active')) {
        return;
      }

      $('#EgressRegion [data-region]').removeClass('active');
      $(this).parents('[data-region]').addClass('active');
      egressRegionValid(false); // This event helps the combobox on the connect pane stay in sync.

      $('#EgressRegion').trigger('change'); // Tell the settings pane a change was made.

      $('#settings-pane').trigger(SETTING_CHANGED_EVENT, 'EgressRegion');
    });
  } // Returns true if the egress region value is valid, otherwise false.
  // Shows/hides an error message as appropriate.


  function egressRegionValid()
  /*finalCheck*/
  {
    var $currRegionElem = $('#EgressRegion li.active'); // Check to make sure the currently selected egress region is one of the
    // available regions.

    var valid = $currRegionElem.length > 0 && !$currRegionElem.hasClass('hidden');
    $('#EgressRegion').toggleClass('error', !valid);
    $('#settings-accordion-egress-region .egress-region-invalid').toggleClass('hidden', valid);
    updateErrorAlert();
    return valid;
  } // Check to make sure the currently selected egress region is one of the available
  // regions. If not, inform the user and get them to pick another.
  // A mismatch may occur either as a result of a change in the available egress
  // regions, or a change in the current selection (i.e., a settings change).


  function forceEgressRegionValid() {
    if (egressRegionValid()) {
      // Valid, nothing to do
      return;
    } // Put up the modal message


    showNoticeModal('settings#egress-region#error-modal-title', 'settings#egress-region#error-modal-body-http', null, null, null);
    showSettingsSection('#settings-accordion-egress-region');
  } // Update the egress region options we show in the UI.
  // If `forceValid` is true, then if the currently selected region is no longer
  // available, the user will be prompted to pick a new one.


  function updateAvailableEgressRegions(forceValid) {
    var regions = getCookie('AvailableEgressRegions'); // On first run there will be no such cookie.

    regions = regions || [];
    $('#EgressRegion li').each(function () {
      var elemRegion = $(this).data('region'); // If no region, this is a divider

      if (!elemRegion) {
        return;
      }

      if (_.includes(regions, elemRegion) || elemRegion === BEST_REGION_VALUE) {
        $(this).removeClass('hidden');
      } else {
        $(this).addClass('hidden');
      }
    });

    if (forceValid) {
      forceEgressRegionValid();
    }

    $('#EgressRegion').trigger('change');
  } //
  // Local Proxy Ports
  //
  // Will be called exactly once. Set up event listeners, etc.


  function localProxySetup() {
    // Handle change events
    $('#LocalHttpProxyPort, #LocalSocksProxyPort').on('keyup keydown keypress change blur', function (event) {
      // We need to delay this processing so that the change to the text has
      // had a chance to take effect. Otherwise this.val() will return the old
      // value.
      _.delay(_.bind(function () {
        // Tell the settings pane a change was made.
        $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id); // Check for validity.

        localProxyValid(false);
      }, this, event), 100);
    });
    $('#ExposeLocalProxiesToLAN').change(function () {
      // Tell the settings pane a change was made.
      $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
    });
  } // Returns true if the local proxy values are valid, otherwise false.
  // Shows/hides an error message as appropriate.


  function localProxyValid()
  /*finalCheck*/
  {
    // This check always shows an error while the user is typing, so finalCheck is ignored.
    var httpPort = validatePort($('#LocalHttpProxyPort').val());
    var socksPort = validatePort($('#LocalSocksProxyPort').val());
    var unique = httpPort !== socksPort || httpPort === 0 || socksPort === 0;

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
      $('#LocalHttpProxyPort, #LocalSocksProxyPort').parents('.control-group').addClass('error'); // Show error message

      $('.help-block.local-port-unique').removeClass('hidden');
    }

    updateErrorAlert();
    return httpPort !== false && socksPort !== false && unique;
  } // Show an error modal telling the user there is a local port conflict problem


  function localProxyPortConflictNotice(noticeType) {
    // Show the appropriate message depending on the error
    var bodyKey = noticeType === 'HttpProxyPortInUse' ? 'settings#local-proxy-ports#error-modal-body-http' : 'settings#local-proxy-ports#error-modal-body-socks';
    showNoticeModal('settings#local-proxy-ports#error-modal-title', bodyKey, null, null, null); // Switch to the appropriate settings section

    showSettingsSection('#settings-accordion-local-proxy-ports', noticeType === 'HttpProxyPortInUse' ? '#LocalHttpProxyPort' : '#LocalSocksProxyPort');
  } //
  // Upstream Proxy
  //
  // Will be called exactly once. Set up event listeners, etc.


  function upstreamProxySetup() {
    // Handle change events
    $('#UpstreamProxyHostname, #UpstreamProxyPort, #UpstreamProxyUsername, #UpstreamProxyPassword, #UpstreamProxyDomain').on('keyup keydown keypress change blur', function (event) {
      // We need to delay this processing so that the change to the text has
      // had a chance to take effect. Otherwise this.val() will return the old
      // value.
      _.delay(_.bind(function () {
        // Tell the settings pane a change was made.
        $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id); // Check for validity.

        upstreamProxyValid(false);
      }, this, event), 100);
    }); // Add the "skip" checkbox handler.

    $('#SkipUpstreamProxy').change(function () {
      // Trigger overall change event
      $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
      skipUpstreamProxyUpdate();
    });
  } // Returns true if the upstream proxy values are valid, otherwise false.
  // Shows/hides an error message as appropriate.


  function upstreamProxyValid()
  /*finalCheck*/
  {
    // If any field other than hostname is set, then hostname must be set,
    // AND if port is set, it must be an integer in the range 1-65535,
    // AND if either of domain or password is set, then username must be as well.
    // Unless 'skip' is checked.
    var skip = $('#SkipUpstreamProxy').prop('checked');
    var needHostname = Boolean($('#UpstreamProxyPort').val()) || Boolean($('#UpstreamProxyUsername').val()) || Boolean($('#UpstreamProxyPassword').val()) || Boolean($('#UpstreamProxyDomain').val());
    var needUsername = Boolean($('#UpstreamProxyPassword').val()) || Boolean($('#UpstreamProxyDomain').val());
    var valid = true;
    var portOK = validatePort($('#UpstreamProxyPort').val()) !== false;

    if (skip || portOK) {
      // Hide the port-specific message
      $('.help-inline.UpstreamProxyPort').addClass('hidden').parents('.control-group').removeClass('error');
    } else {
      valid = false; // Port value bad. Show error while typing

      $('.help-inline.UpstreamProxyPort').removeClass('hidden').parents('.control-group').addClass('error');
    }

    if (skip || !needHostname || Boolean($('#UpstreamProxyHostname').val())) {
      // Hide the "hostname required" message
      $('.upstream-proxy-set-hostname-error-msg').addClass('hidden'); // And remove error state from the control

      $('#UpstreamProxyHostname').parents('.control-group').removeClass('error');
    } else {
      valid = false; // Show the "hostname required" message

      $('.upstream-proxy-set-hostname-error-msg').removeClass('hidden'); // And add error state to the control

      $('#UpstreamProxyHostname').parents('.control-group').addClass('error');
    }

    if (skip || !needUsername || Boolean($('#UpstreamProxyUsername').val())) {
      // Hide the "username required" message
      $('.upstream-proxy-set-username-error-msg').addClass('hidden'); // And remove error state from the control

      $('#UpstreamProxyUsername').parents('.control-group').removeClass('error');
    } else {
      valid = false; // Show the "username required" message

      $('.upstream-proxy-set-username-error-msg').removeClass('hidden'); // And add error state to the control

      $('#UpstreamProxyUsername').parents('.control-group').addClass('error');
    }

    updateErrorAlert();
    return valid;
  } // The other upstream proxy settings should be disabled if skip-upstream-proxy
  // is set.


  function skipUpstreamProxyUpdate() {
    var skipUpstreamProxy = $('#SkipUpstreamProxy').prop('checked');
    $('.skip-upstream-proxy-incompatible input').prop('disabled', skipUpstreamProxy);
    $('.skip-upstream-proxy-incompatible').toggleClass('disabled-text', skipUpstreamProxy);
  } // The occurrence of an upstream proxy error might mean that a tunnel cannot
  // ever be established, but not necessarily -- it might just be, for example,
  // that the upstream proxy doesn't allow the port needed for one of our
  // servers, but not all of them.
  // So instead of showing an error immediately, we'll remember that the upstream
  // proxy error occurred, wait a while to see if we connect successfully, and
  // show it if we haven't connected.
  // Potential enhancement: If the error modal is showing and the connection
  // succeeds, dismiss the modal. This would be good behaviour, but probably too
  // fringe to be worthwhile.


  var g_upstreamProxyErrorNoticeTimer = null; // When the connected state changes, we clear the timer.

  $window.on(CONNECTED_STATE_CHANGE_EVENT, function () {
    if (g_upstreamProxyErrorNoticeTimer) {
      clearTimeout(g_upstreamProxyErrorNoticeTimer);
      g_upstreamProxyErrorNoticeTimer = null;
    }
  });

  function upstreamProxyErrorNotice(errorMessage) {
    if (g_upstreamProxyErrorNoticeTimer) {
      // We've already received an upstream proxy error and we're waiting to show it.
      return;
    } // This is the first upstream proxy error we've received, so start waiting to
    // show a message for it.


    g_upstreamProxyErrorNoticeTimer = setTimeout(function () {
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
      } // There are two slightly different error messages shown depending on whether
      // there is an upstream proxy explicitly configured, or it's the default
      // empty value, which means the pre-existing system proxy will be used.


      var bodyKey = 'settings#upstream-proxy#error-modal-body-default';

      if (g_initObj.Settings.UpstreamProxyHostname) {
        bodyKey = 'settings#upstream-proxy#error-modal-body-configured';
      }

      showNoticeModal('settings#upstream-proxy#error-modal-title', bodyKey, 'general#notice-modal-tech-preamble', errorMessage, function () {
        $('#UpstreamProxyHostname').focus();
      }); // Switch to the appropriate settings section

      showSettingsSection('#settings-accordion-upstream-proxy'); // We are not going to set the timer to null here. We only want the error
      // to show once per connection attempt sequence. It will be cleared when
      // the client transistions to 'stopped' or 'connected'.
    }, 60000);
  } //
  // VPN Mode (Transport Mode)
  //
  // Will be called exactly once. Set up event listeners, etc.


  function vpnModeSetup() {
    // Some fields are disabled in VPN mode
    $('#VPN').change(function () {
      // Tell the settings pane a change was made.
      $('#settings-pane').trigger(SETTING_CHANGED_EVENT, this.id);
      vpnModeUpdate();
    });
  } // Some of the settings are incompatible with VPN mode. We'll modify the
  // display depending on the choice of VPN mode.


  function vpnModeUpdate() {
    var vpn = $('#VPN').prop('checked');
    $('input.vpn-incompatible, .vpn-incompatible input, ' + 'select.vpn-incompatible, .vpn-incompatible select, .vpn-incompatible .dropdown-menu').prop('disabled', vpn).toggleClass('disabled', vpn);
    $('.vpn-incompatible-msg').toggleClass('hidden', !vpn);
    $('.vpn-incompatible').toggleClass('disabled-text', vpn);
    $('.vpn-incompatible-hide').toggleClass('hidden', vpn);
  } //
  // Helpers
  //
  // Show/hide the error alert depending on whether we have an erroneous field


  function updateErrorAlert() {
    $('#settings-pane .value-error-alert').toggleClass('invisible z-behind', $('#settings-pane .control-group.error').length === 0);
  } // Returns the numeric port if valid, otherwise false. Note that 0 is a valid
  // return value, and falsy, so use `=== false`.


  function validatePort(val) {
    if (val.length === 0) {
      return 0;
    }

    var intVal = parseInt(val);

    if (isNaN(intVal) || intVal < 1 || intVal > 65535) {
      return false;
    }

    if (intVal.toString() !== val) {
      // There were extra characters at the end of the string that didn't get converted
      // into the int.
      return false;
    }

    return intVal;
  } // Show the Settings tab and expand the target section.
  // If focusElem is optional; if set, focus will be put in that element.


  function showSettingsSection(section, focusElem) {
    // We can only expand the section after the tab is shown
    function onTabShown() {
      // Hack: The collapse-show doesn't seem to work unless we wait a bit
      setTimeout(function () {
        // Expand the section
        $(section).collapse('show'); // Collapse any other sections

        $('#settings-accordion .in.collapse').not(section).collapse('hide'); // Scroll to the section, after allowing the section to expand

        setTimeout(function () {
          $('#settings-pane').scrollTo($(section).parents('.accordion-group').eq(0), {
            duration: 500,
            // animation time
            offset: -50,
            // leave some space for the alert
            onAfter: function onAfter() {
              if (focusElem) {
                $(focusElem).eq(0).focus();
              }
            }
          });
        }, 200);
      }, 500);
    } // Make sure the settings tab is showing.


    switchToTab('#settings-tab', onTabShown);
  }
  /* FEEDBACK ******************************************************************/


  $(function () {
    // Add click listener to the happy/sad choices
    $('.feedback-smiley .feedback-choice').click(function (e) {
      e.preventDefault();
      $('.feedback-smiley .feedback-choice').removeClass('selected');
      $(this).addClass('selected');
    });
    $('#feedback-submit').click(function (e) {
      e.preventDefault();
      sendFeedback();
    }); // The sponsor-specific links in the text will be set correctly elsewhere.
  });

  function sendFeedback() {
    var smileyResponses = [];
    $('.feedback-choice.selected').each(function () {
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
    }; // Switch to the connection tab

    switchToTab('#connection-tab'); // Clear the feedback form

    $('.feedback-choice.selected').removeClass('selected');
    $('#feedback-comments').val('');
    $('#feedback-email').val('');
    $('#feedback-send-diagnostic').prop('checked', true); // Actually send the feedback

    HtmlCtrlInterface_SendFeedback(JSON.stringify(fields)); // Show (and hide) the success alert

    displayCornerAlert($('#feedback-success-alert'));
  }
  /* LOGS **********************************************************************/


  $(function () {
    $('#show-debug-logs').click(showDebugLogsClicked); // Set the initial show-debug state

    var show = $('#show-debug-logs').prop('checked');
    $('.log-messages').toggleClass('showing-priority-0', show).toggleClass('hiding-priority-0', !show);
  });

  function showDebugLogsClicked() {
    /*jshint validthis:true */
    var show = $(this).prop('checked'); // We use both a showing and a hiding class to try to deal with IE7's CSS insanity.

    $('.log-messages').toggleClass('showing-priority-0', show).toggleClass('hiding-priority-0', !show);
  } // Expects obj to be of the form {priority: 0|1|2, message: string}


  function addLog(obj) {
    $('.log-messages .placeholder').remove();
    $('.log-messages').loadTemplate($("#log-template"), {
      timestamp: new Date().toLocaleTimeString(),
      message: obj.message,
      priority: 'priority-' + obj.priority
    }, {
      prepend: true
    }); // The "Show Debug Logs" checkbox is hidden until we actually get a debug
    // message.

    if (obj.priority < 1) {
      $('#logs-pane .invisible').removeClass('invisible');
    } // Record the priorities we've seen.
    // We'll keep them in a (fake) Set.


    addLog.priorities = addLog.priorities || {};
    addLog.priorities[obj.priority] = true; // Don't allow the log messages list to grow forever!
    // We'll set a limit for each priority. (Arbitrarily. May need to revisit.)

    var MAX_LOGS_PER_PRIORITY = 200;

    var priorities = _.keys(addLog.priorities);

    for (var i = 0; i < priorities.length; i++) {
      $('.log-messages .priority-' + priorities[i]).slice(MAX_LOGS_PER_PRIORITY).remove();
    }
  } // Used for temporary debugging messages.


  function DEBUG_LOG() {
    debugLogHelper(window.console.log, Array.prototype.slice.call(arguments));
  }

  function DEBUG_WARN() {
    debugLogHelper(window.console.warn, Array.prototype.slice.call(arguments));
  }

  function debugLogHelper(consoleLogger, args) {
    if (!g_initObj.Config.Debug) {
      return;
    }

    var msg = 'DEBUG:';

    for (var i = 0; i < args.length; i++) {
      msg += ' ';

      if (_.isString(args[i])) {
        msg += args[i];
      } else {
        msg += JSON.stringify(args[i]);
      }
    }

    addLog({
      priority: 0,
      message: msg
    });
    var logger = consoleLogger || window.console.log;

    try {
      if (IS_BROWSER) {
        logger.apply(console, [msg]);
      }
    } catch (e) {
      // IE8 (at least IE8 mode in IE11) has weird console.log (etc.) implementations that
      // are objects? But can still be called? But even trying to access .apply throws an
      // exception? But calling it directly is okay?
      logger(msg);
    }
  }
  /* LANGUAGE ******************************************************************/


  var RTL_LOCALES = ['devrtl', 'fa', 'fa_AF', 'ar', 'ur'];
  $(function () {
    var fallbackLanguage = 'en'; // Language priority: cookie, system locale, fallback

    var lang = getCookie('language') || g_initObj.Config && g_initObj.Config.Language || fallbackLanguage;
    i18n.init({
      lang: lang,
      fallbackLng: fallbackLanguage,
      resStore: window.PSIPHON.LOCALES
    }, function () {
      switchLocale(lang, true);
    }); // Populate the list of language choices

    populateLocales();
  }); // We only want to show the success/welcome message once.

  var g_languageSuccessAlertShown = false; // Other functions may need to know if we're currently using a RTL locale or not

  var g_isRTL = false;

  function switchLocale(locale, initial) {
    i18n.setLng(locale, function () {
      // This callback does not seem to called asynchronously (probably because
      // we're loading from an object and not a remote resource). But we want this
      // code to run after everything else is done, so we'll force it to be async.
      nextTick(function () {
        // HTML+JS use locale codes of the form 'en-US', rather than
        // Psiphon's 'en_US'.
        $('html').attr('lang', locale.replace('_', '-'));
        $('body').i18n(); // The content of elements will have changed, so trigger custom event that can
        // be listened for to take additional actions.

        $window.trigger(LANGUAGE_CHANGE_EVENT);

        if (!initial && !g_languageSuccessAlertShown) {
          // Show (and hide) the success alert
          g_languageSuccessAlertShown = true;
          displayCornerAlert($('#language-success-alert'));
        } // Remember the user's choice


        setCookie('language', locale);
      });
    }); //
    // Right-to-left languages need special consideration.
    //

    var rtl = _.contains(RTL_LOCALES, locale);

    g_isRTL = rtl;
    $('body').attr('dir', rtl ? 'rtl' : 'ltr').css('direction', rtl ? 'rtl' : 'ltr'); // We'll use a data attribute to store classes which should only be used
    // for RTL and not LTR, and vice-versa.

    $('[data-i18n-rtl-classes]').each(function () {
      var ltrClasses = $(this).data('i18n-ltr-classes');
      var rtlClasses = $(this).data('i18n-rtl-classes');

      if (ltrClasses) {
        $(this).toggleClass(ltrClasses, !rtl);
      }

      if (rtlClasses) {
        $(this).toggleClass(rtlClasses, rtl);
      }
    }); //
    // Update C code string table with new language values
    //
    // Iterate through the English keys, since we know it will be complete.

    var translation = window.PSIPHON.LOCALES.en.translation;
    var appBackendStringTable = {};

    for (var key in translation) {
      if (!translation.hasOwnProperty(key)) {
        continue;
      }

      if (_.startsWith(key, 'appbackend#')) {
        appBackendStringTable[key] = i18n.t(key);
      }
    }

    HtmlCtrlInterface_AddStringTableItem(locale, appBackendStringTable);
  }

  function populateLocales() {
    var localePriorityGuide = ['en', 'fa', 'ar', 'zh', 'zh_CN', 'zh_TW'];
    var locales = $.map(window.PSIPHON.LOCALES, function (val, key) {
      return key;
    }); // Sort the locales according to the priority guide

    locales.sort(function (a, b) {
      var localePriority_a = localePriorityGuide.indexOf(a);
      var localePriority_b = localePriorityGuide.indexOf(b);
      localePriority_a = localePriority_a < 0 ? 999 : localePriority_a;
      localePriority_b = localePriority_b < 0 ? 999 : localePriority_b;

      if (localePriority_a < localePriority_b) {
        return -1;
      } else if (localePriority_a > localePriority_b) {
        return 1;
      } else if (a < b) {
        return -1;
      } else if (a > b) {
        return 1;
      }

      return 0;
    });
    var $localeListElem = $('#language-pane');

    for (var i = 0; i < locales.length; i++) {
      // If we're not in debug mode, don't output the dev locales
      if (!g_initObj.Config.Debug && _.startsWith(locales[i], 'dev')) {
        continue;
      }

      $localeListElem.loadTemplate($("#locale-template"), {
        localeCode: locales[i],
        localeName: window.PSIPHON.LOCALES[locales[i]].name
      }, {
        append: true
      });
    } // Set up the click handlers


    $('.language-choice').click(function (e) {
      e.preventDefault();
      switchLocale($(this).attr('value'));
    });
  }
  /* ABOUT *********************************************************************/
  // No special code. Sponsor-specific links are set elsewhere.

  /* PSICASH *******************************************************************/

  /**
   * PsiCash-related events.
   * @enum {string}
   * @readonly
   */


  var PsiCashEventTypeEnum = {
    /** Argument is PsiCashRefreshData */
    REFRESH: 'psicash::refresh',

    /** Argument is PsiCashPurchaseResponse */
    NEW_PURCHASE: 'psicash::new-purchase',

    /** Argument is PsiCashInitDoneData */
    INIT_DONE: 'psicash::init-done'
  };
  /**
   * Server response statuses
   * @enum {number}
   * @readonly
   */

  var PsiCashServerResponseStatus = {
    Invalid: -1,
    // Should never be used if well-behaved
    Success: 0,
    ExistingTransaction: 1,
    InsufficientBalance: 2,
    TransactionAmountMismatch: 3,
    TransactionTypeNotFound: 4,
    InvalidTokens: 5,
    ServerError: 6
  };
  /**
   * @typedef {Object} PsiCashPurchasePrice
   * @property {!string} class
   * @property {!string} distinguisher
   * @property {!number} price
   */

  /**
   * @typedef {Object} PsiCashPurchase
   * @property {!string} id
   * @property {!string} class
   * @property {!string} distinguisher
   * @property {?moment} localTimeExpiry;
   * @property {?moment} serverTimeExpiry;
   */

  /**
   * The expected payload passed to HtmlCtrlInterface_PsiCashMessage when a purchase is complete.
   * @typedef {Object} PsiCashPurchaseResponse
   * @property {?string} error
   * @property {!PsiCashServerResponseStatus} status
   * @property {?PsiCashRefreshData} refresh
   */

  /**
   * The expected payload passed to HtmlCtrlInterface_PsiCashMessage when a refresh should be done.
   * @typedef {Object} PsiCashRefreshData
   * @property {string[]} valid_token_types
   * @property {number} balance
   * @property {PsiCashPurchasePrice[]} purchase_prices
   * @property {PsiCashPurchase[]} purchases
   * @property {string} buy_psi_url
   */

  /**
   * The expected payload passed to HtmlCtrlInterface_PsiCashMessage when PsiCash.Init() is done.
   * @typedef {Object} PsiCashInitDoneData
   * @property {?string} error
   * @property {?boolean} recovered
   */

  /**
  * Used as the "command" type passed to HtmlCtrlInterface_PsiCashCommand.
  * @enum {string}
  * @readonly
  */

  var PsiCashCommandEnum = {
    REFRESH: 'refresh',
    PURCHASE: 'purchase'
    /**
     * PsiCash command base class constrcutor
     * @class PsiCashCommandBase
     * @classdesc Should only be used by subclasses
     * @param {PsiCashCommandEnum} command
     */

  };

  function PsiCashCommandBase(command) {
    /**
     * @name PsiCashCommandBase#command
     * @type {PsiCashCommandEnum}
     */
    this.command = command;
    /**
     * @name PsiCashCommandBase#id
     * @type {string}
     */

    this.id = randomID();
  }
  /**
   * Construct a new refresh command
   * @class PsiCashCommandRefresh
   * @classdesc Passed to HtmlCtrlInterface_PsiCashCommand to indicate a data refresh is desired
   * @param {string} reason Indicates the reason for the refresh (used for logging and debugging)
   */


  function PsiCashCommandRefresh(reason) {
    PsiCashCommandBase.call(this, PsiCashCommandEnum.REFRESH);
    this.reason = reason;
  }

  PsiCashCommandRefresh.prototype = _.create(PsiCashCommandBase.prototype, {
    constructor: PsiCashCommandRefresh
  });
  /**
   * Construct a new purchase command
   * @class PsiCashCommandPurchase
   * @classdesc Passed to HtmlCtrlInterface_PsiCashCommand to indicate a purchase is desired
   * @param {string} transactionClass
   * @param {string} distinguisher
   * @param {number} expectedPrice
   */

  function PsiCashCommandPurchase(transactionClass, distinguisher, expectedPrice) {
    PsiCashCommandBase.call(this, PsiCashCommandEnum.PURCHASE);
    this.transactionClass = transactionClass;
    this.distinguisher = distinguisher;
    this.expectedPrice = expectedPrice;
  }

  PsiCashCommandPurchase.prototype = _.create(PsiCashCommandBase.prototype, {
    constructor: PsiCashCommandPurchase
  });
  /**
   * PsiCash-related messages from the C code.
   * @enum {string}
   * @readonly
   */

  var PsiCashMessageTypeEnum = {
    REFRESH: 'refresh',
    NEW_PURCHASE: 'new-purchase',
    INIT_DONE: 'init-done'
  };
  /**
   * @typedef {Object} PsiCashMessageData
   * @property {PsiCashMessageTypeEnum} type
   * @property {string} id
   * @property {PsiCashRefreshData} payload
   */
  // TODO: Maybe roll this into a more global pubsub? But why?

  /** @type {Datastore} */

  var PsiCashStore = new Datastore({
    initDone: false,
    purchaseInProgress: false
  }, 'PsiCashStore');
  $(function psicashInit() {
    // NOTE: "refresh" will not make a server request unless we're connected. If not
    // connected, it just gets locally cached values.
    // And update the UI values every time the app gets focus.
    addWindowFocusHandler(function () {
      if (!PsiCashStore.data.initDone) {
        return;
      }

      HtmlCtrlInterface_PsiCashCommand(new PsiCashCommandRefresh('app-focus'));
    }); // And update the UI values every time we get connected.

    $window.on(CONNECTED_STATE_CHANGE_EVENT, function () {
      if (!PsiCashStore.data.initDone) {
        return;
      }

      HtmlCtrlInterface_PsiCashCommand(new PsiCashCommandRefresh('connected-state-change'));
    }); // And update the UI values every time the display language changes, so that the numbers
    // are correctly formatted.

    $window.on(LANGUAGE_CHANGE_EVENT, function () {
      if (!PsiCashStore.data.initDone) {
        return;
      }

      HtmlCtrlInterface_PsiCashCommand(new PsiCashCommandRefresh('ui-language-change'));
    }); // And update the UI when the settings change. If we go into/out of VPN mode we need to
    // disable the UI and indicate why.

    $('#settings-pane').on(SETTING_CHANGED_EVENT, function () {
      if (!PsiCashStore.data.initDone) {
        return;
      }

      HtmlCtrlInterface_PsiCashCommand(new PsiCashCommandRefresh('settings-changed'));
    });
  });
  /**
   * Handles the message indicating that the PsiCash library failed to initialize.
   * @param {*} evt Event. Unused.
   * @param {PsiCashInitDoneData} data
   */

  function psiCashLibraryInitDone(evt, data) {
    if (data.error) {
      // The library failed to initialize. This is very bad. The user lost their Tracker credit
      // or their Account logged-in state.
      if (data.recovered) {
        showNoticeModal('psicash#init-error-title', 'psicash#init-error-body-recovered', 'general#notice-modal-tech-preamble', data.error, null); // callback
      } else {
        showNoticeModal('psicash#init-error-title', 'psicash#init-error-body-unrecovered', 'general#notice-modal-tech-preamble', data.error, null); // callback
      }
    }

    PsiCashStore.set('initDone', true);
    HtmlCtrlInterface_PsiCashCommand(new PsiCashCommandRefresh('init-done'));
  }

  $window.on(PsiCashEventTypeEnum.INIT_DONE, psiCashLibraryInitDone);
  /**
   * Helper function to determine if PsiCash is ready for use.
   * @param {?PsiCashRefreshData} psicashData The data to use for the check. If not provided,
   *    g_PsiCashData will be used.
   */

  function psiCashStateInitialized(psicashData) {
    if (psicashData) {
      g_PsiCashData = psicashData;
    }

    return psicashData.valid_token_types && psicashData.valid_token_types.length > 0 && _.isNumber(psicashData.balance) && !_.isNaN(psicashData.balance);
  }

  var PSICASH_ENABLED_COOKIE = 'psicash::Enabled';
  /**
   * Called when PsiCash-related data should be refreshed. This may or may not mean that
   * a request was made to the server to get fresh data.
   * @param {PsiCashRefreshData} data
   */

  function refreshPsiCashEventHandler(evt, data) {
    psiCashUIUpdater(data);
  }

  $window.on(PsiCashEventTypeEnum.REFRESH, refreshPsiCashEventHandler);
  /** @type {?PsiCashRefreshData} */

  var g_PsiCashData = null;
  /**
   * The state determines which chunks of UI are visible.
   * @enum {object}
   * @readonly
   */

  var PsiCashUIState = {
    ZERO_BALANCE: {
      uiSelector: '#psicash-interface-zerobalance'
    },
    NSF_BALANCE: {
      uiSelector: '#psicash-interface-nsfbalance'
    },
    ENOUGH_BALANCE: {
      uiSelector: '#psicash-interface-enoughbalance'
    },
    BUYING_BOOST: {
      uiSelector: '#psicash-interface-buyingboost'
    },
    ACTIVE_BOOST: {
      uiSelector: '#psicash-interface-activeboost'
    },
    VPN_MODE_DISABLED: {
      uiSelector: '#psicash-interface-vpndisabled'
    }
  };
  PsiCashStore.set('uiState', PsiCashUIState.ZERO_BALANCE);
  /**
   * Called from refreshPsiCash and on an interval to update the PsiCash UI.
   * @param {?PsiCashRefreshData} psicashData Will be undefined when called on a timer.
   */

  function psiCashUIUpdater(psicashData) {
    // This must be set by any code below that queues up another call to this function via
    // setTimeout. This is to prevent building up a flood of redundant calls.
    if (!psiCashUIUpdater.timeout) {
      psiCashUIUpdater.timeout = null;
    }

    if (psicashData) {
      if (g_PsiCashData) {
        // For later diagnostics, log if psicashData values changed
        if (!_.isNaN(psicashData.balance) && psicashData.balance !== g_PsiCashData.balance) {
          HtmlCtrlInterface_Log('PsiCash: balance change:', psicashData.balance - g_PsiCashData.balance);
        } // TODO: Log other changes? Kind of a hassle.

      }

      g_PsiCashData = psicashData;
    } else if (!g_PsiCashData) {
      // No data to use, nothing to do.
      return;
    }

    psicashData = g_PsiCashData;
    /**
     * Will be true for the very first update when the UI is enabled. Affects which animations are shown.
     */

    var veryFirstUpdate = false; // The enabled cookie will not be present on the very first run, and is set to true
    // when we first enter a state where the PsiCash UI can be shown (and the onboarding
    // presented).

    if (getCookie(PSICASH_ENABLED_COOKIE)) {
      // Even if the cookie is present, the user might have deleted their psicashdatastore
      // file, so double-check the sanity of our state.
      if (!psiCashStateInitialized(psicashData)) {
        setCookie(PSICASH_ENABLED_COOKIE, false); // Re-call this function to start all over.

        return psiCashUIUpdater(psicashData);
      }
    } else {
      // Check if we're in a state where the functionality can be enabled.
      if (windowHasFocus() && psiCashStateInitialized(psicashData) && g_lastState === 'connected') {
        // We're in a good state, and this is the very first time.
        HtmlCtrlInterface_Log('PsiCash: very first update');
        veryFirstUpdate = true;
        setCookie(PSICASH_ENABLED_COOKIE, true); // TODO: onboarding
      } else {
        // PsiCash is still disabled, so there's nothing to do.
        return;
      }
    }

    if ($('#psicash-block').hasClass('hidden')) {
      $('#psicash-block').removeClass('hidden'); // Some layout actions like height-matching won't have succeeded while the
      // UI was hidden. So do a content-resize with the newly visible content.

      nextTick(resizeContent);
    }

    if (psicashData.buy_psi_url) {
      var urlComp = urlComponents(psicashData.buy_psi_url);
      urlComp.search += (urlComp.search ? '&' : '?') + 'utm_source=windows_app';
      $('a.psicash-buy-psi').prop('href', urlComp.href).removeClass('hidden');
    } else {
      // For some states (like zero balance), hiding the "buy" button will look strange, but
      // since that implies there's no earner token, the whole PsiCash UI will be hidden anyway.
      $('a.psicash-buy-psi').addClass('hidden');
    }

    var sbPrices = {};

    if (psicashData.purchase_prices) {
      // NOTE: This does not handle disappearing prices.
      for (var i = 0; i < psicashData.purchase_prices.length; i++) {
        var pp = psicashData.purchase_prices[i];

        if (pp['class'] === 'speed-boost') {
          sbPrices[pp.distinguisher] = pp.price;
          $(".psicash-sb-price[data-distinguisher=\"".concat(pp.distinguisher, "\"]")).text(formatPsi(parseInt(pp.price)));
          $(".psicash-sb-price[data-distinguisher=\"".concat(pp.distinguisher, "\"]")).data('expectedPrice', pp.price);
        }
      }
    }

    var state = PsiCashUIState.ZERO_BALANCE; // DO NOT return early from this point. state must be updated in PsiCashStore.uiState.
    // Only the 1-hour Speed Boost is considered for determining if the user has "enough" Psi

    if (_.isNumber(psicashData.balance) && _.isNumber(sbPrices['1hr'])) {
      if (psicashData.balance >= sbPrices['1hr']) {
        state = PsiCashUIState.ENOUGH_BALANCE; // Enable/disable the 1-day button depending on balance.
        // (Note that this is only a cosmetic disabling, and the button will still respond
        // to clicks. It will show an appropriate NSF message.)

        var nsf1Day = psicashData.balance < sbPrices['24hr'];
        $('.psicash-buy[data-distinguisher="24hr"]').prop('disabled', nsf1Day).toggleClass('disabled', nsf1Day);
      } else if (psicashData.balance > 0) {
        state = PsiCashUIState.NSF_BALANCE;
      }
    }

    var millisOfSpeedBoostRemaining = 0;

    if (psicashData.purchases) {
      for (var _i = 0; _i < psicashData.purchases.length; _i++) {
        // There are two different contexts/ways of checking for active Speed Boost.
        // **If we are connected**, then we rely on psiphond to decide that the
        // authorization is expired, which will result in tunnel-core reconnecting and a
        // signal that the authorization is now rejected. This is when the purchase is
        // actually removed from the PsiCash lib datastore. So, if we are connected, its
        // presence in the purchases array should be interpreted as meaning that the
        // purchase/authorization is valid.
        // **If we are not connected**, then we have to rely on the purchase expiry.
        // (We're certainly not going to show a purchase as active for hours too long
        // until the user happens to connect and refresh PsiCash state.)
        // Note: We're making no special effort to check for multiple active Speed Boosts.
        // This should not happen, per server rules.
        if (psicashData.purchases[_i]['class'] === 'speed-boost') {
          var localTimeExpiry = moment(psicashData.purchases[_i].localTimeExpiry);

          if (g_lastState === 'connected' || localTimeExpiry.isAfter(moment())) {
            state = PsiCashUIState.ACTIVE_BOOST;
            millisOfSpeedBoostRemaining = localTimeExpiry.diff(moment()); // Clock skew (between client<->PsiCash server<->psiphond) could result in a
            // purchase being used past the expiry in the purchase record. Ensure we're
            // not showing a negative value in the UI.

            millisOfSpeedBoostRemaining = Math.max(0, millisOfSpeedBoostRemaining);
            var boostRemainingTime = moment.duration(millisOfSpeedBoostRemaining).locale(momentLocale()).humanize().replace(' ', '&nbsp;'); // avoid splitting the time portion

            var boostRemainingText = i18n.t('psicash#ui-speedboost-active').replace('%s', boostRemainingTime);
            $('.speed-boost-time-remaining').html(boostRemainingText);
            break;
          }
        }
      }
    }

    if (PsiCashStore.data.purchaseInProgress) {
      // We are waiting for a purchase request to complete
      state = PsiCashUIState.BUYING_BOOST;
    } // Speed Boost cannot function in L2TP/IPSec mode. We want to disabled controls and
    // indicate why we're in that state.


    if (g_initObj.Settings.VPN) {
      state = PsiCashUIState.VPN_MODE_DISABLED;
    } // Show and hide the appropriate parts of the UI


    $('.psicash-interface').not(state.uiSelector).addClass('hidden');
    $(state.uiSelector).removeClass('hidden'); // Now that the correct interface is showing, update the balance

    PsiCashBalanceChange.push(psicashData.balance, veryFirstUpdate); // If Boost _is not_ active, then whether or not we show the speed limit UI depends
    // on the baseline connection speed. If Boost _is_ active, then we always show the speed.

    if (state === PsiCashUIState.ACTIVE_BOOST) {
      DEBUG_LOG('Speed Boost active; showing speed limit');
      $('.psicash-interface .speed-limit').removeClass('hidden');
    } else {
      var baselineRateLimit = getCookie('BaselineRateLimit');
      var threshold = (5 << 20) / 8; // 5 mbps in bytes; arbitrarily "fast"

      if (!baselineRateLimit || baselineRateLimit > threshold) {
        // Either we don't yet have a baseline, or it's above the threshold
        DEBUG_LOG('Baseline speed is high; hiding speed limit');
        $('.psicash-interface .speed-limit').addClass('hidden');
      } else {
        // The baseline is below the threshold
        DEBUG_LOG('Baseline speed is low; showing speed limit');
        $('.psicash-interface .speed-limit').removeClass('hidden');
      }
    } // When we have an active speed boost, we want this function to be called repeatedly,
    // so that the countdown timer is updated, and so the UI changes when the speed boost
    // ends. But there's no reason to do work on an interval if there's no active boost.


    if (state === PsiCashUIState.ACTIVE_BOOST) {
      // There are triggers that result in this function being called, and we don't want
      // to create periodic update timeouts every time, or else we'll end up with updates
      // happening way too often (multiple times per second).
      clearTimeout(psiCashUIUpdater.timeout); // Wait longer between updates if there's a lot of time left in the boost.

      if (millisOfSpeedBoostRemaining < 120 * 1000) {
        psiCashUIUpdater.timeout = setTimeout(psiCashUIUpdater, 1000);
      } else {
        psiCashUIUpdater.timeout = setTimeout(psiCashUIUpdater, 60 * 1000);
      }
    }

    PsiCashStore.set('uiState', state);
  }
  /**
   * Update the UI to a new balance, complete with animations.
   * @param {number} newBalance The balance to update the UI to match.
   * @param {boolean} veryFirstUpdate Whether this is the very first balance update
   *    (determines which animations are used).
   * @returns {jQuery.Promise} A promise that will be resolved when the update animations
   *    are complete.
   */


  function doBalanceChange(newBalance, veryFirstUpdate) {
    var allDoneDefer = $.Deferred();
    var previousBalance = parseInt($('.psicash-balance').data('psicash-balance')); // may be NaN

    if (_.isNaN(previousBalance)) {
      // If this is the very first refresh after the UI is enabled, we want to animate the
      // balance change. But if this is just an app start-up that's restoring a previous
      // balance, then we don't.
      var startingPoint = veryFirstUpdate ? 0 : newBalance;
      $('.psicash-balance').text(formatPsi(startingPoint)).data('psicash-balance', startingPoint);
      previousBalance = startingPoint;
    } else {
      // Update the value of the balance field. This is mostly so that a post-language-change
      // refresh will show the balance in the correct format.
      $('.psicash-balance').text(formatPsi(previousBalance));
    }

    var balanceDiff = newBalance - previousBalance; // If the diff is 0, we're not going to update anything. Otherwise we're going to
    // animate the change.

    if (balanceDiff === 0) {
      allDoneDefer.resolve();
    } else {
      balanceDiff = _.isNaN(balanceDiff) ? newBalance : balanceDiff; // Set the data value to the real balance immediately, to prevent later confusion.

      $('.psicash-balance').data('psicash-balance', newBalance); // We're going to animate the balance increasing. We need to figure out the steps.

      var BILLION = 1e9;
      var balanceStepTime = 2000; // ms

      var balanceStopInterval = 25; // ms

      var maxSteps = Math.trunc(balanceStepTime / balanceStopInterval); // We only want to consider billions

      var gigaBalanceDiff = balanceDiff / BILLION;
      var balanceStepCount = Math.max(1, Math.min(Math.abs(gigaBalanceDiff), maxSteps));
      var balanceStepSize = Math.trunc(gigaBalanceDiff / balanceStepCount) * BILLION; // If we only stepped balanceStepSize, the last step might be huge. So we'll
      // take some larger steps at the start.

      var extraStepSize = balanceDiff - Math.sign(balanceDiff) * balanceStepCount * balanceStepSize;
      var intermediateBalance = previousBalance;
      var $visiblePsiCashInterface = $('#psicash-block .psicash-interface').not('.hidden');
      var $visibleBalanceElem = $visiblePsiCashInterface.find('.psicash-balance'); // There are two different aynchronous animations that we want to want to wait
      // on before declaring this balance change complete: The number ticking up or down,
      // and the CSS delta transition. We're going to use promises to keep track of them finishing.

      var tickDefer = $.Deferred();
      var deltaDefer = $.Deferred();
      $.when(tickDefer.promise(), deltaDefer.promise()).always(function () {
        // Resolve the master deferred
        allDoneDefer.resolve();
      });

      var finalTickStep = function finalTickStep(balanceTickInterval) {
        if (!balanceTickInterval) {
          return;
        }

        clearInterval(balanceTickInterval);
        balanceTickInterval = null; // Update all balance fields, not just the visible one.

        $('.psicash-balance').text(formatPsi(newBalance)); // Ticking is all done

        tickDefer.resolve();
      };

      var balanceTickInterval = setInterval(function () {
        balanceStepCount--;

        if (balanceStepCount < 1) {
          // Ticking is done
          finalTickStep(balanceTickInterval);
          return;
        }

        intermediateBalance += balanceStepSize;

        if (extraStepSize > BILLION) {
          intermediateBalance += BILLION;
          extraStepSize -= BILLION;
        }

        $visibleBalanceElem.text(formatPsi(intermediateBalance));
      }, 10); // If our animation is super slow, the whole thing might take too long, so we're
      // also going to time-bound the whole process.

      setTimeout(function () {
        finalTickStep(balanceTickInterval);
      }, balanceStepTime); // We're going to test that $visibleBalanceElem actually exists, otherwise we risk
      // never resolving the animation promise.

      if ($visibleBalanceElem.length > 0) {
        var psiText = formatPsi(balanceDiff);

        if (balanceDiff > 0) {
          // Negative numbers naturally get a '-', but we'll need to add a '+' sign (localized)
          psiText = i18n.t('positive-value-indicator').replace('%d', formatPsi(balanceDiff));
        } // Create and insert the element we'll use for the animation


        var $deltaElem = $('<span class="psicash-balance-delta"></span>').addClass(balanceDiff > 0 ? 'credit' : 'debit').text(psiText).insertAfter($visibleBalanceElem); // It's unfortunate that we have to do the animation using jQuery's .animate()
        // rather than CSS transitions, but transitions seem flaky in IE.

        var animationCSSEnpoint = balanceDiff > 0 ? // credit
        {
          'font-size': '0',
          'opacity': '0.4'
        } : // debit
        {
          'font-size': '500%',
          'opacity': '0'
        };
        var animationPromise = $deltaElem.delay(10).addClass('balance-changing').animate(animationCSSEnpoint, {
          duration: balanceDiff > 0 ? 2000 : 2000,
          easing: 'swing',
          queue: true
        }).promise();
        animationPromise.always(function () {
          $deltaElem.remove();
          deltaDefer.resolve();
        });
      } else {
        // We're not doing the transition animation, so just resolve the deferred
        deltaDefer.resolve();
      }
    }

    return allDoneDefer.promise();
  }
  /**
   * Used to queue balance changes. Required to make sure animations don't get messed up.
   */


  var PsiCashBalanceChange = {
    /**
     * The queue of balances to change to
     * @type {number[]}
     * @private
     */
    _queue: [],

    /**
     * Whether the next balance change will be the very first one
     * @type {boolean}
     * @private
     */
    _first: false,

    /**
     * Enqueues the given balance to change the UI to.
     * @param {!number} newBalance The balance to change to.
     * @param {!boolean} first Whether this is the very first balance change ever (not just
     *  a fresh app start-up).
     */
    push: function PsiCashBalanceChange_push(newBalance, first) {
      if (!_.isNumber(newBalance)) {
        return;
      } // This properly be stored in the queue, but it doesn't matter. There will only ever
      // be one "first", and it'll be the first change.


      this._first = first;

      this._queue.push(newBalance);

      if (this._queue.length === 1) {
        // We're not already processing the queue and need to start
        nextTick(this._pop, this);
      }
    },

    /**
     * @private
     */
    _pop: function PsiCashBalanceChange_pop() {
      var _this = this;

      if (this._queue.length === 0) {
        // We're done
        return;
      } // We process the next item in the queue before removing it. Its presence acts as a
      // flag that we're processing.


      var nextBalance = this._queue[0];
      var changePromise = doBalanceChange(nextBalance, this._first);
      this._first = false;
      changePromise.always(function () {
        _this._queue.shift(); // Keep processing the queue


        _this._pop();
      });
    }
  };
  /**
   * Gets the moment locale matching the current UI locale.
   * @returns {!string}
   */

  function momentLocale() {
    var locale = $('html').attr('lang');

    if (_.startsWith(locale, 'dev')) {
      // Moment has different locale codes for pseudolocales.
      locale = 'x-pseudo';
    } // `locale` is now our starting point. Moment has case-sensitive locale matches, but
    // also has case-inconsistent locale names (e.g., it has "en-SG" in the current
    // release, although that looks to be changed in a future release). It also has
    // exact matching, so it won't recognize "zh" even though it has "zh-cn". So we
    // need to do some massaging and fuzzy-matching.


    locale = locale.toLowerCase(); // Just in case we have "en_US" instead of "en-US".

    locale = locale.replace('_', '-'); // Moment uses "uz-latn" rather than "uz@Latn"

    locale = locale.replace('@', '-');
    var subLocale = locale.split('-')[0];
    var exactLocaleMatch, subLocaleMatch;
    var momentLocales = moment.locales();

    for (var i = 0; i < momentLocales.length; i++) {
      var l = momentLocales[i];

      if (l.toLowerCase() === locale) {
        exactLocaleMatch = l;
        break;
      } // We'll also check "sub-locales". This means that if locale is "pt-pt" then we want
      // to match "pt", and if locale is "zh" we want to match "zh-cn".


      if (subLocale === l) {
        subLocaleMatch = l;
      } // HACK: We're only going to record the first sub-locale match, because we know
      // that "zh-cn" will come before "zh-tw", and that's what we want to match for "zh".
      else if (!subLocaleMatch && subLocale === l.split('-')[0]) {
          subLocaleMatch = l;
        }
    }

    if (!exactLocaleMatch) {
      if (!subLocaleMatch) {
        DEBUG_WARN('missing momentjs locale:', locale, '; falling back to English');
      } else {
        DEBUG_WARN('missing momentjs locale:', locale, '; using sublocale match:', subLocaleMatch);
      }
    }

    return exactLocaleMatch || subLocaleMatch || 'en';
  }
  /**
   * Event handler for the "buy speed boost" button click.
   */


  function buySpeedBoostClick() {
    if (g_lastState !== 'connected') {
      showNoticeModal('psicash#mustconnect-modal#title', 'psicash#mustconnect-modal#body', null, null, function () {
        switchToTab('#connection-tab');
      });
      return;
    }

    var distinguisher = $(this).data('distinguisher');
    var expectedPrice = $(".psicash-sb-price[data-distinguisher=\"".concat(distinguisher, "\"]")).data('expectedPrice'); // Set the purchase-in-progress state and update UI.

    PsiCashStore.set('purchaseInProgress', true);
    psiCashUIUpdater();
    HtmlCtrlInterface_PsiCashCommand(new PsiCashCommandPurchase('speed-boost', distinguisher, expectedPrice)).then(function (result) {
      // Clear the purchase-in-progress state and update UI.
      PsiCashStore.set('purchaseInProgress', false);

      if (result.refresh) {
        // The reponse supplied refresh data
        psiCashUIUpdater(result.refresh);
      } else {
        // We need to do a full refresh
        HtmlCtrlInterface_PsiCashCommand(new PsiCashCommandRefresh('purchase-response'));
      }

      if (result.error) {
        // Catastrophic failure. Show a modal error and hope the user can figure it out.
        showNoticeModal('psicash#transaction-error-title', 'psicash#transaction-error-body', 'general#notice-modal-tech-preamble', result.error, null); // callback
      } else {
        switch (result.status) {
          case PsiCashServerResponseStatus.ExistingTransaction:
            // There's an existing transaction that this purchase attempt conflicts with.
            // This is a weird state, since a non-expired purchase should have prevented
            // the purchase attempt in the first place. Hopefully the attempt request has
            // corrected our clock skew with the server, and now the existing purchase
            // will be indicated correctly.
            showNoticeModal('psicash#transaction-ExistingTransaction-title', 'psicash#transaction-ExistingTransaction-body', null, // tech detail preamble
            null, // tech detail body
            null); // callback

            break;

          case PsiCashServerResponseStatus.InsufficientBalance:
            // The user doesn't have enough credit to make this purchase. This is unusual,
            // as we don't allow the user to have make a purchase that they can't afford.
            // It can happen if the user has local "optimistic" credit that hasn't cleared
            // on the server, or if the user's balance has changed elsewhere. (But we
            // don't actually have optimistic balance usage in the Windows client yet.)
            showNoticeModal('psicash#transaction-InsufficientBalance-title', 'psicash#transaction-InsufficientBalance-body', null, // tech detail preamble
            null, // tech detail body
            null); // callback

            break;

          case PsiCashServerResponseStatus.TransactionAmountMismatch:
            // The price that we thought the purchase cost is different from what the server
            // thinks it costs. We'll need a data refresh to get new prices.
            showNoticeModal('psicash#transaction-TransactionAmountMismatch-title', 'psicash#transaction-TransactionAmountMismatch-body', null, // tech detail preamble
            null, // tech detail body
            null); // callback

            break;

          case PsiCashServerResponseStatus.TransactionTypeNotFound:
            // The kind of thing we try tried to buy doesn't exist on the server. This is
            // very unlikely to happen, except maybe for very old clients.
            showNoticeModal('psicash#transaction-TransactionTypeNotFound-title', 'psicash#transaction-TransactionTypeNotFound-body', null, // tech detail preamble
            null, // tech detail body
            null); // callback

            break;

          case PsiCashServerResponseStatus.InvalidTokens:
            // The tokens we tried to use were not accepted by the server.
            // This shouldn't happen for Trackers, barring DB replication lag.
            // TODO: support Accounts
            showNoticeModal('psicash#transaction-InvalidTokens-title', 'psicash#transaction-InvalidTokens-body', null, // tech detail preamble
            null, // tech detail body
            null); // callback

            break;

          case PsiCashServerResponseStatus.ServerError:
            // The server gave a 500-ish error
            showNoticeModal('psicash#transaction-ServerError-title', 'psicash#transaction-ServerError-body', null, // tech detail preamble
            null, // tech detail body
            null); // callback

            break;

          case PsiCashServerResponseStatus.Success:
            // The purchase succeeded. We need to reconnect to apply the authorization.
            displayCornerAlert($('#psicash-transaction-purchase-complete')); // Suppress home page opening after reconnect (because it's a terrible UX).

            HtmlCtrlInterface_ReconnectTunnel(true);
            break;

          default:
            throw new Error('Unknown PsiCashServerResponseStatus received: ' + result.status);
        }
      }
    });
  }

  $('.psicash-buy').click(buySpeedBoostClick);
  /**
   * Event handler for the "buy PsiCash with real money" button click.
   */

  function buyPsiClick(e) {
    if (g_lastState !== 'connected') {
      e.preventDefault();
      showNoticeModal('psicash#mustconnect-modal#title', 'psicash#mustconnect-modal#body', null, null, function () {
        switchToTab('#connection-tab');
      });
      return;
    } // Otherwise allow the default action

  }

  $('a.psicash-buy-psi').click(buyPsiClick);
  /**
   * Format a numeric amount of PsiCash, for display in the UI.
   * @param {!number} nanopsi The amount of PsiCash, in nanopsi.
   * @returns {!string} The formatted PsiCash value.
   */

  function formatPsi(nanopsi) {
    if (!_.isNumber(nanopsi)) {
      nanopsi = parseInt(nanopsi);
    }

    var BILLION = 1e9;
    var currLang = $('html').attr('lang'); // NOTE: If you're testing in Chrome, toLocaleString will behave differently than in IE.
    // For example, in Chrome locale='ar' doesn't seem to result in any change, but
    // locale='ar-EG' does. In IE11 they both do.

    var psi = nanopsi / BILLION; // Reduce to two decimal places for display

    psi = Math.floor(psi * 100) / 100; // Localize number

    try {
      psi = psi.toLocaleString(currLang);
    } catch (e) {
      // Our test locales (like devltr) are no valid and will cause an exception. Just fall
      // back to English.
      psi = psi.toLocaleString('en');
    } // Old IE seems to always localize to English, with `.00` suffix. We want to strip off
    // that suffix.


    if (psi.substring(psi.length - '.00'.length) === '.00') {
      psi = psi.substring(0, psi.length - '.00'.length);
    }

    return psi;
  }
  /**
   * Called when tunnel core indicates that there was an attempt to access a
   * port disallowed by the current traffic rules. We will show an alert to
   * encourage the user to buy Speed Boost.
   */


  function handleDisallowedTrafficNotice() {
    if (PsiCashStore.data.uiState === PsiCashUIState.ACTIVE_BOOST) {
      // If we're boosting, then any disallowed traffic is something that won't
      // be let through by purchasing speed boost, so logging, etc., is pointless.
      DEBUG_LOG('handleDisallowedTrafficNotice: already boosting');
      return;
    }

    addLog({
      priority: 2,
      // high
      message: 'Disallowed traffic detected; please purchase Speed Boost'
    });

    if (g_initObj.Settings.DisableDisallowedTrafficAlert) {
      // User has disabled this alert in the settings
      DEBUG_LOG('handleDisallowedTrafficNotice: DisableDisallowedTrafficAlert is true');
      return;
    }

    if (!handleDisallowedTrafficNotice.alertDisallowedTraffic) {
      // We have already shown the alert this session.
      DEBUG_LOG('handleDisallowedTrafficNotice: alert already shown this session');
      return;
    } // else show the alert


    DEBUG_LOG('handleDisallowedTrafficNotice: showing alert');
    handleDisallowedTrafficNotice.alertDisallowedTraffic = false;
    HtmlCtrlInterface_DisallowedTraffic();
    showNoticeModal('notice#disallowed-traffic-alert-title', 'notice#disallowed-traffic-alert-body', null, null, function () {
      if (compareIEVersion('gte', 9, true)) {
        var psicashBlock = $('#psicash-block');

        if (!psicashBlock.hasClass('hidden')) {
          $('#psicash-block').addClass('draw-attention'); // The animation is 1s of movement, and we want the effect to linger for a bit.

          setTimeout(function () {
            return $('#psicash-block').removeClass('draw-attention');
          }, 1500);
        }
      }
    });
  }

  handleDisallowedTrafficNotice.alertDisallowedTraffic = true;
  $window.on(CONNECTED_STATE_CHANGE_EVENT, function () {
    if (g_lastState === 'stopped') {
      // After a hard stop, we will again show the "disallowed traffic" alert one time.
      DEBUG_LOG('handleDisallowedTrafficNotice: resetting alertDisallowedTraffic to true because connected state stopped');
      handleDisallowedTrafficNotice.alertDisallowedTraffic = true;
    }
  });
  PsiCashStore.subscribe('uiState', function () {
    if (PsiCashStore.data.uiState === PsiCashUIState.ACTIVE_BOOST && !handleDisallowedTrafficNotice.boosting) {
      DEBUG_LOG('handleDisallowedTrafficNotice: boost started');
      handleDisallowedTrafficNotice.boosting = true;
    } else if (handleDisallowedTrafficNotice.boosting) {
      // We were boosting and now we're not. Show the "disallowed traffic" alert again.
      DEBUG_LOG('handleDisallowedTrafficNotice: resetting alertDisallowedTraffic to true because boost ended');
      handleDisallowedTrafficNotice.boosting = false;
      handleDisallowedTrafficNotice.alertDisallowedTraffic = true;
    }
  });
  /* UI HELPERS ****************************************************************/

  function displayCornerAlert(elem) {
    // Show -- and then hide -- the alert
    var appearAnimationTime = 300;
    var showingTime = 4000;
    var disappearAnimationTime = 1000;
    $(elem).toggle('fold', {
      horizFirst: true
    }, appearAnimationTime, function () {
      setTimeout(function () {
        $(elem).toggle('fold', {
          horizFirst: true
        }, disappearAnimationTime);
      }, showingTime);
    });
  } // Make the given tab visible. `tab` may be a selector, a DOM element, or a
  // jQuery object. If `callback` is provided, it will be invoked when tab is shown.


  function switchToTab(tab, callback) {
    var $tab = $(tab);

    if ($tab.hasClass('active')) {
      // Tab already showing.
      if (callback) {
        nextTick(callback);
      }
    } else {
      // Settings tab not already showing. Switch to it before expanding and scrolling.
      if (callback) {
        $tab.find('[data-toggle="tab"]').one('show', callback);
      }

      $tab.find('[data-toggle="tab"]').tab('show');
    }
  }
  /**
   * Shows a modal box. String table keys will be used for filling in the content.
   * The "tech" values are optional.
   * @param {!string} titleKey
   * @param {!string} bodyKey
   * @param {?string} techPreambleKey
   * @param {?string} techInfoString An explicit string -- not a string table key.
   * @param {?callback} closedCallback Optional and will be called when the modal is closed.
   */


  function showNoticeModal(titleKey, bodyKey, techPreambleKey, techInfoString, closedCallback) {
    DEBUG_ASSERT(titleKey && bodyKey, 'missing titleKey or bodyKey', titleKey, bodyKey);
    var $modal = $('#NoticeModal');
    $modal.find('.modal-title').html(i18n.t(titleKey));
    $modal.find('.notice-modal-body').html(i18n.t(bodyKey));

    if (techPreambleKey && techInfoString) {
      $modal.find('.notice-modal-tech-preamble').html(i18n.t(techPreambleKey));
      $modal.find('.notice-modal-tech-info').text(techInfoString);
      $modal.find('.notice-modal-tech').removeClass('hidden');
    } else {
      $modal.find('.notice-modal-tech').addClass('hidden');
    } // Put up the modal


    $modal.modal({
      show: true,
      backdrop: 'static'
    }).one('hidden', function () {
      if (closedCallback) {
        closedCallback();
      }
    });
  }
  /* MISC HELPERS AND UTILITIES ************************************************/
  // Support the `data-match-height` feature.


  function doMatchHeight() {
    var i,
        j,
        $elem,
        matchSelector,
        matchSelectorsToMaxHeight = {},
        $matchSelectorMatches;
    var $elemsToChange = $('[data-match-height]'); //
    // Reset previously adjusted heights; record the match selectors.
    //

    for (i = 0; i < $elemsToChange.length; i++) {
      $elem = $elemsToChange.eq(i); // Store the original padding, if we don't already have it.

      if (_.isUndefined($elem.data('match-height-orig-padding-top'))) {
        $elem.data('match-height-orig-padding-top', parseInt($elem.css('padding-top'))).data('match-height-orig-padding-bottom', parseInt($elem.css('padding-bottom')));
      } // Reset the padding to its original state


      $elem.css('padding-top', $elem.data('match-height-orig-padding-top')).css('padding-bottom', $elem.data('match-height-orig-padding-bottom'));
      matchSelector = $elem.data('match-height');
      matchSelectorsToMaxHeight[matchSelector] = null;
    } //
    // Alter the heights.
    //


    for (i = 0; i < $elemsToChange.length; i++) {
      $elem = $elemsToChange.eq(i);
      matchSelector = $elem.data('match-height'); // If we haven't already determined the max for this selector, calculate it

      if (matchSelectorsToMaxHeight[matchSelector] === null) {
        $matchSelectorMatches = $(matchSelector);

        for (j = 0; j < $matchSelectorMatches.length; j++) {
          matchSelectorsToMaxHeight[matchSelector] = Math.max(matchSelectorsToMaxHeight[matchSelector], $matchSelectorMatches.eq(j).height());
        }
      } // Alter the height.


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
      } else if (align === 'bottom') {
        $elem.css('padding-top', heightDiff + $elem.data('match-height-orig-padding-top'));
      } else {
        // center
        $elem.css('padding-top', heightDiff / 2 + $elem.data('match-height-orig-padding-top')).css('padding-bottom', heightDiff / 2 + $elem.data('match-height-orig-padding-bottom'));
      }
    }
  } // Support the `data-match-width` feature


  function doMatchWidth() {
    var matchSelectors = [],
        $dataMatchElems,
        $elem,
        matchSelector,
        $matchMaxElems,
        matchSelectorMaxWidth,
        i,
        j;
    $dataMatchElems = $('[data-match-width]'); // First reset the widths of all the elements we will be adjusting. Otherwise
    // the find-max-width will be tainted if an element-to-adjust is also part of
    // of the find-max-width selector.

    for (i = 0; i < $dataMatchElems.length; i++) {
      $elem = $dataMatchElems.eq(i);
      $elem.css('width', ''); // Build up a list of the selectors used, with dependent ones at the end.

      matchSelector = $elem.data('match-width');

      if (!_.includes(matchSelectors, matchSelector)) {
        if ($elem.data('match-width-dependent') === 'true') {
          matchSelectors.push(matchSelector);
        } else {
          matchSelectors.unshift(matchSelector);
        }
      }
    } // Then collect the maximums for each of the selectors provided by data-match-width


    for (i = 0; i < matchSelectors.length; i++) {
      matchSelector = matchSelectors[i];
      matchSelectorMaxWidth = 0.0;
      $matchMaxElems = $(matchSelector);

      for (j = 0; j < $matchMaxElems.length; j++) {
        matchSelectorMaxWidth = Math.max(matchSelectorMaxWidth, $matchMaxElems.eq(j).outerWidth());
      } // Apply the max width to the target elements.


      $dataMatchElems = $('[data-match-width="' + matchSelector + '"]');

      for (j = 0; j < $dataMatchElems.length; j++) {
        $elem = $dataMatchElems.eq(j);
        $elem.width(getNetWidth($elem, matchSelectorMaxWidth));
      }
    }

    function getNetWidth($elem, grossWidth) {
      return grossWidth - (parseFloat($elem.css('padding-right')) || 0) - (parseFloat($elem.css('padding-left')) || 0) - (parseFloat($elem.css('border-right-width')) || 0) - (parseFloat($elem.css('border-left-width')) || 0) - (parseFloat($elem.css('margin-right')) || 0) - (parseFloat($elem.css('margin-left')) || 0);
    }
  }

  function getIEVersion() {
    // This is complicated by the fact that the MSHTML control uses a different
    // user agent string than browsers do.
    var ie7_10Match, ie11Match, edgeMatch, tridentMatch, msieMatch;
    var ieVersion = false; // User agents differ between browser and actual application, so we need
    // different checks

    if (IS_BROWSER) {
      // Some care needs to be taken to work with IE11+'s old-version test mode.
      // We can't just use Trident versions.
      // This will match IEv7-10.
      ie7_10Match = window.navigator.userAgent.match(/^Mozilla\/\d\.0 \(compatible; MSIE (\d+)/); // This will match IE11.

      ie11Match = window.navigator.userAgent.match(/Trident\/(\d+)/); // This will match Edgev1 (which we will consider IE12).

      edgeMatch = window.navigator.userAgent.match(/Edge\/(\d+)/);

      if (ie7_10Match) {
        ieVersion = parseInt(ie7_10Match[1]);
      } else if (ie11Match) {
        // Trident version is 4 versions behind IE version.
        ieVersion = parseInt(ie11Match[1]) + 4;
      } else if (edgeMatch) {
        ieVersion = parseInt(edgeMatch[1]);
      }
    } else {
      // This will match MSHTMLv8-11.
      tridentMatch = window.navigator.userAgent.match(/MSIE \d+.*Trident\/(\d+)/); // This will match MSHTMLv7. Note that it must be checked after the Trident match.

      msieMatch = window.navigator.userAgent.match(/MSIE (\d+)/); // This will match Edgev1 (which we will consider IE12).
      // Note that this hasn't been seen in the Wild. MSHTML on Win10 uses Trident/8,
      // which hits the above regex and returns version 12.

      edgeMatch = window.navigator.userAgent.match(/Edge\/(\d+)/);

      if (tridentMatch) {
        // Trident version is 4 versions behind IE version.
        ieVersion = parseInt(tridentMatch[1]) + 4;
      } else if (msieMatch) {
        ieVersion = parseInt(msieMatch[1]);
      } else if (edgeMatch) {
        ieVersion = parseInt(edgeMatch[1]);
      }
    }

    return ieVersion;
  } // `comparison` may be: 'eq', 'lt', 'gt', 'lte', 'gte'


  function compareIEVersion(comparison, targetVersion, acceptNonIE) {
    var ieVersion = getIEVersion();

    if (ieVersion === false) {
      return acceptNonIE;
    }

    if (comparison === 'eq') {
      return ieVersion == targetVersion;
    } else if (comparison === 'lt') {
      return ieVersion < targetVersion;
    } else if (comparison === 'gt') {
      return ieVersion > targetVersion;
    } else if (comparison === 'lte') {
      return ieVersion <= targetVersion;
    } else if (comparison === 'gte') {
      return ieVersion >= targetVersion;
    }

    return false;
  } //
  // We don't have the ability to use real cookies or DOM storage, so we'll store
  // persistent stuff in the registry via the win32 code.
  //


  var g_cookies = g_initObj.Cookies ? JSON.parse(g_initObj.Cookies) : {};
  /**
   * Retrieves the stored cookie value associated with `name`.
   * @param {string} name
   * @returns {} The value for `name`. May be null or undefined.
   */

  function getCookie(name) {
    if (IS_BROWSER) {
      return JSON.parse(window.localStorage.getItem(name)) || g_cookies[name];
    }

    return g_cookies[name];
  }
  /**
   * Persistently stores `value` under the key `name`.
   * @param {string} name The key to store the `value` under.
   * @param {} value The value to be stored. Must be JSON-able.
   */


  function setCookie(name, value) {
    if (IS_BROWSER) {
      window.localStorage.setItem(name, JSON.stringify(value));
    }

    g_cookies[name] = value;
    HtmlCtrlInterface_SetCookies(JSON.stringify(g_cookies));
  } // Sets a short timeout and then calls `callback`
  // `context` is optional. Will be `this` when `callback` is called.


  function nextTick(callback, context) {
    nextTickFn(callback, context)();
  } // Creates a function that, when called, sets a short timeout and then calls `callback`
  // `context` is optional. Will be `this` when `callback` is called.


  function nextTickFn(callback, context) {
    return function nextTickFnInner() {
      var args = arguments;

      var runner = function runner() {
        callback.apply(context, args);
      };

      setTimeout(runner, 0);
    };
  } // Convert hex-style color (#FFFFFF or #FFF) to RGB style (rgb(255, 255, 255)).
  // From http://stackoverflow.com/a/5624139/729729


  function hexToRgb(hex) {
    // Expand shorthand form (e.g. "03F") to full form (e.g. "0033FF")
    var shorthandRegex = /^#?([a-f\d])([a-f\d])([a-f\d])$/i;
    hex = hex.replace(shorthandRegex, function (m, r, g, b) {
      return r + r + g + g + b + b;
    });
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : null;
  } // Draws attention to a button. Note that this may require some extra styling
  // to fully work (so test accordingly).


  function drawAttentionToButton(elem) {
    // We will draw attention by temporarily flaring a box-shadow, and then
    // wiggling the button. The wiggle comes from CSS animation (see main.less).
    var backgroundColor, backgroundColorRGB, shadowColor;
    var $elem = $(elem);
    var originalBoxShadow = $elem.data('drawAttentionToButton-originalBoxShadow');

    if (!originalBoxShadow) {
      originalBoxShadow = $elem.css('box-shadow');
      $elem.data('drawAttentionToButton-originalBoxShadow', originalBoxShadow);
    } // The box shadow will be same colour as the button background, with reduced
    // opacity.


    backgroundColor = $elem.css('background-color');

    if (!backgroundColor) {
      backgroundColor = '#999';
    }

    if (backgroundColor[0] === '#') {
      // hex style; convert to rgb
      backgroundColorRGB = hexToRgb(backgroundColor);
      backgroundColor = 'rgb(' + backgroundColorRGB.r + ', ' + backgroundColorRGB.g + ', ' + backgroundColorRGB.b + ')';
    } // Only add alpha if this is a rgb(..) colour, and not rgba(...) or anything else.


    if (backgroundColor.slice(0, 4) === 'rgb(') {
      shadowColor = 'rgba(' + backgroundColor.slice(4, -1) + ', 0.7)';
    } else {
      shadowColor = backgroundColor;
    } // The effect we're going for is the color draining from outside into the button.


    var animationTimeSecs = 3; // Add the initial internal and external shadow

    $elem.css({
      'transition': '',
      'box-shadow': 'inset 0 0 100px #FFF, 0 0 10px ' + shadowColor
    }); // After giving that CSS change a moment to take effect...

    _.delay(function () {
      // ...add a transition and remove the box shadow. It will disappear slowly.
      $elem.css({
        'transition': 'box-shadow ' + animationTimeSecs + 's',
        'box-shadow': 'none'
      }); // When the big box shadow is gone, reset to the original value

      $elem.one('transitionend', function () {
        $elem.css({
          'transition': '',
          'box-shadow': originalBoxShadow
        });
      });
    }, 10); // if this is too low, the effect seems to fail sometimes (like when a port number field is changed in settings)

  }

  function DEBUG_ASSERT(check) {
    if (g_initObj.Config.Debug) {
      if (_.isFunction(check)) {
        check = check();
      }

      if (!check) {
        var message = 'DEBUG_ASSERT failed: ' + check + '; args: \n';

        for (var i = 1; i < arguments.length; i++) {
          message += '\t' + arguments[i] + '\n';
        }

        throw new Error(message);
      }
    }
  }
  /**
   * Returns true if the windows currently has focus; false otherwise.
   * @returns {boolean}
   */


  function windowHasFocus() {
    return document.hasFocus();
  }
  /**
   * Adds a handler that will be called when the windows gets focus.
   * @param {function} handler
   */


  function addWindowFocusHandler(handler) {
    $window.focus(handler);
  }
  /**
   * Generates a pseudo-random string, suitable for non-crypto uniqueness.
   * @returns {string}
   */


  function randomID() {
    return base64.encode(Math.random());
  }
  /**
   * Splits the given URL into components that can be accessed with `result.hash`, etc.
   * @param {string} url
   * @returns {HTMLAnchorElement}
   */


  function urlComponents(url) {
    var parser = document.createElement('a');
    parser.href = url;
    return parser;
  }
  /* DEBUGGING *****************************************************************/
  // Some functionality to help us debug (and demo) in browser.


  $(function debugInit() {
    if (!g_initObj.Config.Debug) {
      $('#debug-tab, #debug-pane').remove();
      return;
    } // Make the connect button "work" in browser mode


    $('#connect-toggle a').click(function (e) {
      if (!IS_BROWSER) {
        return;
      }

      e.preventDefault();
      var buttonConnectState = $(this).parents('.connect-toggle-content').data('connect-state');

      if (buttonConnectState === 'stopped') {
        console.log('DEBUG: connection starting');
        HtmlCtrlInterface_SetState({
          state: 'starting'
        });
        setTimeout(function () {
          HtmlCtrlInterface_SetState({
            state: 'connected'
          });
        }, 5000);
      } else if (buttonConnectState === 'starting' || buttonConnectState === 'connected') {
        console.log('DEBUG: connection stopping');
        HtmlCtrlInterface_SetState({
          state: 'stopping'
        });
        setTimeout(function () {
          HtmlCtrlInterface_SetState({
            state: 'stopped'
          });
        }, 5000);
      } // the stopping button is disabled

    }); // Keep state combo up-to-date

    function updateStateCombo() {
      $('#debug-state').val(g_lastState);
    }

    $window.on(CONNECTED_STATE_CHANGE_EVENT, updateStateCombo);
    updateStateCombo(); // Change state when combo changes

    $('#debug-state').on('change', function () {
      HtmlCtrlInterface_SetState({
        state: $('#debug-state').val()
      });
    }); // Wire up AddLog

    $('#debug-log a').click(function () {
      HtmlCtrlInterface_AddLog({
        message: $('#debug-log input').val(),
        priority: parseInt($('#debug-log select').val())
      });
    }); // Wire up the AvailableEgressRegions notice

    $('#debug-AvailableEgressRegions a').click(function () {
      var regions = [],
          regionCheckboxes = $('#debug-AvailableEgressRegions input');

      for (var i = 0; i < regionCheckboxes.length; i++) {
        if (regionCheckboxes.eq(i).prop('checked')) {
          regions.push(regionCheckboxes.eq(i).val());
        }
      }

      HtmlCtrlInterface_AddNotice({
        noticeType: 'AvailableEgressRegions',
        data: {
          regions: regions
        }
      });
    }); // Wire up the UpstreamProxyError notice

    $('#debug-UpstreamProxyError a').click(function () {
      HtmlCtrlInterface_AddNotice({
        noticeType: 'UpstreamProxyError',
        data: {
          message: $('#debug-UpstreamProxyError input').val()
        }
      });
    }); // Wire up the TrafficRateLimits notice

    $('#debug-TrafficRateLimits a').click(function () {
      HtmlCtrlInterface_AddNotice({
        noticeType: 'TrafficRateLimits',
        data: {
          downstreamBytesPerSecond: Number($('#debug-TrafficRateLimits input').val())
        }
      });
    }); // Wire up the HttpProxyPortInUse notice

    $('#debug-HttpProxyPortInUse a').click(function () {
      HtmlCtrlInterface_AddNotice({
        noticeType: 'HttpProxyPortInUse'
      });
    }); // Wire up the SocksProxyPortInUse notice

    $('#debug-SocksProxyPortInUse a').click(function () {
      HtmlCtrlInterface_AddNotice({
        noticeType: 'SocksProxyPortInUse'
      });
    }); // Wire up the SystemProxySettings::SetProxyError test

    $('#debug-SetProxyError a').click(function () {
      HtmlCtrlInterface_AddNotice({
        noticeType: 'SystemProxySettings::SetProxyError'
      });
    }); // Wire up the SystemProxySettings::SetProxyWarning test

    $('#debug-SetProxyWarning a').click(function () {
      HtmlCtrlInterface_AddNotice({
        noticeType: 'SystemProxySettings::SetProxyWarning',
        data: '[CONN NAME]'
      });
    }); // Wire up the UpdateDpiScaling test

    $('#debug-UpdateDpiScaling a').click(function () {
      HtmlCtrlInterface_UpdateDpiScaling({
        dpiScaling: $('#debug-UpdateDpiScaling input').val()
      });
    }); // Wire up the RefreshPsiCash test

    $('#debug-RefreshPsiCash a').click(function debugRefreshPsiCashClick() {
      if (!$('#debug-RefreshPsiCash-balance').val() || !$('#debug-RefreshPsiCash-price-1hr').val() || !$('#debug-RefreshPsiCash-price-24hr').val()) {
        return;
      }

      var msg = makeTestRefreshMsg(null);
      HtmlCtrlInterface_PsiCashMessage(msg);
      setCookie('debug-RefreshPsiCash-balance', msg.payload.balance);
      setCookie('debug-RefreshPsiCash-price-1hr', msg.payload.purchase_prices.find(function (pp) {
        return pp.distinguisher === '1hr';
      }).price);
      setCookie('debug-RefreshPsiCash-price-24hr', msg.payload.purchase_prices.find(function (pp) {
        return pp.distinguisher === '24hr';
      }).price);
    }); // Wire up the PsiCash InitDone test

    $('#debug-PsiCashInitDone a').click(function debugPsiCashInitDoneClick() {
      testInitDoneResponse();
    });
    var BILLION = 1e9; // Populate the PsiCash balance and price

    $('#debug-RefreshPsiCash-balance').val(getCookie('debug-RefreshPsiCash-balance') ? getCookie('debug-RefreshPsiCash-balance') / BILLION : '');
    $('#debug-RefreshPsiCash-price-1hr').val(getCookie('debug-RefreshPsiCash-price-1hr') ? getCookie('debug-RefreshPsiCash-price-1hr') / BILLION : '');
    $('#debug-RefreshPsiCash-price-24hr').val(getCookie('debug-RefreshPsiCash-price-24hr') ? getCookie('debug-RefreshPsiCash-price-24hr') / BILLION : ''); // Wire up the Disallowed Traffic test

    $('#debug-DisallowedTraffic a').click(function debugPsiCashInitDoneClick() {
      handleDisallowedTrafficNotice();
    });
  });
  /**
   * Construct a PsiCashMessageData object from the fields in the debug interface.
   * @param {?string} msg_id The message.id to use in the "response".
   * @returns {!PsiCashMessageData}
   */

  function makeTestRefreshMsg(msg_id) {
    var purchase = null;
    /** @type {?PsiCashPurchase} */

    if ($('#debug-RefreshPsiCash-boost-remaining').val()) {
      purchase = {
        id: 'purchase-id',
        'class': 'speed-boost',
        distinguisher: '1hr',
        localTimeExpiry: moment().add(parseFloat($('#debug-RefreshPsiCash-boost-remaining').val()), 'minutes').toISOString()
      };
    }
    /** @type {PsiCashMessageData} */


    var msg = {
      type: PsiCashMessageTypeEnum.REFRESH,
      id: msg_id,
      payload: makeTestRefreshPayload(purchase)
    };
    return msg;
  }
  /**
   *
   * @param {?PsiCashPurchase} purchase
   * @returns {PsiCashRefreshData}
   */


  function makeTestRefreshPayload(purchase) {
    var BILLION = 1e9;
    return {
      valid_token_types: ['spender', 'earner', 'indicator'],
      balance: parseFloat($('#debug-RefreshPsiCash-balance').val()) * BILLION,
      purchase_prices: [{
        'class': 'speed-boost',
        distinguisher: '1hr',
        price: parseFloat($('#debug-RefreshPsiCash-price-1hr').val()) * BILLION
      }, {
        'class': 'speed-boost',
        distinguisher: '24hr',
        price: parseFloat($('#debug-RefreshPsiCash-price-24hr').val()) * BILLION
      }],
      purchases: purchase ? [purchase] : null,
      buy_psi_url: 'https://buy.psi.cash/#psicash=example'
    };
  }
  /**
   * Mimic an init-done message (via C code).
   */


  function testInitDoneResponse() {
    var error = $('#debug-PsiCashInitDone-error')[0].checked;
    var recovered = $('#debug-PsiCashInitDone-recovered')[0].checked;
    /** @type {PsiCashMessageData} */

    var msg = {
      type: PsiCashMessageTypeEnum.INIT_DONE,
      id: '',
      payload: error ? {
        error: 'test init error message',
        recovered: recovered
      } : {}
    };
    setTimeout(function () {
      return HtmlCtrlInterface_PsiCashMessage(msg);
    }, 200);
  }
  /**
   * Mimic a refresh response from the server (via C code).
   * @param {!PsiCashCommandPurchase} command
   */


  function testRefreshResponse(command) {
    var msg = makeTestRefreshMsg(command.id); // Pretend the request takes a while.

    setTimeout(function () {
      return HtmlCtrlInterface_PsiCashMessage(msg);
    }, 2000);
  }
  /**
   * Mimic a purchase response from the server (via C code).
   * @param {!PsiCashCommandPurchase} command
   */


  function testPurchaseResponse(command) {
    var resp = $('#debug-PsiCashSpeedBoost-response').val();
    /** @type {PsiCashPurchaseResponse} */

    var msgPayload = {
      error: null,
      status: PsiCashServerResponseStatus.Invalid,
      purchase: null
    };
    /** @type {PsiCashMessageData} */

    var msg = {
      type: PsiCashMessageTypeEnum.NEW_PURCHASE,
      id: command.id,
      payload: msgPayload
    };

    if (resp === 'error') {
      msg.payload.error = 'debug error';
    } else if (PsiCashServerResponseStatus[resp] === PsiCashServerResponseStatus.Success) {
      msg.payload.status = PsiCashServerResponseStatus.Success;
      var expiry = moment().add(1, 'hour').toISOString();

      if (command.distinguisher === '24hr') {
        expiry = moment().add(24, 'hour').toISOString();
      }
      /** @type {PsiCashPurchase} */


      var purchase = {
        id: 'debugpurchaseid',
        'class': command.transactionClass,
        // quoting key b/c it's a keyword and old IE will complain
        distinguisher: command.distinguisher,
        localTimeExpiry: expiry,
        serverTimeExpiry: expiry
      };
      msg.payload.refresh = makeTestRefreshPayload(purchase);
      debugSetPsiCashData({
        balance: g_PsiCashData.balance - command.expectedPrice,
        purchases: [purchase]
      });
    } else {
      msg.payload.status = PsiCashServerResponseStatus[resp];
    } // Pretend the request takes a while.


    setTimeout(function () {
      return HtmlCtrlInterface_PsiCashMessage(msg);
    }, 5000);
  }

  function debugSetPsiCashData(data) {
    g_PsiCashData = Object.assign(g_PsiCashData, data);
  }
  /* INTERFACE METHODS *********************************************************/


  var PSIPHON_LINK_PREFIX = 'psi:';
  /* Calls from C code to JS code. */
  // Add new status message.

  function HtmlCtrlInterface_AddLog(jsonArgs) {
    nextTick(function () {
      // Allow object as input to assist with debugging
      var args = _.isObject(jsonArgs) ? jsonArgs : JSON.parse(jsonArgs);
      addLog(args);
    });
  } // Add new notice. This may be interpreted and acted upon.


  function HtmlCtrlInterface_AddNotice(jsonArgs) {
    nextTick(function () {
      // Allow object as input to assist with debugging
      var args = _.isObject(jsonArgs) ? jsonArgs : JSON.parse(jsonArgs);

      if (args.noticeType === 'UpstreamProxyError') {
        upstreamProxyErrorNotice(args.data.message);
      } else if (args.noticeType === 'HttpProxyPortInUse' || args.noticeType === 'SocksProxyPortInUse') {
        localProxyPortConflictNotice(args.noticeType);
      } else if (args.noticeType === 'AvailableEgressRegions') {
        // Store the value in a cookie so that it's available at the next startup.
        setCookie('AvailableEgressRegions', args.data.regions); // Update the UI.

        updateAvailableEgressRegions(true);
      } else if (args.noticeType === 'ServerAlert') {
        if (args.data.reason === 'disallowed-traffic') {
          handleDisallowedTrafficNotice();
        }
      } else if (args.noticeType === 'TrafficRateLimits') {
        // We are interested in the connection speed when _not_ Boosted
        if (PsiCashStore.data.uiState !== PsiCashUIState.ACTIVE_BOOST) {
          // Store the value in a cookie so that it's available at the next startup.
          setCookie('BaselineRateLimit', args.data.downstreamBytesPerSecond || Number.MAX_SAFE_INTEGER); // Update the UI.

          psiCashUIUpdater();
        }
      } else if (args.noticeType === 'SystemProxySettings::SetProxyError') {
        showNoticeModal('notice#systemproxysettings-setproxy-error-title', 'notice#systemproxysettings-setproxy-error-body', null, null, null);
      } else if (args.noticeType === 'SystemProxySettings::SetProxyWarning') {
        var setProxyWarningTemplate = i18n.t('notice#systemproxysettings-setproxy-warning-template');
        addLog({
          priority: 2,
          // high
          message: _.template(setProxyWarningTemplate)({
            data: args.data
          })
        });
      }
    });
  } // Set the connected state.
  // We will de-bounce the state change messages.


  var g_timeoutSetState = null;

  function HtmlCtrlInterface_SetState(jsonArgs) {
    // Clear any queued state changes.
    if (g_timeoutSetState !== null) {
      clearTimeout(g_timeoutSetState);
      g_timeoutSetState = null;
    }

    g_timeoutSetState = setTimeout(function () {
      g_timeoutSetState = null; // Allow object as input to assist with debugging

      var args = _.isObject(jsonArgs) ? jsonArgs : JSON.parse(jsonArgs);
      g_lastState = args.state;
      $window.trigger(CONNECTED_STATE_CHANGE_EVENT);
    }, 100);
  } // Refresh the current settings values.


  function HtmlCtrlInterface_RefreshSettings(jsonArgs) {
    nextTick(function () {
      var args = JSON.parse(jsonArgs);
      refreshSettings(args.settings, true);

      if (args.success) {
        if (args.reconnectRequired) {
          // backend is reconnecting to apply the settings
          displayCornerAlert($('#settings-apply-alert'));
        } else {
          displayCornerAlert($('#settings-save-alert'));
        }
      } // else an error occurred when saving settings... TODO: tell user?

    });
  } // Indicate a DPI-based scaling change.


  function HtmlCtrlInterface_UpdateDpiScaling(jsonArgs) {
    DEBUG_LOG('HtmlCtrlInterface_UpdateDpiScaling called'); // Allow object as input to assist with debugging

    var args = _.isObject(jsonArgs) ? jsonArgs : JSON.parse(jsonArgs);
    nextTick(function () {
      updateDpiScaling(args.dpiScaling);
    });
  }
  /**
   * Called from C code when something PsiCash-related occurs, such as a data refresh or
   * purchase completion.
   * @param {PsiCashMessageData|string} jsonArgs
   */


  function HtmlCtrlInterface_PsiCashMessage(jsonArgs) {
    nextTick(function () {
      DEBUG_LOG('HtmlCtrlInterface_PsiCashMessage', jsonArgs);
      var args;

      try {
        // Allow object as input to assist with debugging
        args = _.isObject(jsonArgs) ? jsonArgs : JSON.parse(jsonArgs);
      } catch (e) {
        HtmlCtrlInterface_Log('HtmlCtrlInterface_PsiCashMessage: JSON parse failed: ' + e);
        return;
      }

      switch (args.type) {
        case PsiCashMessageTypeEnum.REFRESH:
          // Payload is PsiCashRefreshData
          $window.trigger(PsiCashEventTypeEnum.REFRESH, args.payload);
          break;

        case PsiCashMessageTypeEnum.NEW_PURCHASE:
          // Payload is PsiCashPurchaseResponse
          // Nothing special to be done; the promise will be resolved below
          break;

        case PsiCashMessageTypeEnum.INIT_DONE:
          // Payload is PsiCashInitDoneData
          $window.trigger(PsiCashEventTypeEnum.INIT_DONE, args.payload);
          break;
      } // Resolve any promies that's awaiting this message/response.


      if (args.id && !_.isUndefined(g_PsiCashCommandIDToResolver[args.id])) {
        // Trigger a unique event so that the waiting promise can be resolved.
        g_PsiCashCommandIDToResolver[args.id].call(null, args.payload);
        delete g_PsiCashCommandIDToResolver[args.id];
      }
    });
  }
  /* Calls from JS code to C code. */
  // Let the C code know that the UI is ready.


  function HtmlCtrlInterface_AppReady() {
    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'ready';

      if (IS_BROWSER) {
        console.log(appURL);
      } else {
        window.location = appURL;
      }

      $window.trigger(UI_READY_EVENT);
    });
  } // Give the C code a string table entry in the appropriate language.
  // `locale` is the locale for this string table.
  // The `stringtable` can and should be a full set of key:string mappings, but
  // the strings will be sent to the C code one at a time, to prevent URL size
  // overflow.


  function HtmlCtrlInterface_AddStringTableItem(locale, stringtable) {
    for (var key in stringtable) {
      if (!stringtable.hasOwnProperty(key)) {
        continue;
      }

      var item = {
        locale: locale,
        key: key,
        string: stringtable[key]
      };
      sendStringTableItem(item);
    }

    function sendStringTableItem(itemObj) {
      nextTick(function () {
        var appURL = PSIPHON_LINK_PREFIX + 'stringtable?' + encodeURIComponent(JSON.stringify(itemObj));

        if (IS_BROWSER) {
          console.log(decodeURIComponent(appURL));
        } else {
          window.location = appURL;
        }
      });
    }
  }
  /**
   * Add a log entry to the log pane.
   */


  function HtmlCtrlInterface_Log() {
    var msg = Array.prototype.slice.call(arguments).join(' ');
    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'log?' + encodeURIComponent(msg);

      if (IS_BROWSER) {
        console.log(decodeURIComponent(appURL));
      } else {
        window.location = appURL;
      }
    });
  } // Connection should start.


  function HtmlCtrlInterface_StartTunnel() {
    // Prevent duplicate state change attempts
    if (g_lastState === 'starting' || g_lastState === 'connected') {
      return;
    }

    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'start';

      if (IS_BROWSER) {
        console.log(appURL);
      } else {
        window.location = appURL;
      }
    });
  } // Connection should stop.


  function HtmlCtrlInterface_StopTunnel() {
    // Prevent duplicate state change attempts
    if (g_lastState === 'stopping' || g_lastState === 'disconnected') {
      return;
    }

    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'stop';

      if (IS_BROWSER) {
        console.log(appURL);
      } else {
        window.location = appURL;
      }
    });
  } // The tunnel should be reconnected (if connected or connecting).


  function HtmlCtrlInterface_ReconnectTunnel(suppressHomePage) {
    var appURL = PSIPHON_LINK_PREFIX + 'reconnect?suppress=' + (suppressHomePage ? '1' : '0'); // Prevent duplicate state change attempts

    if (g_lastState === 'stopping' || g_lastState === 'disconnected') {
      return;
    }

    nextTick(function () {
      if (IS_BROWSER) {
        console.log(appURL);
      } else {
        window.location = appURL;
      }
    });
  } // Settings should be saved.


  function HtmlCtrlInterface_SaveSettings(settingsJSON) {
    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'savesettings?' + encodeURIComponent(settingsJSON);

      if (IS_BROWSER) {
        console.log(decodeURIComponent(appURL)); // DEBUG: Make it appear to behave like a real client

        _.delay(HtmlCtrlInterface_RefreshSettings, 100, JSON.stringify({
          settings: JSON.parse(settingsJSON),
          success: true,
          reconnectRequired: g_lastState === 'connected' || g_lastState === 'starting'
        }));
      } else {
        window.location = appURL;
      }
    });
  } // Feedback should be sent.


  function HtmlCtrlInterface_SendFeedback(feedbackJSON) {
    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'sendfeedback?' + encodeURIComponent(feedbackJSON);

      if (IS_BROWSER) {
        console.log(decodeURIComponent(appURL));
      } else {
        window.location = appURL;
      }
    });
  } // Cookies (i.e., UI settings) should be saved.


  function HtmlCtrlInterface_SetCookies(cookiesJSON) {
    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'setcookies?' + encodeURIComponent(cookiesJSON);

      if (IS_BROWSER) {
        console.log(decodeURIComponent(appURL));
      } else {
        window.location = appURL;
      }
    });
  } // Banner was clicked.


  function HtmlCtrlInterface_BannerClick() {
    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'bannerclick';

      if (IS_BROWSER) {
        console.log(decodeURIComponent(appURL));
        alert('Call from JS to C to launch banner URL');
      } else {
        window.location = appURL;
      }
    });
  }
  /**
   * Called when a "disallowed traffic" server alert is encountered. The C code will take
   * steps to make the window visible.
   */


  function HtmlCtrlInterface_DisallowedTraffic() {
    nextTick(function () {
      var appURL = PSIPHON_LINK_PREFIX + 'disallowedtraffic';

      if (IS_BROWSER) {
        console.log(decodeURIComponent(appURL));
        alert('Call from JS to C in response to disallowed traffic');
      } else {
        window.location = appURL;
      }
    });
  }
  /**
   * Used to communicate Promise resolvers between HtmlCtrlInterface_PsiCashCommand (called
   * locally) and HtmlCtrlInterface_PsiCashMessage (called from C code).
   * NOTE: Ideally this would be a map, but our restrictive environment doesn't support them.
   * @type {Map<string, function>}
   */


  var g_PsiCashCommandIDToResolver = {};
  /**
   * A PsiCash "command" should be executed, like purchasing speed boost or refreshing state.
   * Returns a promise that will be resolved when the command completes.
   * @param {!PsiCashCommandBase} command
   * @returns {Promise}
   */

  function HtmlCtrlInterface_PsiCashCommand(command) {
    var commandJSON = JSON.stringify(command);
    var promise = new Promise(function (resolve) {
      g_PsiCashCommandIDToResolver[command.id] = resolve;
      nextTick(function () {
        var appURL = PSIPHON_LINK_PREFIX + 'psicash?' + encodeURIComponent(commandJSON);

        if (IS_BROWSER) {
          console.log(decodeURIComponent(appURL));

          switch (command.command) {
            case PsiCashCommandEnum.REFRESH:
              testRefreshResponse(command);
              break;

            case PsiCashCommandEnum.PURCHASE:
              testPurchaseResponse(command);
              break;
          }
        } else {
          window.location = appURL;
        }
      });
    });
    return promise;
  }
  /* EXPORTS */
  // The C interface code is unable to access functions that are members of objects,
  // so we'll need to directly expose our exports.


  window.HtmlCtrlInterface_AddLog = HtmlCtrlInterface_AddLog; // @ts-ignore

  window.HtmlCtrlInterface_SetState = HtmlCtrlInterface_SetState;
  window.HtmlCtrlInterface_AddNotice = HtmlCtrlInterface_AddNotice;
  window.HtmlCtrlInterface_RefreshSettings = HtmlCtrlInterface_RefreshSettings;
  window.HtmlCtrlInterface_UpdateDpiScaling = HtmlCtrlInterface_UpdateDpiScaling;
  window.HtmlCtrlInterface_PsiCashMessage = HtmlCtrlInterface_PsiCashMessage;
})(window);
//# sourceMappingURL=app.js.map
