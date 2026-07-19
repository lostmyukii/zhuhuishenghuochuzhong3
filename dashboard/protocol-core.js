(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  }
  root.SmartLifeProtocolCore = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
  "use strict";

  const TARGET_PROJECT = "smartlife-junior";
  const TARGET_PROFILE = "smartlife-junior-solar-home-v1";
  const OFFLINE_AFTER_MS = 3500;
  const COMMAND_TIMEOUT_MS = 5000;
  const SOLAR_TERMS = [
    "立春", "雨水", "惊蛰", "春分", "清明", "谷雨",
    "立夏", "小满", "芒种", "夏至", "小暑", "大暑",
    "立秋", "处暑", "白露", "秋分", "寒露", "霜降",
    "立冬", "小雪", "大雪", "冬至", "小寒", "大寒",
  ];
  const SOLAR_PROFILES = {
    "立春": [26, 20, 35], "雨水": [26, 20, 35],
    "惊蛰": [26, 50, 35], "春分": [26, 50, 35],
    "清明": [26, 50, 35], "谷雨": [26, 50, 35],
    "立夏": [27, 80, 35], "小满": [27, 80, 35],
    "芒种": [27, 80, 35], "夏至": [27, 80, 30],
    "小暑": [26, 80, 30], "大暑": [26, 80, 30],
    "立秋": [27, 80, 35], "处暑": [27, 80, 35],
    "白露": [26, 50, 35], "秋分": [26, 50, 35],
    "寒露": [25, 20, 40], "霜降": [25, 20, 40],
    "立冬": [24, 20, 45], "小雪": [24, 20, 45],
    "大雪": [24, 20, 45], "冬至": [24, 20, 45],
    "小寒": [24, 20, 45], "大寒": [24, 20, 45],
  };

  function createSession() {
    return {
      identityLocked: false,
      identity: null,
      dataSource: null,
      hello: null,
      telemetry: null,
      lastEvent: null,
      lastAck: null,
      lastFrameAt: null,
      acceptedFrames: 0,
    };
  }

  function isObject(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
  }

  function classifyFrame(frame) {
    if (!isObject(frame)) {
      return "unknown";
    }
    return ["hello", "telemetry", "event", "ack"].includes(frame.type)
      ? frame.type
      : "unknown";
  }

  function parseLine(line) {
    if (typeof line !== "string" || line.trim() === "") {
      return { ok: false, reason: "invalid_json" };
    }
    try {
      const frame = JSON.parse(line);
      if (!isObject(frame)) {
        return { ok: false, reason: "invalid_json" };
      }
      return { ok: true, frame };
    } catch (_error) {
      return { ok: false, reason: "invalid_json" };
    }
  }

  function targetHello(frame) {
    return frame.type === "hello" &&
      frame.project === TARGET_PROJECT &&
      frame.profileId === TARGET_PROFILE;
  }

  function frameSource(frame) {
    return frame.source === "mock" ? "mock" : "real";
  }

  function ingestLine(state, line, nowMs) {
    const parsed = parseLine(line);
    if (!parsed.ok) {
      return { state, accepted: false, reason: parsed.reason, frame: null };
    }
    return ingestFrame(state, parsed.frame, nowMs);
  }

  function ingestFrame(state, frame, nowMs) {
    const kind = classifyFrame(frame);
    if (kind === "unknown") {
      return { state, accepted: false, reason: "unsupported_frame", frame };
    }

    if (!state.identityLocked) {
      if (kind !== "hello") {
        return { state, accepted: false, reason: "awaiting_identity", frame };
      }
      if (!targetHello(frame)) {
        return { state, accepted: false, reason: "wrong_profile", frame };
      }
      const next = {
        ...state,
        identityLocked: true,
        identity: frame.profileId,
        dataSource: frameSource(frame),
        hello: frame,
        lastFrameAt: nowMs,
        acceptedFrames: state.acceptedFrames + 1,
      };
      return { state: next, accepted: true, reason: null, frame };
    }

    if (kind === "hello" && !targetHello(frame)) {
      return { state, accepted: false, reason: "wrong_profile", frame };
    }
    if (state.dataSource === "mock" && frame.source !== "mock") {
      return { state, accepted: false, reason: "mixed_source", frame };
    }
    if (state.dataSource === "real" && frame.source === "mock") {
      return { state, accepted: false, reason: "mixed_source", frame };
    }

    const next = { ...state, acceptedFrames: state.acceptedFrames + 1 };
    if (kind === "hello") {
      next.hello = frame;
      next.lastFrameAt = nowMs;
    } else if (kind === "telemetry") {
      next.telemetry = frame;
      next.lastFrameAt = nowMs;
    } else if (kind === "event") {
      next.lastEvent = frame;
    } else if (kind === "ack") {
      next.lastAck = frame;
    }
    return { state: next, accepted: true, reason: null, frame };
  }

  function isFresh(state, nowMs, staleMs = OFFLINE_AFTER_MS) {
    return state.lastFrameAt !== null && nowMs - state.lastFrameAt <= staleMs;
  }

  function connectionState(state, serialConnected, nowMs) {
    if (!serialConnected) {
      return { code: "disconnected", label: "串口未连接" };
    }
    if (!state.identityLocked) {
      return { code: "awaiting", label: "串口已连接，等待开发板" };
    }
    if (!isFresh(state, nowMs)) {
      return { code: "offline", label: "开发板离线" };
    }
    if (state.dataSource === "mock") {
      return { code: "mock", label: "模拟主板" };
    }
    return { code: "real", label: "真板在线" };
  }

  function finiteNumber(value) {
    return typeof value === "number" && Number.isFinite(value);
  }

  function liveValues(state, nowMs) {
    if (!isFresh(state, nowMs) || !state.telemetry) {
      return {
        live: false,
        temperature: "--",
        humidity: "--",
        airQuality: "--",
        sound: "--",
        presence: "等待实时数据",
        light: "--",
        knob: "--",
        threshold: "--",
      };
    }
    const sensors = state.telemetry.sensors || {};
    const thresholds = state.telemetry.thresholds || {};
    return {
      live: true,
      temperature: finiteNumber(sensors.temperatureC) ? `${sensors.temperatureC.toFixed(1)}℃` : "--",
      humidity: finiteNumber(sensors.humidityRh) ? `${Math.round(sensors.humidityRh)}%RH` : "--",
      airQuality: finiteNumber(sensors.airQualityEqPpm) ? `${Math.round(sensors.airQualityEqPpm)} ppm` : "--",
      sound: finiteNumber(sensors.soundRelative) ? String(Math.round(sensors.soundRelative)) : "--",
      presence: typeof sensors.presence === "boolean"
        ? (sensors.presence ? "有人活动" : "未检测到活动")
        : "--",
      light: finiteNumber(sensors.lightRelative) ? String(Math.round(sensors.lightRelative)) : "--",
      knob: finiteNumber(sensors.knobRaw) ? String(Math.round(sensors.knobRaw)) : "--",
      threshold: finiteNumber(thresholds.temperatureC) ? `${Math.round(thresholds.temperatureC)}℃` : "--",
    };
  }

  function buildCommand(kind, value, id) {
    if (typeof id !== "string" || id.length === 0) {
      throw new Error("command id is required");
    }
    const command = { type: "command", id, origin: "dashboard" };
    if (kind === "mode") {
      if (!['Auto', 'Sleep'].includes(value)) {
        throw new Error("unsupported mode");
      }
      command.mode = value;
      return command;
    }
    if (kind === "solarTerm") {
      if (!SOLAR_TERMS.includes(value)) {
        throw new Error("unsupported solar term");
      }
      command.set = { solarTerm: value };
      return command;
    }
    if (kind === "guardArmed" || kind === "buzzerEnabled") {
      if (typeof value !== "boolean") {
        throw new Error(`${kind} must be boolean`);
      }
      command.set = { [kind]: value };
      return command;
    }
    throw new Error("unsupported command kind");
  }

  function createTransaction(command, sentAt, startSeq) {
    return {
      id: command.id,
      command,
      sentAt,
      startSeq: Number.isFinite(startSeq) ? startSeq : 0,
      status: "waiting_ack",
      ack: null,
      telemetry: null,
      updatedAt: sentAt,
    };
  }

  function reduceTransaction(transaction, frame, nowMs) {
    if (!transaction || ["complete", "rejected", "timeout"].includes(transaction.status)) {
      return transaction;
    }
    let next = { ...transaction };
    if (frame.type === "ack" && frame.id === transaction.id) {
      next.ack = frame;
      next.updatedAt = nowMs;
      if (!frame.ok) {
        next.status = "rejected";
        return next;
      }
    }
    if (
      frame.type === "telemetry" &&
      frame.lastAppliedCommandId === transaction.id &&
      Number(frame.seq) > transaction.startSeq
    ) {
      next.telemetry = frame;
      next.updatedAt = nowMs;
    }
    if (next.ack && next.ack.ok && next.telemetry) {
      next.status = "complete";
    } else if (next.ack && next.ack.ok) {
      next.status = "accepted_waiting_telemetry";
    } else {
      next.status = "waiting_ack";
    }
    return next;
  }

  function expireTransaction(transaction, nowMs, timeoutMs = COMMAND_TIMEOUT_MS) {
    if (!transaction || ["complete", "rejected", "timeout"].includes(transaction.status)) {
      return transaction;
    }
    if (nowMs - transaction.sentAt > timeoutMs) {
      return { ...transaction, status: "timeout", updatedAt: nowMs };
    }
    return transaction;
  }

  function transactionLabel(transaction) {
    if (!transaction) {
      return "尚未发送操作";
    }
    if (transaction.status === "waiting_ack") {
      return "等待开发板接受";
    }
    if (transaction.status === "accepted_waiting_telemetry") {
      if (transaction.ack && transaction.ack.deferredBy === "safety") {
        return "模式已记录，安全状态解除后生效；等待状态更新";
      }
      return "已接受，等待状态更新";
    }
    if (transaction.status === "complete") {
      if (transaction.ack && transaction.ack.deferredBy === "safety") {
        return "状态已更新：模式已记录，P1 安全联动继续";
      }
      return "状态已更新";
    }
    if (transaction.status === "rejected") {
      if (transaction.ack && transaction.ack.error === "safety_lock") {
        return "开发板拒绝：安全告警期间不能覆盖设备";
      }
      const error = transaction.ack && transaction.ack.error
        ? transaction.ack.error
        : "unknown";
      return `开发板拒绝：${error}`;
    }
    if (transaction.status === "timeout") {
      return "等待超时，没有形成状态闭环";
    }
    return "正在发送";
  }

  function modeActiveRooms(mode) {
    if (mode === "Sleep") {
      return ["master", "living"];
    }
    if (mode === "Auto") {
      return ["master", "study", "living", "dining", "balcony"];
    }
    return [];
  }

  function solarTermProfile(name) {
    const values = SOLAR_PROFILES[name];
    if (!values) {
      return null;
    }
    return {
      temperatureC: values[0],
      curtainClosePercent: values[1],
      lightThreshold: values[2],
    };
  }

  return {
    TARGET_PROJECT,
    TARGET_PROFILE,
    OFFLINE_AFTER_MS,
    COMMAND_TIMEOUT_MS,
    SOLAR_TERMS,
    createSession,
    classifyFrame,
    parseLine,
    ingestLine,
    ingestFrame,
    isFresh,
    connectionState,
    liveValues,
    buildCommand,
    createTransaction,
    reduceTransaction,
    expireTransaction,
    transactionLabel,
    modeActiveRooms,
    solarTermProfile,
  };
});
