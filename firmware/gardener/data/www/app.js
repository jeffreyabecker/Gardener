// What both pages do the same way: name an element, post a form body, put a reply's result on the
// page. The API answers in the form encoding the page already uses, so nothing here parses JSON.

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
