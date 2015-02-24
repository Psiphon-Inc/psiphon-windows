;(function(window) {
"use strict";

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

  $(window).resize(function() {
    setTimeout(resizeConnectContent, 1)});
  resizeConnectContent();
});

function resizeConnectContent() {
  // We want the content part of our window to fill the window, we don't want
  // excessive scroll bars, etc. It's difficult to do "fill the remaining height"
  // with just CSS, so we're going to do some on-resize height adjustment in JS.
  var fillHeight = $(window).innerHeight() - $('.main-height').position().top;
  $('.main-height').outerHeight(fillHeight);
  $('.main-height').parentsUntil('.body').add($('.main-height').siblings()).css('height', '100%');

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

function updateConnectToggle() {
  $('.connect-toggle-content').each(function() {
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

function HtmlCtrlInterface_AddMessage(jsonArgs) {
  setTimeout(function() {
    var msgElem = $('<li>');
    msgElem.text(jsonArgs);
    $('#messages').append(msgElem);
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

/* SETTINGS ******************************************************************/

$(function() {
  // ****** FILL VALUES

  // Some fields are disabled in VPN mode
  $('#VPN').change(vpnModeUpdate);
  vpnModeUpdate();

  // Check for valid input in port number fields
  $('.port-entry').keyup(function(event) {checkPortField(event.target);});
  $('.port-entry').each(function() {checkPortField(this);});

  // Disable the other upstream proxy settings if skipping
  $('#SkipUpstreamProxy').change(skipUpstreamProxyUpdate);
  skipUpstreamProxyUpdate();
});

// Returns the numeric port if valid, otherwise false
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

function checkPortField(target) {
  var val = $(target).val();
  var portOK = (validatePort(val) !== false);
  $('.help-inline.'+target.id)
    .toggleClass('hidden', portOK)
    .parents('.control-group').eq(0).toggleClass('error', !portOK);
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

/* EXPORTS */

// The C interface code is unable to access functions that are members of objects,
// so we'll need to directly expose our exports.

window.HtmlCtrlInterface_AddMessage = HtmlCtrlInterface_AddMessage;
window.HtmlCtrlInterface_SetState = HtmlCtrlInterface_SetState;

})(window);
