// What both pages do the same way: name an element, call the API, put a reply on the page. The
// status code is the outcome; the body is the fields that answer or the error that explains it.

const $ = (id) => document.getElementById(id);

async function send(path, payload) {
  const response = await fetch(path, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(payload || {})
  });
  const text = await response.text();
  return {status: response.status, result: text ? JSON.parse(text) : {}};
}

function show(reply) {
  if (reply.status < 400) {
    const state = reply.result.state;
    $('message').textContent = state ? 'ok: ' + state.toLowerCase() : 'ok';
    return;
  }
  const detail = reply.result.reason ?? reply.result.value ?? '';
  $('message').textContent = 'error: ' + (reply.result.error || 'unknown') +
    (detail === '' ? '' : ' (' + detail + ')');
}
