function CtrlInterface_AddMessage(jsonArgs) {
  var msgElem = document.createElement('li');
  msgElem.textContent = jsonArgs;
  document.getElementById('messages').appendChild(msgElem);
}

function HtmlCtrlInterface_SetState(jsonArgs) {
  document.getElementById('status').textContent = jsonArgs;
  var args = JSON.parse(jsonArgs);
  if (args.state === 'stopped') {
    document.getElementById('start').disabled = false;
    document.getElementById('stop').disabled = true;
  }
  else {
    document.getElementById('start').disabled = true;
    document.getElementById('stop').disabled = false;
  }
}

document.getElementById('start').onclick = function() {
  setTimeout(function () { window.location = 'app:start'; }, 0);
};
document.getElementById('stop').onclick = function() {
  setTimeout(function () { window.location = 'app:stop'; }, 0);
};
