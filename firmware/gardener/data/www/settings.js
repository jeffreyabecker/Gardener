// The settings page: the board's own ID, and the network — where the radio stands, what it can see,
// and the choice the board stores.

async function act(path, params) {
  show(await send(path, params));
  networkState();
}

async function readDevice() {
  const status = new URLSearchParams(await (await fetch('/api/status')).text());
  $('device').textContent = status.get('device_id') || '-';
}

async function networkState() {
  const wifi = new URLSearchParams(await (await fetch('/api/wifi')).text());
  $('mode').textContent = wifi.get('mode') || '-';
  $('network').textContent = wifi.get('ssid') || '-';
  $('address').textContent = wifi.get('ip') || '-';
  $('keep').checked = wifi.get('keep_ap') === '1';
  if ($('mode').textContent === 'ap' || $('mode').textContent === 'ap+sta') {
    $('message').textContent = 'joined the setup network';
  }
}

async function scan() {
  const found = new URLSearchParams(await (await fetch('/api/wifi/scan')).text());
  const list = $('networks');
  list.innerHTML = '';
  const count = Number(found.get('count') || 0);
  for (let i = 0; i < count; i++) {
    const name = found.get('ssid' + i) || '';
    const item = document.createElement('li');
    item.textContent = name + '  ' + found.get('rssi' + i) + ' dBm';
    item.onclick = () => { $('ssid').value = name; };
    list.appendChild(item);
  }
  if (count === 0) $('message').textContent = 'no networks found';
}

async function saveNetwork() {
  const ssid = $('ssid').value.trim();
  if (!ssid) {
    $('message').textContent = 'error: enter or pick a network';
    return;
  }
  show(await send('/api/wifi', {
    ssid,
    pass: $('pass').value,
    keep_ap: $('keep').checked ? '1' : '0'
  }));
  setTimeout(networkState, 2000);
}

readDevice();
networkState();
setInterval(networkState, 5000);
