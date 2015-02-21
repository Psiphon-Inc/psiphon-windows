$(function() {
  $('#start').click(function() {
    setTimeout(function () { window.location = 'app:start'; }, 0);
  });
  $('#stop').click(function() {
    setTimeout(function () { window.location = 'app:stop'; }, 0);
  });

  $(window).resize(adjustContentHeight);
  adjustContentHeight();
});

// We want the content part of our window to fill the window, we don't want
// excessive scroll bars, etc. It's difficult to do "fill the remaining height"
// with just CSS, so we're going to do some on-resize height adjustment in JS.
function adjustContentHeight() {
  var fillHeight = $(window).innerHeight() - $('.main-height').position().top;
  $('.main-height').outerHeight(fillHeight);
  $('.main-height').parentsUntil('.body').add($('.main-height').siblings()).css('height', '100%');
}

function CtrlInterface_AddMessage(jsonArgs) {
  var msgElem = $('<li>');
  msgElem.text(jsonArgs);
  $('#messages').append(msgElem);
}

function HtmlCtrlInterface_SetState(jsonArgs) {
  $('#status').text(jsonArgs);
  var args = JSON.parse(jsonArgs);
  if (args.state === 'stopped') {
    $('#start').removeClass('disabled');
    $('#stop').addClass('disabled');
  }
  else {
    $('#start').addClass('disabled');
    $('#stop').removeClass('disabled');
  }
}

/* Settings ******************************************************************/

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
