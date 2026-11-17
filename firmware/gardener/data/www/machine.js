// The machine's page: the readings `get` publishes, and the three commands under them.

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

refresh();
setInterval(refresh, 1000);
