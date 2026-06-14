const http = require('http');

const port = Number(process.env.PORT || 8787);

function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', chunk => {
      body += chunk;
      if (body.length > 1024 * 64) {
        reject(new Error('request too large'));
        req.destroy();
      }
    });
    req.on('end', () => resolve(body));
    req.on('error', reject);
  });
}

function json(res, status, payload) {
  const data = JSON.stringify(payload);
  res.writeHead(status, {
    'content-type': 'application/json',
    'content-length': Buffer.byteLength(data)
  });
  res.end(data);
}

const server = http.createServer(async (req, res) => {
  if (req.method !== 'POST' || req.url !== '/api/token') {
    json(res, 404, { error: 'not found' });
    return;
  }

  try {
    const body = await readBody(req);
    const input = body ? JSON.parse(body) : {};
    const suffix = String(input.uid || 'NO_UID').replace(/[^A-Za-z0-9]/g, '').slice(-10) || 'NOUID';
    const token = `TKN_${Date.now()}_${suffix}`;
    json(res, 200, {
      token,
      url: `https://wstree.cn/t/${token}`,
      urlMode: 'cloud',
      deviceId: input.deviceId || '',
      readerId: input.readerId || ''
    });
  } catch (err) {
    json(res, 400, { error: err.message });
  }
});

server.listen(port, '127.0.0.1', () => {
  console.log(`tcxNFC cloud mock listening on http://127.0.0.1:${port}`);
});
