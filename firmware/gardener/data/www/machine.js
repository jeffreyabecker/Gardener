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
  const status = await (await fetch('/api/status')).json();
  machineState = status.state || 'IDLE';
  $('state').textContent = machineState;
  $('hold').textContent =
    status.hold_time_ms == null ? '-' : (status.hold_time_ms / 1000).toFixed(1) + ' s';
  $('pressure').textContent =
    status.pressure_kpa == null ? '-' : status.pressure_kpa.toFixed(2) + ' kPa';
  $('target').textContent = status.target == null ? '-' : status.target.toFixed(1) + ' kPa';
  $('duty').textContent = status.duty == null ? '-' : status.duty.toFixed(1) + ' %';
  $('fault').textContent = status.fault || 'none';
}

// `change` rather than `input`: a number being typed is not a target yet, and a spinner clicked five
// times is one new value, not five.
$('setpoint').addEventListener('change', () => {
  if (machineState === 'IDLE') return;
  act('/api/set', {target: $('setpoint').value});
});

refresh();
setInterval(refresh, 1000);
