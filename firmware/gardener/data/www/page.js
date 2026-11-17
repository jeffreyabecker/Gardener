const $ = (id) => document.getElementById(id);

async function send(path, params) {
  const body = new URLSearchParams(params || {}).toString();
  const response = await fetch(path, {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body
  });
  return new URLSearchParams(await response.text());
}

function show(result) {
  if (result.get('ok') === '1') {
    const state = result.get('state');
    $('message').textContent = state ? 'ok: ' + state.toLowerCase() : 'ok';
    return;
  }
  const detail = result.get('reason') || result.get('value') || '';
  $('message').textContent = 'error: ' + (result.get('err') || 'unknown') +
    (detail ? ' (' + detail + ')' : '');
}

async function act(path, params) {
  show(await send(path, params));
  refresh();
}

async function refresh() {
  const status = new URLSearchParams(await (await fetch('/api/status')).text());
  $('state').textContent = status.get('state') || '-';
  const pressure = status.get('pressure_kpa');
  $('pressure').textContent = pressure === null ? '-' : pressure + ' kPa';
  const target = status.get('target');
  $('target').textContent = target === null ? '-' : target + ' kPa';
  const duty = status.get('duty');
  $('duty').textContent = duty === null ? '-' : duty + ' %';
  $('fault').textContent = status.get('fault') || 'none';
}

async function wifiState() {
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

async function saveWifi() {
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
  setTimeout(wifiState, 2000);
}

refresh();
wifiState();
setInterval(refresh, 1000);
setInterval(wifiState, 5000);
