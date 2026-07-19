const assert = require("assert");
const Core = require("../protocol-core.js");

const tests = [];
const test = (name, fn) => tests.push({ name, fn });

function hello(overrides = {}) {
  return {
    type: "hello",
    protocolVersion: 1,
    project: "smartlife-junior",
    profileId: "smartlife-junior-solar-home-v1",
    board: "n16r8_esp32s3",
    baud: 115200,
    rfid: false,
    ...overrides,
  };
}

function telemetry(overrides = {}) {
  return {
    type: "telemetry",
    seq: 1,
    uptimeMs: 1000,
    mode: "Auto",
    solarTerm: "小暑",
    sensors: {
      temperatureC: 27.4,
      humidityRh: 61,
      airQualityEqPpm: 386,
      soundRelative: 63,
      presence: true,
      lightRelative: 44,
      water: false,
      flame: false,
      keypadRaw: 4095,
      knobRaw: 2048,
    },
    thresholds: { temperatureC: 27, mq2EqPpm: 600 },
    actuators: {
      fanPercent: 100,
      curtainClosePercent: 80,
      curtainControlEnabled: true,
      relay: false,
      buzzer: false,
      rgb: "yellow",
    },
    alerts: [],
    health: { buzzerEnabled: true },
    lastAppliedCommandId: null,
    ...overrides,
  };
}

test("classifies protocol frames and rejects bad JSON without changing state", () => {
  const state = Core.createSession();
  assert.equal(Core.classifyFrame(hello()), "hello");
  assert.equal(Core.classifyFrame(telemetry()), "telemetry");
  assert.equal(Core.classifyFrame({ type: "event" }), "event");
  assert.equal(Core.classifyFrame({ type: "ack" }), "ack");
  assert.equal(Core.classifyFrame({ type: "mystery" }), "unknown");

  const result = Core.ingestLine(state, "not-json", 1000);
  assert.strictEqual(result.state, state);
  assert.equal(result.accepted, false);
  assert.equal(result.reason, "invalid_json");
});

test("requires the exact hello identity before accepting telemetry", () => {
  const initial = Core.createSession();
  const early = Core.ingestLine(initial, JSON.stringify(telemetry()), 1000);
  assert.equal(early.accepted, false);
  assert.equal(early.reason, "awaiting_identity");

  const wrong = Core.ingestLine(
    initial,
    JSON.stringify(hello({ profileId: "smartlife-junior-home-v1" })),
    1100,
  );
  assert.equal(wrong.accepted, false);
  assert.equal(wrong.reason, "wrong_profile");
  assert.equal(wrong.state.identityLocked, false);

  const locked = Core.ingestLine(initial, JSON.stringify(hello()), 1200);
  assert.equal(locked.accepted, true);
  assert.equal(locked.state.identityLocked, true);
  assert.equal(locked.state.dataSource, "real");

  const live = Core.ingestLine(locked.state, JSON.stringify(telemetry()), 1300);
  assert.equal(live.accepted, true);
  assert.equal(live.state.telemetry.seq, 1);
});

test("marks mock identity explicitly and never upgrades it to real", () => {
  const initial = Core.createSession();
  const locked = Core.ingestLine(
    initial,
    JSON.stringify(hello({ source: "mock" })),
    100,
  );
  const live = Core.ingestLine(
    locked.state,
    JSON.stringify(telemetry({ source: "mock" })),
    200,
  );
  assert.equal(live.state.dataSource, "mock");
  assert.equal(Core.connectionState(live.state, true, 300).code, "mock");
  assert.match(Core.connectionState(live.state, true, 300).label, /模拟/);
});

test("expires fresh data after 3500ms and clears live values", () => {
  const locked = Core.ingestLine(Core.createSession(), JSON.stringify(hello()), 1000);
  const live = Core.ingestLine(locked.state, JSON.stringify(telemetry()), 1500);

  assert.equal(Core.isFresh(live.state, 5000), true);
  assert.equal(Core.isFresh(live.state, 5001), false);
  assert.equal(Core.liveValues(live.state, 5000).temperature, "27.4℃");
  const stale = Core.liveValues(live.state, 5001);
  assert.equal(stale.temperature, "--");
  assert.equal(stale.airQuality, "--");
  assert.equal(stale.presence, "等待实时数据");
  assert.equal(Core.connectionState(live.state, true, 5001).code, "offline");
});

