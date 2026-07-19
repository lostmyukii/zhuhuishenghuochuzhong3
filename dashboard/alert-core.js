(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  }
  root.SmartLifeAlertCore = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
  "use strict";

  const DEFINITIONS = {
    mq2: { room: "kitchen", roomLabel: "厨房附区", sensor: "MQ2", gpio: "GPIO2" },
    flame: { room: "kitchen", roomLabel: "厨房附区", sensor: "火焰传感器", gpio: "GPIO45" },
    water: { room: "bath", roomLabel: "卫生间", sensor: "水浸传感器", gpio: "GPIO8" },
    intrusion: { room: "entry", roomLabel: "入户附区", sensor: "PIR", gpio: "GPIO5" },
  };

  function canonicalCode(value) {
    const code = String(value);
    return code === "gas" ? "mq2" : code;
  }

  function normalizeCodes(codes) {
    if (!Array.isArray(codes)) {
      return [];
    }
    const seen = new Set();
    const normalized = [];
    for (const value of codes) {
      const code = canonicalCode(value);
      if (!seen.has(code)) {
        seen.add(code);
        normalized.push(code);
      }
    }
    return normalized;
  }

  function rgbLabel(value) {
    if (value === "red") return "红";
    if (value === "yellow") return "黄";
    return "关闭";
  }

  function buzzerLabel(telemetry) {
    const actuators = telemetry.actuators || {};
    const health = telemetry.health || {};
    if (actuators.buzzer === true) {
      return "蜂鸣器正在报警";
    }
    if (health.buzzerEnabled === false) {
      return "安全蜂鸣器已静音";
    }
    return "蜂鸣器当前未响";
  }

  function actualHomeRiskAction(telemetry) {
    const actuators = telemetry.actuators || {};
    const fan = Number.isFinite(actuators.fanPercent) ? `${actuators.fanPercent}%` : "状态未知";
    const relay = typeof actuators.relay === "boolean" ? (actuators.relay ? "开" : "关") : "未知";
    return `风扇 ${fan}、RGB ${rgbLabel(actuators.rgb)}、低压灯 GPIO 目标为${relay}、${buzzerLabel(telemetry)}`;
  }

  function actualIntrusionAction(telemetry) {
    const actuators = telemetry.actuators || {};
    return `RGB ${rgbLabel(actuators.rgb)}、${buzzerLabel(telemetry)}`;
  }

  function reasonFor(code, telemetry) {
    const sensors = telemetry.sensors || {};
    const thresholds = telemetry.thresholds || {};
    if (code === "mq2") {
      const value = Number.isFinite(sensors.airQualityEqPpm)
        ? Math.round(sensors.airQualityEqPpm)
        : "--";
      const threshold = Number.isFinite(thresholds.mq2EqPpm)
        ? Math.round(thresholds.mq2EqPpm)
        : "--";
      return `MQ2 等效估算 ${value} ppm，超过当前安全阈值 ${threshold} ppm`;
    }
    if (code === "flame") {
      return "火焰传感器检测到触发信号";
    }
    if (code === "water") {
      return "水浸传感器检测到漏水";
    }
    if (code === "intrusion") {
      return "布防状态下 PIR 检测到人员活动；PIR 不识别具体人员";
    }
    return `设备上报异常：${code}`;
  }

  function describeAlerts(telemetry) {
    if (!telemetry || !Array.isArray(telemetry.alerts)) {
      return [];
    }
    return normalizeCodes(telemetry.alerts).map((code) => {
      const definition = DEFINITIONS[code];
      if (!definition) {
        return {
          code,
          room: "unknown",
          roomLabel: "未知区域",
          sensor: "未知来源",
          gpio: "未上报",
          reason: reasonFor(code, telemetry),
          action: "请查看开发板上报与现场设备状态",
        };
      }
      return {
        code,
        ...definition,
        reason: reasonFor(code, telemetry),
        action: code === "intrusion"
          ? actualIntrusionAction(telemetry)
          : actualHomeRiskAction(telemetry),
      };
    });
  }

  function alertRooms(telemetry) {
    return describeAlerts(telemetry).map((item) => item.room);
  }

  return {
    DEFINITIONS,
    normalizeCodes,
    describeAlerts,
    alertRooms,
  };
});
