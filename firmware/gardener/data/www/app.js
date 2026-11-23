// What both pages do the same way: name an element, post a form body, put a reply on the page. The
// status code is the outcome; the body is the fields that answer or the error that explains it.

const $ = (id) => document.getElementById(id);

async function send(path, params) {
  const body = new URLSearchParams(params || {}).toString();
  const response = await fetch(path, {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body
  });
  return {status: response.status, result: new URLSearchParams(await response.text())};
}

function show(reply) {
  if (reply.status < 400) {
    const state = reply.result.get('state');
    $('message').textContent = state ? 'ok: ' + state.toLowerCase() : 'ok';
    return;
  }
  const detail = reply.result.get('reason') || reply.result.get('value') || '';
  $('message').textContent = 'error: ' + (reply.result.get('error') || 'unknown') +
    (detail ? ' (' + detail + ')' : '');
}
