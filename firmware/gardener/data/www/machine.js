// The machine's page: the readings `get` publishes, and the three commands under them.

// The last state the page read. What the setpoint does with a new number depends on it: a running
// machine takes the number as its new target, and an idle one waits for the Set button, because on
// an idle machine a target is what starts it.
let machineState = 'IDLE';

async function act(path, params) {
  show(await send(path, params));
  refresh();
}

async function refresh() {
  const status = new URLSearchParams(await (await fetch('/api/status')).text());
  $('device').textContent = status.get('device_id') || '-';
  machineState = status.get('state') || 'IDLE';
  $('state').textContent = machineState;
  const hold = status.get('hold_time_ms');
  $('hold').textContent = hold === null ? '-' : (Number(hold) / 1000).toFixed(1) + ' s';
  const pressure = status.get('pressure_kpa');
  $('pressure').textContent = pressure === null ? '-' : pressure + ' kPa';
  const target = status.get('target');
  $('target').textContent = target === null ? '-' : target + ' kPa';
  const duty = status.get('duty');
  $('duty').textContent = duty === null ? '-' : duty + ' %';
  $('fault').textContent = status.get('fault') || 'none';
}

// `change` rather than `input`: a number being typed is not a target yet, and a spinner clicked five
// times is one new value, not five.
$('setpoint').addEventListener('change', () => {
  if (machineState === 'IDLE') return;
  act('/api/set', {target: $('setpoint').value});
});

refresh();
setInterval(refresh, 1000);
