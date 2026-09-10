#!/usr/bin/env node
// Real server + a loopback Foundry receiver for interactive physical keypad tests.
const fs = require("node:fs");
const path = require("node:path");
const readline = require("node:readline");

const [
  serverPath,
  statePath,
  deviceHost,
  deviceId = "hwtest-keypad",
  targetVersion,
] = process.argv.slice(2);
if (!serverPath || !statePath || !deviceHost) {
  console.error(
    "Usage: node scripts/hwtest-server.cjs SERVER_REPO STATE_DIR DEVICE_HOST [DEVICE_ID] [TARGET_VERSION]",
  );
  process.exit(2);
}
const serverRepo = path.resolve(serverPath);
const stateDir = path.resolve(statePath);
fs.mkdirSync(stateDir, { recursive: true, mode: 0o700 });
process.env.MINDFLAYER_DATA_DIR = path.join(stateDir, "data");
process.env.MINDFLAYER_FIRMWARE_DIR = path.join(stateDir, "firmware");
const { DeviceStore } = require(
  path.join(serverRepo, "src/security/device-store"),
);
const store = new DeviceStore(
  path.join(process.env.MINDFLAYER_DATA_DIR, "devices.json"),
);
if (!store.get(deviceId)) store.provision(deviceId);
const { FirmwareRepository, VERSION } = require(
  path.join(serverRepo, "src/firmware/repository"),
);
const firmware = new FirmwareRepository(process.env.MINDFLAYER_FIRMWARE_DIR);
if (targetVersion) {
  if (
    !VERSION.test(targetVersion) ||
    !firmware.get("mindflayer-keypad-v1", targetVersion)
  ) {
    throw new Error(
      "Target version must have a valid artifact in the test firmware repository",
    );
  }
  // Test-only rollout selection; do not change persistent deployment settings.
  store.get(deviceId).targetVersion = targetVersion;
}
const { startAll } = require(path.join(serverRepo, "src/index"));
const WebSocket = require(path.join(serverRepo, "node_modules/ws"));
const runtime = startAll({
  // Bench sessions install only firmware explicitly selected by the operator.
  autoFirmwareUpdates: false,
  host: "127.0.0.1",
  deviceHost,
  foundryPort: 8080,
  devicePort: 10443,
  deviceStore: store,
  firmwareRepository: firmware,
});
const devices = new Map();
for (const id of Object.keys(store.devices))
  devices.set(id, { connected: false, counts: {} });
let sequence = [];
let receiver;
function record(event) {
  const entry = { time: new Date().toISOString(), ...event };
  const line = JSON.stringify(entry);
  fs.appendFileSync(path.join(stateDir, "events.jsonl"), line + "\n", {
    mode: 0o600,
  });
  console.log(line);
}
if (targetVersion) record({ test: "ota-target", deviceId, targetVersion });
runtime.device.server.on("request", (request, response) => {
  if (!request.url.startsWith("/firmware/")) return;
  response.once("finish", () =>
    record({
      test: "firmware-response",
      path: request.url.split("?")[0],
      status: response.statusCode,
      bytes: response.getHeader("Content-Length"),
    }),
  );
});
function colors(label, led1, led2, id = deviceId) {
  if (!devices.get(id)?.connected || receiver?.readyState !== WebSocket.OPEN) {
    record({
      test: "led-skipped",
      deviceId: id,
      label,
      reason: "keypad not registered",
    });
    return;
  }
  receiver.send(
    JSON.stringify({
      type: "configuration",
      "controller-id": id,
      led1,
      led2,
    }),
  );
  record({ test: "led-command-sent", deviceId: id, label, led1, led2 });
}
const rgb = (r = 0, g = 0, b = 0) => ({ r, g, b });
function cancelSequence() {
  sequence.forEach(clearTimeout);
  sequence = [];
}
function ledSequence(id = deviceId) {
  cancelSequence();
  const steps = [
    ["LED 1 red; LED 2 off", rgb(64), rgb()],
    ["LED 1 green; LED 2 off", rgb(0, 64), rgb()],
    ["LED 1 blue; LED 2 off", rgb(0, 0, 64), rgb()],
    ["LED 1 off; LED 2 red", rgb(), rgb(64)],
    ["LED 1 off; LED 2 green", rgb(), rgb(0, 64)],
    ["LED 1 off; LED 2 blue", rgb(), rgb(0, 0, 64)],
    ["Both white", rgb(32, 32, 32), rgb(32, 32, 32)],
    ["Both off", rgb(), rgb()],
    ["Ready for buttons: both green", rgb(0, 16), rgb(0, 16)],
  ];
  steps.forEach((step, index) => {
    sequence.push(setTimeout(() => colors(...step, id), index * 2500));
  });
}
runtime.foundry.server.once("listening", () => {
  receiver = new WebSocket("ws://127.0.0.1:8080/ws");
  receiver.on("open", () => {
    receiver.send(
      JSON.stringify({
        type: "registration",
        "controller-id": "hwtest-receiver",
        receiver: true,
        players: [],
      }),
    );
    record({ test: "receiver-ready", deviceId });
  });
  receiver.on("message", (data) => {
    const message = JSON.parse(data);
    record({ test: "foundry-received", message });
    const device = devices.get(message["controller-id"]);
    if (!device) return;
    if (message.type === "registration") {
      device.connected = message.status === "connected";
    }
    if (message.type === "key-event") {
      const key = `${message.key}:${message.state}`;
      device.counts[key] = (device.counts[key] || 0) + 1;
    }
  });
  receiver.on("error", (error) =>
    record({ test: "receiver-error", error: error.message }),
  );
});
let closing = false;
function close() {
  if (closing) return;
  closing = true;
  cancelSequence();
  for (const id of devices.keys())
    colors("Test shutdown: both off", rgb(), rgb(), id);
  record({ test: "summary", devices: Object.fromEntries(devices) });
  setTimeout(() => {
    receiver?.terminate();
    for (const endpoint of [runtime.foundry, runtime.device]) {
      endpoint.wss.clients.forEach((client) => client.terminate());
      endpoint.wss.close();
      endpoint.server.close();
    }
    runtime.foundry.registry.close();
    process.exit(0);
  }, 250);
}
readline.createInterface({ input: process.stdin }).on("line", (line) => {
  const [command, id = deviceId] = line.trim().split(/\s+/);
  switch (command) {
    case "leds":
      ledSequence(id);
      break;
    case "off":
      cancelSequence();
      colors("Both off", rgb(), rgb(), id);
      break;
    case "status":
      record({ test: "summary", devices: Object.fromEntries(devices) });
      break;
    case "quit":
      close();
      break;
    default:
      console.log("Commands: leds [device-id], off [device-id], status, quit");
  }
});
process.on("SIGINT", close);
process.on("SIGTERM", close);
console.log(
  "Commands: leds [device-id], off [device-id], status, quit. LED output needs visual confirmation.",
);
