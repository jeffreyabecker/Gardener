// The interval plan runs here, not in the machine: work is a set, rest is a vent, and this page owns
// the clock that alternates them. Nothing about it survives a reload — closing the tab ends the plan
// and leaves the machine in whatever phase it was in.
//
// The fields' own bounds are what keeps a work period inside the machine's ceiling: a work period is
// at most an hour, well under the two-hour run the machine grants, so a plan cannot outlive a run.

const TickMs = 250;

// The plan in flight, or null. `until` is a monotonic deadline; the tick compares it with
// performance.now(), so a late or coalesced timer cannot shorten a phase.
let plan = null;
// One post at a time: a tick that fires while the last one's request is still out would post twice.
let planning = false;

function remainingSeconds() {
  if (!plan) return 0;
  return Math.max(0, Math.ceil((plan.until - performance.now()) / 1000));
}

async function postPlan(path, params) {
  const result = await send(path, params);
  if (result.get('ok') === '1') return true;

  const detail = result.get('reason') || '';
  endPlan();
  $('message').textContent = 'error: ' + (result.get('err') || 'unknown') +
    (detail ? ' (' + detail + ')' : '');
  return false;
}

async function beginWork() {
  plan.phase = 'work';
  plan.until = performance.now() + plan.workMs;
  if (await postPlan('/api/set', {target: plan.target})) $('message').textContent = 'working';
}

// The target stays live while a plan runs. While the machine is working, a new one is posted as the
// set it is, so the pull turns toward it at once; during a rest nothing is posted — a set would start
// the pump and cut the rest short — and the new value is simply what the next work period pulls to.
function setTarget(value) {
  const clamped = Math.min(60, Math.max(0, Number(value)));
  $('targetKpa').value = clamped.toFixed(1);
  if (!plan) return;

  plan.target = clamped;
  if (plan.phase === 'work') postPlan('/api/set', {target: clamped});
}

function nudgeTarget(step) {
  setTarget(Number($('targetKpa').value) + step);
}

async function beginRest() {
  plan.done += 1;
  plan.phase = 'rest';
  plan.until = performance.now() + plan.restMs;
  if (await postPlan('/api/vent')) $('message').textContent = 'resting';
}

function endPlan() {
  plan = null;
  for (const id of ['targetKpa', 'work', 'rest', 'count']) $(id).disabled = false;
  $('startButton').disabled = false;
}

async function startPlan() {
  if (plan) return;

  const target = Number($('targetKpa').value);
  const work = Number($('work').value);
  const rest = Number($('rest').value);
  const count = Number($('count').value);
  if (!(work >= 1 && work <= 3600) || !(rest >= 0 && rest <= 3600) ||
      !(count >= 1 && count <= 999) || !(target >= 0 && target <= 60)) {
    $('message').textContent =
      'error: target 0-60 kPa, work 1-3600 s, rest 0-3600 s, count 1-999';
    return;
  }

  plan = {
    target,
    workMs: work * 1000,
    restMs: rest * 1000,
    count,
    done: 0,
    phase: 'work',
    until: 0
  };
  // The target stays editable while the plan runs; the period lengths and the count do not, because
  // changing those mid-cycle is a different plan, not a tweak.
  for (const id of ['work', 'rest', 'count']) $(id).disabled = true;
  $('startButton').disabled = true;
  await beginWork();
  draw();
}

async function stopPlan() {
  if (!plan) return;
  endPlan();
  if (await postPlan('/api/vent')) $('message').textContent = 'stopped';
  draw();
}

async function tick() {
  if (planning) return;
  if (!plan || performance.now() < plan.until) {
    draw();
    return;
  }

  planning = true;
  try {
    if (plan.phase === 'work') {
      // The work period is over. The vent is the release, and on the last interval it is the ending.
      if (plan.done + 1 >= plan.count) {
        plan.done += 1;
        const released = await postPlan('/api/vent');
        endPlan();
        if (released) $('message').textContent = 'finished';
        return;
      }
      await beginRest();
      return;
    }
    await beginWork();
  } finally {
    planning = false;
    draw();
  }
}

// What the machine says, beside what the plan says: when the two disagree — a fault, a ceiling that
// released a run early, a hand on the machine page — the page shows it rather than pretending.
async function readMachine() {
  const status = new URLSearchParams(await (await fetch('/api/status')).text());
  $('state').textContent = status.get('state') || '-';
  const pressure = status.get('pressure_kpa');
  $('pressure').textContent = pressure === null ? '-' : pressure + ' kPa';
  const target = status.get('target');
  $('target').textContent = target === null ? '-' : target + ' kPa';
}

function draw() {
  $('progress').textContent = plan ? plan.done + ' of ' + plan.count : '-';
  $('phase').textContent = plan ? plan.phase : '-';
  $('left').textContent = plan ? remainingSeconds() + ' s' : '-';
}

$('targetKpa').addEventListener('change', () => setTarget($('targetKpa').value));

readMachine();
draw();
setInterval(readMachine, 1000);
setInterval(tick, TickMs);
