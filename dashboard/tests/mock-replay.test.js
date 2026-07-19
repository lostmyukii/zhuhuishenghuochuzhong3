const assert = require("assert");
const fs = require("fs");
const path = require("path");
const Core = require("../protocol-core.js");
const Alerts = require("../alert-core.js");

const evidencePath = path.resolve(
  __dirname,
  "../../docs/evidence/mock/task7-water-demo-20260719.jsonl",
);
const frames = fs
  .readFileSync(evidencePath, "utf8")
  .trim()
  .split("\n")
  .map((line) => JSON.parse(line));

let session = Core.createSession();
let timestamp = 1000;
let sawWaterAlert = false;
let sawP1Actions = false;

for (const frame of frames) {
  const result = Core.ingestLine(session, JSON.stringify(frame), timestamp);
  assert.equal(result.accepted, true, `${frame.type} should be accepted`);
  session = result.state;
  if (frame.type === "telemetry" && frame.alerts.includes("water")) {
    sawWaterAlert = Alerts.describeAlerts(frame).some(
      (item) => item.room === "bath" && item.gpio === "GPIO8",
    );
    sawP1Actions =
      frame.actuators.fanPercent === 100 &&
      frame.actuators.relay === false &&
      frame.actuators.buzzer === true &&
      frame.actuators.rgb === "red";
  }
  timestamp += 100;
}

assert.equal(session.dataSource, "mock");
assert.equal(session.telemetry.seq, 7);
assert.deepEqual(session.telemetry.alerts, []);
assert.equal(sawWaterAlert, true);
assert.equal(sawP1Actions, true);
assert.equal(Core.connectionState(session, true, timestamp).code, "mock");
assert.equal(Core.connectionState(session, true, session.lastFrameAt + 3501).code, "offline");

console.log(`mock replay passed: ${frames.length} frames, water P1 and recovery observed`);
