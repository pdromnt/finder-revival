/* Exercise the dashboard's actual API helper with Orca's fetch interception. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../www/app.js'), 'utf8');
// Compile progressively through the complete declaration, independent of wrapping.
const start = source.search(/^async function api\(/m);
let helper;
if (start >= 0) {
  const lines = source.slice(start).split('\n');
  for (let count = 1; count <= lines.length; count++) {
    const candidate = lines.slice(0, count).join('\n');
    try {
      new vm.Script(candidate);
      helper = candidate;
      break;
    } catch (error) {
      if (!(error instanceof SyntaxError)) throw error;
    }
  }
}
assert.ok(helper, 'Dashboard API helper must be found');
const key = 'a'.repeat(64);

async function run(embedded) {
  const calls = [];
  const browserFetch = async (url, init) => {
    // Fetch performs case-insensitive HTTP normalization of a plain JS object.
    calls.push({ url, init, headers: new Headers(init.headers) });
    return { status: 200, ok: true, json: async () => ({ files: [] }) };
  };
  // Model Orca's case-sensitive plain-object injection before browser normalization.
  const injectedFetch = async (url, options = {}) => {
    options.headers ??= {};
    Object.assign(options.headers, { 'X-API-Key': key });
    return browserFetch(url, options);
  };
  const context = vm.createContext({ fetch: embedded ? injectedFetch : browserFetch, apiKey: key });
  vm.runInContext(helper, context);
  await context.api('/api/files');
  await context.api('/api/notifications/dismiss', {
    method: 'POST',
    headers: { 'If-Match': 'fixture-identity' },
  });
  for (const call of calls) {
    assert.equal(
      call.headers.get('X-Api-Key'),
      key,
      'Credential must remain one key after browser normalization',
    );
    assert.equal(call.init.cache, 'no-store');
  }
  assert.equal(calls[1].init.method, 'POST');
  assert.equal(calls[1].headers.get('If-Match'), 'fixture-identity');
}

(async () => {
  await run(false);
  await run(true);
  console.log('Dashboard headers passed in ordinary-browser and Orca-injected fetch contexts.');
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
