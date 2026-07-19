const assert = require("assert");
const Alerts = require("../alert-core.js");

const tests = [];
const test = (name, fn) => tests.push({ name, fn });

function telemetry(overrides = {}) {
  return {
    sensors: {
      airQualityEqPpm: 720,
      water: false,
      flame: false,
      presence: true,
    },
    thresholds: { mq2EqPpm: 600 },
    actuators: {
      fanPercent: 100,
      relay: false,
      buzzer: true,
      rgb: "red",
    },
    health: { buzzerEnabled: true },
    alerts: [],
    ...overrides,
  };
}

test("uses telemetry.alerts as the only alert truth", () => {
  const descriptions = Alerts.describeAlerts(
    telemetry({ sensors: { presence: true, water: true, airQualityEqPpm: 900 } }),
  );
  assert.deepEqual(descriptions, []);
});

test("describes MQ2 value, threshold, GPIO and actual actions", () => {
  const [alert] = Alerts.describeAlerts(telemetry({ alerts: ["mq2"] }));
  assert.equal(alert.code, "mq2");
  assert.equal(alert.room, "kitchen");
  assert.equal(alert.gpio, "GPIO2");
  assert.match(alert.reason, /720/);
  assert.match(alert.reason, /600/);
  assert.match(alert.action, /风扇 100%/);
  assert.match(alert.action, /蜂鸣器正在报警/);
  assert.match(alert.action, /低压灯 GPIO 目标为关/);
});

test("normalizes gas and removes duplicates while preserving order", () => {
  const descriptions = Alerts.describeAlerts(
    telemetry({ alerts: ["water", "gas", "mq2", "water", "flame"] }),
  );
  assert.deepEqual(
    descriptions.map((item) => item.code),
    ["water", "mq2", "flame"],
  );
});

test("intrusion never invents fan or relay actions", () => {
  const [alert] = Alerts.describeAlerts(telemetry({ alerts: ["intrusion"] }));
  assert.equal(alert.room, "entry");
  assert.equal(alert.gpio, "GPIO5");
  assert.doesNotMatch(alert.action, /风扇/);
  assert.doesNotMatch(alert.action, /低压灯/);
  assert.match(alert.action, /RGB 红/);
});

test("explicit buzzer mute is reported instead of sounding", () => {
  const [alert] = Alerts.describeAlerts(
    telemetry({
      alerts: ["water"],
      actuators: { fanPercent: 100, relay: false, buzzer: false, rgb: "red" },
      health: { buzzerEnabled: false },
    }),
  );
  assert.match(alert.action, /安全蜂鸣器已静音/);
  assert.doesNotMatch(alert.action, /正在报警/);
});

test("keeps unknown codes visible and safe for text rendering", () => {
  const [alert] = Alerts.describeAlerts(
    telemetry({ alerts: ["<img src=x onerror=alert(1)>"] }),
  );
  assert.equal(alert.room, "unknown");
  assert.match(alert.reason, /设备上报异常/);
  assert.match(alert.reason, /<img/);
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
  console.log(`${tests.length} alert-core tests passed`);
}
