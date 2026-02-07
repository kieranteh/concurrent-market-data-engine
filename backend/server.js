const express = require('express');
const http = require('http');
const { Server } = require("socket.io");
const { spawn } = require('child_process');
const cors = require('cors');
const path = require('path');

const app = express();
app.use(cors());

const server = http.createServer(app);
const io = new Server(server, {
  cors: {
    origin: "http://localhost:3000",
    methods: ["GET", "POST"]
  }
});

// 1. Locate the C++ Executable
// We assume the structure is: root/backend/server.js and root/engine/build/market_data_engine.exe
const exePath = path.join(__dirname, '../engine/build/market_data_engine.exe');

console.log(`Target Executable: ${exePath}`);

// 2. Spawn the C++ Process
let childProcess = null;

const startEngine = () => {
  if (childProcess) return;

  console.log("Starting C++ Engine...");
  childProcess = spawn(exePath);

  // Stop the engine automatically after 10 seconds
  setTimeout(() => {
    if (childProcess) {
      console.log("10 seconds elapsed. Stopping engine...");
      stopEngine();
    }
  }, 10000);

  childProcess.stdout.on('data', (data) => {
    const output = data.toString();
    const lines = output.split('\n');
    
    lines.forEach(line => {
      const trimmed = line.trim();
      if (!trimmed) return;

      try {
        // 3. Parse JSON from C++ and send to Frontend via WebSocket
        const jsonData = JSON.parse(trimmed);
        io.emit('stats', jsonData);
      } catch (e) {
        console.log(`[CPP]: ${trimmed}`);
      }
    });
  });

  childProcess.stderr.on('data', (data) => {
    console.error(`[CPP Error]: ${data}`);
  });

  childProcess.on('close', (code) => {
    console.log(`C++ Engine exited with code ${code}`);
    childProcess = null;
  });
};

const stopEngine = () => {
  if (childProcess) {
    console.log("Stopping C++ Engine...");
    childProcess.kill();
    childProcess = null;
  }
};

startEngine();

io.on('connection', (socket) => {
  socket.on('restart_engine', () => {
    console.log("Restart command received from frontend.");
    stopEngine();
    startEngine();
  });
});

const PORT = 4000;
server.listen(PORT, () => {
  console.log(`Backend running on http://localhost:${PORT}`);
});
