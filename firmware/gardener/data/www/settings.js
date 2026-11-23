// The settings page: the board's own ID, and the network — where the radio stands, what it can see,
// and the choice the board stores.

async function act(path, params) {
  show(await send(path, params));
  networkState();
}

async function readDevice() {
  const status = await (await fetch('/api/status')).json();
  $('device').textContent = status.device_id || '-';
}

async function networkState() {
  const wifi = await (await fetch('/api/wifi')).json();
  $('mode').textContent = wifi.mode || '-';
  $('network').textContent = wifi.ssid || '-';
  $('address').textContent = wifi.ip || '-';
  $('keep').checked = wifi.keep_ap === true;
  if ($('mode').textContent === 'ap' || $('mode').textContent === 'ap+sta') {
    $('message').textContent = 'joined the setup network';
  }
}

async function scan() {
  const found = await (await fetch('/api/wifi/scan')).json();
  const list = $('networks');
  list.innerHTML = '';
  const networks = found.networks || [];
  for (const network of networks) {
    const item = document.createElement('li');
    item.textContent = network.ssid + '  ' + network.rssi + ' dBm';
    item.onclick = () => { $('ssid').value = network.ssid; };
    list.appendChild(item);
  }
  if (networks.length === 0) $('message').textContent = 'no networks found';
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
    keep_ap: $('keep').checked
  }));
  setTimeout(networkState, 2000);
}

readDevice();
networkState();
setInterval(networkState, 5000);
