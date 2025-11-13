const express = require('express');
const path = require('path');
const os = require('os');
const { exec, spawn } = require('child_process');

const app = express();
const PORT = process.env.PORT || 8080;
const HOST = '0.0.0.0';

/* ----------------------------------------------
   Utility: get local IPv4 address for LAN display
----------------------------------------------- */
function getLocalIp() {
  const ifaces = os.networkInterfaces();
  for (const list of Object.values(ifaces)) {
    for (const iface of list) {
      if (iface.family === 'IPv4' && !iface.internal) return iface.address;
    }
  }
  return '127.0.0.1';
}
const LOCAL_IP = getLocalIp();

/* ----------------------------------------------
   Middleware
----------------------------------------------- */
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

/* ----------------------------------------------
   Paths to binaries (use absolute paths)
----------------------------------------------- */
// const SETTING_BIN = path.join(__dirname, 'hash_board_setting', 'setting');
// const CHECK_BIN   = path.join(__dirname, 'main_check');
const MINING_BIN  = path.join(__dirname, 'mining_operation', 'mining');
const VERIFY_BIN  = path.join(__dirname, 'hash_board_verification', 'verification');

/* ----------------------------------------------
   System Info API
----------------------------------------------- */
app.get('/api/system/info', (req, res) => {
  res.json({
    firmware: 'Sun Apr 11 20:36:08 CST 2021',
    ip: LOCAL_IP,
    mac: '10:EA:A3:76:F2:17',
    type: 'Release'
  });
});

// /* ----------------------------------------------
   // Hash Board Setting APIs
// ----------------------------------------------- */
// app.post('/api/set_frequency', (req, res) => {
  // const { frequency } = req.body;
  // if (frequency === undefined) return res.status(400).send('Missing frequency');
  // exec(`${SETTING_BIN} --set_frequency ${frequency}`, (err, out, errout) => {
    // if (err) return res.status(500).send(errout || err.message);
    // res.send(out);
  // });
// });

// app.post('/api/read_frequency', (req, res) => {
  // const asic = req.body.asic || 0;
  // exec(`${SETTING_BIN} --read_frequency ${asic}`, (err, out, errout) => {
    // if (err) return res.status(500).send(errout || err.message);
    // res.send(out);
  // });
// });

// app.post('/api/set_base_nonce', (req, res) => {
  // const { asic, value } = req.body;
  // if (asic === undefined || !value) return res.status(400).send('Missing parameters');
  // exec(`${SETTING_BIN} --set_base_nonce ${asic} ${value}`, (err, out, errout) => {
    // if (err) return res.status(500).send(errout || err.message);
    // res.send(out);
  // });
// });

// app.post('/api/read_base_nonce', (req, res) => {
  // const { asic } = req.body;
  // if (asic === undefined) return res.status(400).send('Missing asic');
  // exec(`${SETTING_BIN} --read_base_nonce ${asic}`, (err, out, errout) => {
    // if (err) return res.status(500).send(errout || err.message);
    // res.send(out);
  // });
// });

// app.post('/api/set_range_nonce', (req, res) => {
  // const { value } = req.body;
  // if (!value) return res.status(400).send('Missing value');
  // exec(`${SETTING_BIN} --set_range_nonce ${value}`, (err, out, errout) => {
    // if (err) return res.status(500).send(errout || err.message);
    // res.send(out);
  // });
// });

// app.post('/api/read_range_nonce', (req, res) => {
  // const { asic } = req.body;
  // if (asic === undefined) return res.status(400).send('Missing asic');
  // exec(`${SETTING_BIN} --read_range_nonce ${asic}`, (err, out, errout) => {
    // if (err) return res.status(500).send(errout || err.message);
    // res.send(out);
  // });
// });

// /* ----------------------------------------------
   // Hash Board Status
// ----------------------------------------------- */
// app.post('/run/check', (req, res) => {
  // exec(`${CHECK_BIN} ${req.body.args || ''}`, (err, out, errout) => {
    // if (err) return res.status(500).send(errout || err.message);
    // res.send(out);
  // });
// });

/* ----------------------------------------------
   Mining Operation
   Use spawn to keep process running and log output
----------------------------------------------- */
let miningProcess = null;
let miningLog = '';
let currentMiningRunId = 0; 

// Maximum log size before clearing old content completely
const MAX_LOG_CHARS = 2_000_000; // ~2 MB

function appendMiningLog(s) {
  miningLog += s;

  // If the log exceeds threshold, clear it completely
  // instead of trimming old tail segments.
  // This avoids partial/fragmented log display and preserves consistency.
  if (miningLog.length > MAX_LOG_CHARS) {
    miningLog = '';
    miningLog += '[LOG CLEARED - size limit exceeded]\n';
  }
}

