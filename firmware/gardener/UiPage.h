#pragma once

namespace gardener::runtime {

// The page HttpLink serves at /. One file, no external assets, because the setup network has no
// internet behind it. Its script talks to the API in the API's own encoding: form posts out, form
// bodies read back with URLSearchParams.
constexpr const char* UiPage = R"html(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Gardener</title>
<style>
  :root { color-scheme: light; }
  body { font: 15px system-ui, sans-serif; margin: 0; background: #f4f6f8; color: #1a1a1a; }
  main { max-width: 640px; margin: 0 auto; padding: 16px; }
  h1 { font-size: 20px; margin: 4px 0 16px; }
  section { background: #fff; border: 1px solid #dde3e8; border-radius: 10px; padding: 14px;
            margin-bottom: 14px; }
  h2 { font-size: 15px; margin: 0 0 10px; }
  .stat { display: grid; grid-template-columns: repeat(2, 1fr); gap: 4px 18px; }
  .stat div { display: flex; justify-content: space-between; border-bottom: 1px solid #eef1f4;
              padding: 3px 0; }
  .stat span { color: #667; }
  .row { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; margin-top: 10px; }
  input, button { font: inherit; padding: 7px 10px; border-radius: 7px; }
  input { border: 1px solid #c9d2da; }
  button { border: 1px solid #2563eb; background: #2563eb; color: #fff; cursor: pointer; }
  button.secondary { background: #fff; color: #1a1a1a; border-color: #c9d2da; }
  #message { min-height: 20px; margin: 10px 0 0; color: #444; }
  #networks { list-style: none; margin: 10px 0 0; padding: 0; max-height: 160px; overflow: auto; }
  #networks li { padding: 4px 0; border-bottom: 1px solid #eef1f4; cursor: pointer; }
  #networks li:hover { background: #f4f6f8; }
  .muted { color: #667; font-size: 13px; }
</style>
</head>
<body>
<main>
  <h1>Gardener</h1>

  <section>
    <h2>Machine</h2>
    <div class="stat">
      <div><span>State</span><b id="state">-</b></div>
      <div><span>Pressure</span><b id="pressure">-</b></div>
      <div><span>Target</span><b id="target">-</b></div>
      <div><span>Duty</span><b id="duty">-</b></div>
      <div><span>Fault</span><b id="fault">-</b></div>
    </div>
    <div class="row">
      <input id="setpoint" type="number" min="0" max="60" step="0.1" value="20" style="width:96px">
      <button onclick="act('/api/set', {target: setpoint.value})">Set</button>
      <button class="secondary" onclick="act('/api/vent')">Vent</button>
      <button class="secondary" onclick="act('/api/reset')">Reset</button>
    </div>
    <p id="message"></p>
  </section>

  <section>
    <h2>WiFi</h2>
    <div class="stat">
      <div><span>Mode</span><b id="mode">-</b></div>
      <div><span>Network</span><b id="network">-</b></div>
      <div><span>Address</span><b id="address">-</b></div>
    </div>
    <div class="row">
      <button class="secondary" onclick="scan()">Scan</button>
      <input id="ssid" placeholder="network name" autocomplete="off">
      <input id="pass" type="password" placeholder="password" autocomplete="current-password">
    </div>
    <ul id="networks"></ul>
    <div class="row">
      <label class="muted"><input id="keep" type="checkbox">
        keep the setup network on while joined</label>
    </div>
    <div class="row">
      <button onclick="saveWifi()">Save and join</button>
      <button class="secondary" onclick="act('/api/wifi/forget')">Forget</button>
    </div>
    <p class="muted">Saving joins the selected network. If the setup network disappears, the board
      is on the new network and the serial log prints its address.</p>
  </section>
</main>

<script>
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
</script>
</body>
</html>
)html";

}  // namespace gardener::runtime
