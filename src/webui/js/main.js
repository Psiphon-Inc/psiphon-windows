;(function(window) {
"use strict";

/* GENERAL */

// Parse whatever JSON parameters were passed by the application.
var g_initObj = {};
(function() {
  var uriSearch = location.search;
  if (uriSearch) {
    g_initObj = JSON.parse(decodeURIComponent(uriSearch.slice(1)));
  }
})();

$(function() {
  // Update the size of our tab content element when the window resizes...
  $(window).smartresize(function() {
    setTimeout(resizeContent, 1);
  });
  // ...and when a tab is activated...
  $('a[data-toggle="tab"]').on('shown', function() {
    setTimeout(resizeContent, 1);
  });
  // ...and now.
  resizeContent();
});

function resizeContent() {
  // We want the content part of our window to fill the window, we don't want
  // excessive scroll bars, etc. It's difficult to do "fill the remaining height"
  // with just CSS, so we're going to do some on-resize height adjustment in JS.
  var fillHeight = $(window).innerHeight() - $('.main-height').position().top;
  $('.main-height').outerHeight(fillHeight);
  $('.main-height').parentsUntil('.body').add($('.main-height').siblings()).css('height', '100%');

  // Let the panes know that content resized
  $('.main-height').trigger('resize');
}

/* CONNECTION ****************************************************************/

var g_lastState = 'stopped';

$(function() {
  $('#start').click(function() {
    if ($(this).hasClass('disabled')) {
      return;
    }
    HtmlCtrlInterface_Start();
  });
  $('#stop').click(function() {
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
  // In the connection toggle box, make all the sub-boxes the same size by
  // padding the top (effectively aligning the bottoms).
  var maxHeight = 0, maxstate = '';
  // Assume the default top padding is the same as the bottom.
  var prePad = parseInt($('#connect-toggle > div').css('padding-bottom'));
  // Reset the padding before calculating height.
  $('#connect-toggle > div').css('padding-top', prePad);

  $('#connect-toggle > div').each(function() {
    maxHeight = Math.max(maxHeight, $(this).outerHeight());
  });
  $('#connect-toggle > div').each(function() {
    var reqPad = (maxHeight - $(this).outerHeight()) + prePad;
    $(this).css('padding-top', reqPad+'px');
  });

  // Set the outer box to the correct height
  $('#connect-toggle').height(maxHeight);

  // Reposition the button part-way down the available space
  var buttonTop = ($('#connection-pane').innerHeight() - maxHeight) / 3;
  $('#connect-toggle').css('top', buttonTop > 0 ? buttonTop : 0);
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

  $('#connect-toggle a').click(function() {
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
}

// Update the main connect button, as well as the connection indicator on the tab.
function updateConnectToggle() {
  $('.connect-toggle-content').each(function() {
    $(this).toggleClass('hidden', $(this).data('connect-state') !== g_lastState);
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
      setTimeout(function() {cycleToggleClass(elem, cls, untilStateChangeFrom)}, 1);
    }
  });
}


/* SETTINGS ******************************************************************/

// We will use this later to check if any settings have changed.
var g_initialSettingsJSON;

$(function() {
  // This is merely to help with testing
  if (!g_initObj.Settings)
  {
    g_initObj.Settings = { "SplitTunnel": 0, "VPN": 0, "LocalHttpProxyPort": 7771, "LocalSocksProxyPort": 7770, "SkipUpstreamProxy": 1, "UpstreamProxyHostname": "upstreamhost", "UpstreamProxyPort": 234, "EgressRegion": "GB", "defaults": { "SplitTunnel": 0, "VPN": 0, "LocalHttpProxyPort": "", "LocalSocksProxyPort": "", "SkipUpstreamProxy": 0, "UpstreamProxyHostname": "", "UpstreamProxyPort": "", "EgressRegion": ""  } };
  }

  fillSettingsValues(g_initObj.Settings);

  // Some fields are disabled in VPN mode
  $('#VPN').change(vpnModeUpdate);
  vpnModeUpdate();

  // Check for valid input in port number fields
  $('.port-entry').on('keyup change blur', function(event) {checkPortField(event.target);});
  $('.port-entry').each(function() {checkPortField(this);});

  // Disable the other upstream proxy settings if skipping
  $('#SkipUpstreamProxy').change(skipUpstreamProxyUpdate);
  skipUpstreamProxyUpdate();

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
        // Settings are invalid. Scroll to the (first) offender and prevent switching tabs.
        $('.tab-content').scrollTo(
          $('#settings-pane .error').eq(0),
          500,            // animation time
          {offset: -50}); // leave some space for the alert
        $('#settings-pane .error input').eq(0).focus();
        e.preventDefault();
        return;
      }
      else if (settingsJSON !== g_initialSettingsJSON) {
        // Settings have changed -- update them in the application (and trigger a reconnect).
        HtmlCtrlInterface_UpdateSettings(settingsJSON);
      }
    }
  });
});

function fillSettingsValues(obj) {
  if (typeof(obj.SplitTunnel) !== 'undefined') {
    $('#SplitTunnel').prop('checked', obj.SplitTunnel);
  }

  if (typeof(obj.VPN) !== 'undefined') {
    $('#VPN').prop('checked', obj.VPN);
  }
  $('#VPN').change(vpnModeUpdate);
  vpnModeUpdate();

  if (typeof(obj.LocalHttpProxyPort) !== 'undefined') {
    $('#LocalHttpProxyPort').val(obj.LocalHttpProxyPort > 0 ? obj.LocalHttpProxyPort : "");
  }
  $('#LocalHttpProxyPort').trigger('keyup');

  if (typeof(obj.LocalSocksProxyPort) !== 'undefined') {
    $('#LocalSocksProxyPort').val(obj.LocalSocksProxyPort > 0 ? obj.LocalSocksProxyPort : "");
  }
  $('#LocalSocksProxyPort').trigger('keyup');

  if (typeof(obj.UpstreamProxyHostname) !== 'undefined') {
    $('#UpstreamProxyHostname').val(obj.UpstreamProxyHostname);
  }

  if (typeof(obj.UpstreamProxyPort) !== 'undefined') {
    $('#UpstreamProxyPort').val(obj.UpstreamProxyPort > 0 ? obj.UpstreamProxyPort : "");
  }
  $('#UpstreamProxyPort').trigger('keyup');

  if (typeof(obj.SkipUpstreamProxy) !== 'undefined') {
    $('#SkipUpstreamProxy').prop('checked', obj.SkipUpstreamProxy);
  }
  $('#SkipUpstreamProxy').change(skipUpstreamProxyUpdate);
  skipUpstreamProxyUpdate();

  if (typeof(obj.EgressRegion) !== 'undefined') {
    $('#EgressRegion').val(obj.EgressRegion);
  }
  $('body select').each(function() {
    if (this.refresh) this.refresh();
  });
}

// Packages the current settings into JSON string. Returns if invalid value found.
function settingsToJSON() {
  var valid = true;

  $('.port-entry').each(function() {
    if (checkPortField(this) === false) {
      valid = false;
    }
  });

  if (!valid) {
    return false;
  }

  var returnValue = {
    VPN: $('#VPN').prop('checked') ? 1 : 0,
    SplitTunnel: $('#SplitTunnel').prop('checked') ? 1 : 0,
    LocalHttpProxyPort: validatePort($('#LocalHttpProxyPort').val()),
    LocalSocksProxyPort: validatePort($('#LocalSocksProxyPort').val()),
    UpstreamProxyHostname: $('#UpstreamProxyHostname').val(),
    UpstreamProxyPort: validatePort($('#UpstreamProxyPort').val()),
    SkipUpstreamProxy: $('#SkipUpstreamProxy').prop('checked') ? 1 : 0,
    EgressRegion: $('#EgressRegion').val()
  };

  return JSON.stringify(returnValue);
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

// Returns the numeric port if valid, otherwise false. Note that 0 is a valid
// return value, and falsy, so use `=== false`.
function checkPortField(target) {
  var val = $(target).val();
  var port = validatePort(val);
  var portOK = (port !== false);
  $('.help-inline.'+target.id)
    .toggleClass('hidden', portOK)
    .parents('.control-group').eq(0).toggleClass('error', !portOK);

  // Show/hide the error alert depending on whether we have an erroneous field
  $('#settings-pane .value-error-alert').toggleClass(
    'hidden', $('#settings-pane .control-group.error').length === 0);

  return port;
}

// Some of the settings are incompatible with VPN mode. We'll modify the display
// depending on the choice of VPN mode.
function vpnModeUpdate() {
  var vpn = $('#VPN').prop('checked');
  $('input.vpn-incompatible:not(.perma-disabled), .vpn-incompatible:not(.perma-disabled) input, '+
    'select.vpn-incompatible:not(.perma-disabled), .vpn-incompatible:not(.perma-disabled) select')
      .prop('disabled', vpn).toggleClass('disabled', vpn);
  $('.vpn-incompatible-msg').toggleClass('invisible', !vpn);
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

/* FEEDBACK ******************************************************************/


/* LOG MESSAGES **************************************************************/

$(function() {
  $('#show-debug-messages').click(showDebugMessagesClicked);
});

function showDebugMessagesClicked() {
  var show = $(this).prop('checked');
  $('.log-messages').toggleClass('showing-priority-0', show);
}

function addLogMessage(obj) {
  var row = $('<tr>');
  var msgCell = $('<td>');
  msgCell.text(obj.message);
  row.addClass('priority-' + obj.priority);
  row.append(msgCell);
  $('.log-messages').prepend(row);

  // The "Show Debug Messages" checkbox is hidden until we actually get a debug
  // message.
  if (obj.priority < 1) {
    $('label[for="show-debug-messages"]').removeClass('invisible');
  }
}

/* INTERFACE METHODS *********************************************************/

function HtmlCtrlInterface_AddMessage(jsonArgs) {
  setTimeout(function() {
    addLogMessage(JSON.parse(jsonArgs));
  }, 1);
}

function HtmlCtrlInterface_SetState(jsonArgs) {
  setTimeout(function() {
    $('#status').text(jsonArgs);
    var args = JSON.parse(jsonArgs);
    $('#start').toggleClass('disabled', (args.state === 'started' || args.state === 'starting'));
    $('#stop').toggleClass('disabled', (args.state === 'stopped' || args.state === 'stopping'));
    $('#connect').toggleClass('disabled', (args.state === 'started' || args.state === 'starting'));
    $('#disconnect').toggleClass('disabled', (args.state === 'stopped' || args.state === 'stopping'));
    if (args.state === 'started' || args.state === 'starting') {
      $('.toggle-connect .toggle-on').toggleClass('btn-success', args.state === 'started')
        .toggleClass('btn-warning', args.state === 'starting');
    }
    else {
      $('.toggle-connect .toggle-off').toggleClass('btn-danger', args.state === 'stopped')
        .toggleClass('btn-warning', args.state === 'stopping');
    }
    g_lastState = args.state;
    updateConnectToggle();
  }, 1);
}

function HtmlCtrlInterface_Start() {
  // Prevent duplicate state change attempts
  if (g_lastState === 'starting' || g_lastState === 'connected') {
    return;
  }
  setTimeout(function() {
    window.location = 'app:start';
  }, 1);
}

function HtmlCtrlInterface_Stop() {
  // Prevent duplicate state change attempts
  if (g_lastState === 'stopping' || g_lastState === 'disconnected') {
    return;
  }
  setTimeout(function() {
    window.location = 'app:stop';
  }, 1);
}

function HtmlCtrlInterface_UpdateSettings(settingsJSON) {
  setTimeout(function() {
    // Don't call encodeURIComponent. The application code can more easily handle a plain string.
    window.location = 'app:updatesettings?' + settingsJSON;
  }, 1);
}


/* EXPORTS */

// The C interface code is unable to access functions that are members of objects,
// so we'll need to directly expose our exports.

window.HtmlCtrlInterface_AddMessage = HtmlCtrlInterface_AddMessage;
window.HtmlCtrlInterface_SetState = HtmlCtrlInterface_SetState;

})(window);
