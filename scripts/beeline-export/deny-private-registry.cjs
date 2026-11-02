// Test-only Node preload. Not included in the standalone app.
// Use with an empty npm cache and the public registry to prove that installation
// does not fall back to this workspace's private tarball URLs.
const dns = require('node:dns');
const http = require('node:http');
const https = require('node:https');

function rejectPrivate(hostname) {
  if (typeof hostname === 'string' &&
      (hostname.endsWith('.internal') || hostname === 'localhost' ||
       hostname === '127.0.0.1' || hostname === '::1')) {
    throw new Error(`Portability check blocked private registry host: ${hostname}`);
  }
}

const lookup = dns.lookup;
dns.lookup = function (hostname, ...args) {
  rejectPrivate(hostname);
  return lookup.call(this, hostname, ...args);
};

for (const protocol of [http, https]) {
  for (const method of ['request', 'get']) {
    const original = protocol[method];
    protocol[method] = function (input, ...args) {
      if (typeof input === 'string' || input instanceof URL) {
        rejectPrivate(new URL(input).hostname);
      } else if (input) {
        rejectPrivate(input.hostname || input.host);
      }
      return original.call(this, input, ...args);
    };
  }
}