test("command completion requires both ack and a matching new telemetry", () => {
  const command = Core.buildCommand("mode", "Sleep", "cmd-001");
  let transaction = Core.createTransaction(command, 1000, 8);
  assert.equal(transaction.status, "waiting_ack");

  transaction = Core.reduceTransaction(
    transaction,
    { type: "ack", id: "cmd-001", ok: true, applied: { mode: "Sleep" } },
    1200,
  );
  assert.equal(transaction.status, "accepted_waiting_telemetry");
  assert.match(Core.transactionLabel(transaction), /等待状态更新/);

  transaction = Core.reduceTransaction(
    transaction,
    telemetry({ seq: 9, mode: "Sleep", lastAppliedCommandId: "cmd-001" }),
    1300,
  );
  assert.equal(transaction.status, "complete");
  assert.match(Core.transactionLabel(transaction), /状态已更新/);
});

test("rejected, deferred and timed-out commands stay truthful", () => {
  const rejectedBase = Core.createTransaction(
    Core.buildCommand("mode", "Sleep", "cmd-reject"),
    0,
    0,
  );
  const rejected = Core.reduceTransaction(
    rejectedBase,
    { type: "ack", id: "cmd-reject", ok: false, error: "safety_lock" },
    10,
  );
  assert.equal(rejected.status, "rejected");
  assert.match(Core.transactionLabel(rejected), /安全告警/);

  const deferredBase = Core.createTransaction(
    Core.buildCommand("mode", "Sleep", "cmd-deferred"),
    0,
    0,
  );
  const deferred = Core.reduceTransaction(
    deferredBase,
    { type: "ack", id: "cmd-deferred", ok: true, deferredBy: "safety" },
    10,
  );
  assert.equal(deferred.status, "accepted_waiting_telemetry");
  assert.match(Core.transactionLabel(deferred), /安全状态解除后生效/);

  const deferredComplete = Core.reduceTransaction(
    deferred,
    telemetry({ seq: 1, mode: "Sleep", lastAppliedCommandId: "cmd-deferred" }),
    20,
  );
  assert.equal(deferredComplete.status, "complete");
  assert.match(Core.transactionLabel(deferredComplete), /P1 安全联动继续/);

  const timeout = Core.expireTransaction(deferredBase, 5001);
  assert.equal(timeout.status, "timeout");
  assert.match(Core.transactionLabel(timeout), /超时/);
});

test("builds only the current dashboard command shapes", () => {
  assert.deepEqual(Core.buildCommand("mode", "Auto", "mode-1"), {
    type: "command",
    id: "mode-1",
    origin: "dashboard",
    mode: "Auto",
  });
  assert.deepEqual(Core.buildCommand("solarTerm", "大寒", "term-1"), {
    type: "command",
    id: "term-1",
    origin: "dashboard",
    set: { solarTerm: "大寒" },
  });
  assert.deepEqual(Core.buildCommand("guardArmed", true, "guard-1"), {
    type: "command",
    id: "guard-1",
    origin: "dashboard",
    set: { guardArmed: true },
  });
  assert.throws(() => Core.buildCommand("mode", "Away", "bad"));
  assert.throws(() => Core.buildCommand("solarTerm", "不存在", "bad"));
});

test("maps participating rooms without inventing safety alerts", () => {
  assert.deepEqual(Core.modeActiveRooms("Sleep"), ["master", "living"]);
  assert.deepEqual(Core.modeActiveRooms("Auto"), [
    "master",
    "study",
    "living",
    "dining",
    "balcony",
  ]);
});

test("keeps the approved Xiaoshu and Dahan strategy values", () => {
  assert.deepEqual(Core.solarTermProfile("小暑"), {
    temperatureC: 26,
    curtainClosePercent: 80,
    lightThreshold: 30,
  });
  assert.deepEqual(Core.solarTermProfile("大寒"), {
    temperatureC: 24,
    curtainClosePercent: 20,
    lightThreshold: 45,
  });
  assert.equal(Core.solarTermProfile("不存在"), null);
});

let failures = 0;
for (const { name, fn } of tests) {
  try {
    fn();
    console.log(`✓ ${name}`);
  } catch (error) {
    failures += 1;
    console.error(`✗ ${name}`);
    console.error(error.stack || error);
  }
}

if (failures > 0) {
  process.exitCode = 1;
} else {
  console.log(`${tests.length} protocol-core tests passed`);
}
