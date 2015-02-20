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