app.post('/run/mining_start', (req, res) => {
  const { pool, worker, pass } = req.body || {};
  if (!pool || !worker) return res.status(400).send('Missing pool or worker');

  // Gracefully stop the previous process if running
  if (miningProcess) {
    miningProcess.kill('SIGINT');
    appendMiningLog('\n[Stopping previous mining run... SIGINT sent]\n');
  }

  miningLog = '';
  const runId = ++currentMiningRunId;

  const args = ['-o', pool, '-u', worker, '-p', pass || 'x'];
  console.log(`[DEBUG] Mining start (run #${runId}): ${MINING_BIN} ${args.join(' ')}`);

  const child = spawn(MINING_BIN, args, {
    cwd: path.dirname(MINING_BIN)
  });
  miningProcess = child;

  appendMiningLog(`[Spawned Mining Run #${runId}] ${MINING_BIN} ${args.join(' ')}\n`);

  child.stdout.on('data', (data) => {
    if (runId !== currentMiningRunId) return;  // Ignore stale process logs
    appendMiningLog(data.toString());
  });

  child.stderr.on('data', (data) => {
    if (runId !== currentMiningRunId) return;
    appendMiningLog('[ERR] ' + data.toString());
  });

  child.on('error', (err) => {
    if (runId !== currentMiningRunId) return;
    appendMiningLog(`[ERROR] ${err.message}\n`);
  });

  child.on('exit', (code, signal) => {
    if (runId !== currentMiningRunId) return;
    appendMiningLog(`\n[Mining process exit | code=${code} | signal=${signal || 'none'}]`);
  });

  child.on('close', (code, signal) => {
    if (runId !== currentMiningRunId) return;
    appendMiningLog(`\n[Mining process closed | code=${code} | signal=${signal || 'none'}]`);
    miningProcess = null;
  });

  res.send('Mining started.');
});

app.post('/run/mining_stop', (req, res) => {
  if (miningProcess) {
    // Do NOT set miningProcess = null here.
    // Let the `close` handler handle cleanup properly.
    miningProcess.kill('SIGINT');
    appendMiningLog('\n[Stop requested by user]\n');
    return res.send('Mining stopping...');
  }
  res.send('No mining process running.');
});

app.get('/run/mining_log', (req, res) => {
  res.set('Content-Type', 'text/plain; charset=utf-8');
  res.set('Cache-Control', 'no-store, no-cache, must-revalidate, proxy-revalidate');
  res.send(miningLog || 'No activity yet.');
});

/* ----------------------------------------------
   Hash Board Verification
----------------------------------------------- */
let verificationProcess = null;
let verificationLog = '';
let currentRunId = 0;  

app.post('/api/verification/start', (req, res) => {
  const block = String(req.body.block || '').trim();
  if (!block) return res.status(400).send('Missing block number');

  if (verificationProcess) {
    verificationProcess.kill('SIGINT');
    verificationLog += '\n[Stopping previous run...]\n';
  }

  const runId = ++currentRunId;
  verificationLog = '';

  verificationProcess = spawn(VERIFY_BIN, ['-n', block], {
    cwd: path.dirname(VERIFY_BIN)
  });

  verificationLog += `[Spawned Run #${runId}] verification -n ${block}\n`;

  verificationProcess.stdout.on('data', data => {
    if (runId !== currentRunId) return;  
    verificationLog += data.toString();
  });

  verificationProcess.stderr.on('data', data => {
    if (runId !== currentRunId) return;
    verificationLog += '[ERR] ' + data.toString();
  });

  verificationProcess.on('close', (code, signal) => {
    if (runId !== currentRunId) return;
    verificationLog += `\n[Verification process exited | code=${code} | signal=${signal || 'none'}]`;
    verificationProcess = null;
  });

  res.send(`Verification started for block ${block}`);
});

app.post('/api/verification/stop', (req, res) => {
  if (verificationProcess) {
    verificationProcess.kill('SIGINT');   
    verificationLog += '\n[Stop requested by user]\n';
    return res.send('Verification stopping...');
  }
  res.send('No verification process running.');
});

app.get('/api/verification/log', (req, res) => {
  res.set('Content-Type', 'text/plain; charset=utf-8');
  res.set('Cache-Control', 'no-store, no-cache, must-revalidate, proxy-revalidate');
  res.send(verificationLog || 'No output yet.');
});

/* ----------------------------------------------
   SPA Fallback
----------------------------------------------- */
app.get(/(.*)/, (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

/* ----------------------------------------------
   Start server
----------------------------------------------- */
app.listen(PORT, HOST, () => {
  console.log('LENZO Miner UI running');
  console.log(`Local: http://localhost:${PORT}`);
  console.log(`LAN:   http://${LOCAL_IP}:${PORT}`);
});
