(function () {
  "use strict";

  const Core = window.SmartLifeProtocolCore;
  const AlertCore = window.SmartLifeAlertCore;
  if (!Core || !AlertCore) {
    return;
  }

  const $ = (id) => document.getElementById(id);
  const refs = {
    connect: $("connect-serial"),
    disconnect: $("disconnect-serial"),
    serialStatus: $("serial-status"),
    boardStatus: $("board-status"),
    sourceStatus: $("source-status"),
    lastFrameTime: $("last-frame-time"),
    browserMessage: $("browser-message"),
    mockRibbon: $("mock-ribbon"),
    alertNotice: $("alert-notice"),
    alertKicker: $("alert-kicker"),
    alertTitle: $("alert-title"),
    alertSummary: $("alert-summary"),
    alertList: $("alert-list"),
    houseShell: $("house-shell"),
    houseMode: $("house-mode"),
    seasonGlyph: $("season-glyph"),
    seasonTitle: $("season-title"),
    seasonExplanation: $("season-explanation"),
    strategyTemperature: $("strategy-temperature"),
    strategyCurtain: $("strategy-curtain"),
    strategyLight: $("strategy-light"),
    liveDot: $("live-dot"),
    metricTemperature: $("metric-temperature"),
    metricHumidity: $("metric-humidity"),
    metricAir: $("metric-air"),
    metricSound: $("metric-sound"),
    metricPresence: $("metric-presence"),
    metricKnob: $("metric-knob"),
    metricThreshold: $("metric-threshold"),
    controlLock: $("control-lock"),
    solarSelect: $("solar-term-select"),
    applySolar: $("apply-solar-term"),
    buttonAEvent: $("button-a-event"),
    commandStatus: $("command-status"),
    deviceFan: $("device-fan"),
    deviceCurtain: $("device-curtain"),
    deviceRelay: $("device-relay"),
    deviceBuzzer: $("device-buzzer"),
    deviceRgb: $("device-rgb"),
    rgbIndicator: $("rgb-indicator"),
    runLog: $("run-log"),
    clearLog: $("clear-log"),
  };

  let session = Core.createSession();
  let serialPort = null;
  let serialReader = null;
  let serialLoopPromise = null;
  let serialConnected = false;
  let keepReading = false;
  let inputBuffer = "";
  let testTransport = null;
  let identityError = null;
  let pendingTransaction = null;
  let transportError = null;
  let commandCounter = 0;
  let previousMode = null;
  let modeFlashUntil = 0;
  let localSettings = { guardArmed: null };

  function setText(element, value) {
    if (element) {
      element.textContent = value;
    }
  }

  function setStatus(element, label, state) {
    setText(element, label);
    element.dataset.state = state;
  }

  function transportConnected() {
    return serialConnected || typeof testTransport === "function";
  }

  function currentTimeLabel(timestamp) {
    if (timestamp === null || timestamp === undefined) {
      return "--";
    }
    return new Date(timestamp).toLocaleTimeString("zh-CN", {
      hour12: false,
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
  }

  function logEntry(type, message, isError = false, timestamp = Date.now()) {
    const item = document.createElement("li");
    const time = document.createElement("time");
    const badge = document.createElement("span");
    const copy = document.createElement("p");
    time.textContent = currentTimeLabel(timestamp);
    badge.className = `log-type${isError ? " is-error" : ""}`;
    badge.textContent = type;
    copy.textContent = message;
    item.append(time, badge, copy);

    if (refs.runLog.dataset.started !== "true") {
      refs.runLog.replaceChildren();
      refs.runLog.dataset.started = "true";
    }
    refs.runLog.prepend(item);
    while (refs.runLog.children.length > 60) {
      refs.runLog.lastElementChild.remove();
    }
  }

  function frameSummary(frame) {
    if (frame.type === "hello") {
      return `识别到 ${frame.profileId || "未知身份"}，来源 ${frame.source === "mock" ? "模拟" : "串口"}`;
    }
    if (frame.type === "telemetry") {
      return `遥测 #${frame.seq ?? "--"} · ${frame.mode || "--"} · ${frame.solarTerm || "--"}`;
    }
    if (frame.type === "event") {
      return `本地事件 ${frame.event || "unknown"}${frame.key ? ` · ${frame.key}` : ""}`;
    }
    if (frame.type === "ack") {
      return `${frame.ok ? "已接受" : "已拒绝"} ${frame.id || "无 ID"}${frame.error ? ` · ${frame.error}` : ""}`;
    }
    return "未知协议帧";
  }

  function processLine(line, timestamp = Date.now()) {
    const result = Core.ingestLine(session, line, timestamp);
    if (!result.accepted) {
      if (result.reason === "wrong_profile") {
        identityError = "不是本项目开发板";
      }
      const messageMap = {
        invalid_json: "收到无法解析的串口行，已忽略，实时状态未改变",
        awaiting_identity: "身份确认前收到数据帧，已忽略",
        wrong_profile: "开发板身份与本项目不匹配，已拒绝数据",
        mixed_source: "同一会话混入不同数据来源，已拒绝数据",
        unsupported_frame: "收到未知协议类型，已忽略",
      };
      logEntry("解析", messageMap[result.reason] || result.reason, true, timestamp);
      render(timestamp);
      return result;
    }

    session = result.state;
    identityError = null;
    const frame = result.frame;
    logEntry(frame.type, frameSummary(frame), frame.type === "ack" && frame.ok === false, timestamp);

    if (frame.type === "event" && frame.event === "button" && frame.key === "A") {
      setText(refs.buttonAEvent, `${frame.action === "toggle_mode" ? "切换模式" : frame.action || "触发"} → ${frame.mode || "--"}`);
    }

    if (pendingTransaction) {
      pendingTransaction = Core.reduceTransaction(pendingTransaction, frame, timestamp);
      if (frame.type === "telemetry" && frame.lastAppliedCommandId === pendingTransaction.id) {
        applyCompletedSetting();
      }
    }

    if (frame.type === "telemetry" && frame.mode && frame.mode !== previousMode) {
      previousMode = frame.mode;
      modeFlashUntil = timestamp + 2200;
    }
    render(timestamp);
    return result;
  }

  function applyCompletedSetting() {
    if (!pendingTransaction || pendingTransaction.status !== "complete" || pendingTransaction.uiApplied) {
      return;
    }
    const command = pendingTransaction.command;
    if (command.set && Object.prototype.hasOwnProperty.call(command.set, "guardArmed")) {
      localSettings.guardArmed = command.set.guardArmed;
    }
    pendingTransaction = { ...pendingTransaction, uiApplied: true };
  }

  function clearElement(element) {
    element.replaceChildren();
  }

  function renderAlerts(telemetry, live) {
    refs.alertNotice.classList.remove("is-waiting", "is-safe", "is-danger");
    clearElement(refs.alertList);
    if (!live || !telemetry) {
      refs.alertNotice.classList.add("is-waiting");
      setText(refs.alertKicker, "家庭安全");
      setText(refs.alertTitle, "等待实时数据");
      setText(refs.alertSummary, "连接目标开发板后，这里会显示安全原因、来源位置和设备目标动作。");
      return [];
    }

    const alerts = AlertCore.describeAlerts(telemetry);
    if (alerts.length === 0) {
      refs.alertNotice.classList.add("is-safe");
      setText(refs.alertKicker, "家庭安全 · 本帧结论");
      setText(refs.alertTitle, "当前没有安全告警");
      setText(refs.alertSummary, "结论来自开发板 telemetry.alerts；传感器原始值不会在网页中自行生成告警。");
      return [];
    }

    refs.alertNotice.classList.add("is-danger");
    setText(refs.alertKicker, `家庭安全 · ${alerts.length} 项告警`);
    setText(refs.alertTitle, "P1 安全联动正在执行");
    setText(refs.alertSummary, "安全状态优先于家庭模式、节气策略和普通操作。");
    for (const alert of alerts) {
      const item = document.createElement("li");
      const source = document.createElement("strong");
      const details = document.createElement("span");
      source.textContent = `${alert.roomLabel} · ${alert.gpio}`;
      details.textContent = `${alert.reason}。目标动作：${alert.action}`;
      item.append(source, details);
      refs.alertList.append(item);
    }
    return alerts;
  }

  function roomElement(room) {
    return document.querySelector(`[data-room="${room}"]`);
  }

  function setRoom(room, value, status) {
    const element = roomElement(room);
    if (!element) return;
    setText(element.querySelector("[data-room-value]"), value);
    setText(element.querySelector("[data-room-status]"), status);
  }

  function renderHouse(telemetry, values, live, alerts, now) {
    const rooms = document.querySelectorAll("[data-room]");
    rooms.forEach((room) => room.classList.remove("is-mode-active", "is-mode-flash", "is-alert"));
    refs.houseShell.classList.toggle("is-live", live);
    refs.houseShell.classList.toggle("is-danger", alerts.length > 0);

    if (!live || !telemetry) {
      setText(refs.houseMode, "等待模式");
      refs.houseMode.className = "mode-badge";
      setRoom("master", "窗帘 --", "等待数据");
      setRoom("living", "温度 --", "等待数据");
      setRoom("study", "声音 --", "等待数据");
      setRoom("balcony", "光照 --", "等待数据");
      setRoom("dining", "节气 --", "等待数据");
      setRoom("kitchen", "空气 --", "等待数据");
      setRoom("entry", "活动 --", "等待数据");
      setRoom("bath", "水浸 --", "等待数据");
      return;
    }

    const sensors = telemetry.sensors || {};
    const actuators = telemetry.actuators || {};
    setText(refs.houseMode, telemetry.mode || "--");
    refs.houseMode.className = `mode-badge ${telemetry.mode === "Sleep" ? "is-sleep" : "is-auto"}`;

    const activeRooms = Core.modeActiveRooms(telemetry.mode);
    for (const roomName of activeRooms) {
      const room = roomElement(roomName);
      if (room) {
        room.classList.add("is-mode-active");
        if (now < modeFlashUntil) room.classList.add("is-mode-flash");
      }
    }
    for (const alert of alerts) {
      const room = roomElement(alert.room);
      if (room) room.classList.add("is-alert");
    }

    const relayTarget = typeof actuators.relay === "boolean" ? (actuators.relay ? "开" : "关") : "--";
    const waterValue = typeof sensors.water === "boolean" ? (sensors.water ? "探头触发" : "探头干燥") : "--";
    setRoom("master", `窗帘 ${Number.isFinite(actuators.curtainClosePercent) ? `${actuators.curtainClosePercent}%` : "--"}`, activeRooms.includes("master") ? "本模式工作中" : "普通策略暂停");
    setRoom("living", `温度 ${values.temperature} · 风扇 ${Number.isFinite(actuators.fanPercent) ? `${actuators.fanPercent}%` : "--"}`, activeRooms.includes("living") ? "本模式工作中" : "普通策略暂停");
    setRoom("study", `声音 ${values.sound} · 灯 ${relayTarget}`, activeRooms.includes("study") ? "本模式工作中" : "普通策略暂停");
    setRoom("balcony", `光照 ${values.light}`, activeRooms.includes("balcony") ? "本模式工作中" : "环境监测");
    setRoom("dining", `节气 ${telemetry.solarTerm || "--"}`, activeRooms.includes("dining") ? "本模式工作中" : "节气策略暂停");
    setRoom("kitchen", `空气 ${values.airQuality} · 火焰 ${sensors.flame === true ? "触发" : sensors.flame === false ? "未触发" : "--"}`, alerts.some((item) => item.room === "kitchen") ? "安全告警" : "安全监测中");
    setRoom("entry", `活动 ${values.presence}`, alerts.some((item) => item.room === "entry") ? "安全告警" : localSettings.guardArmed === true ? "已布防" : localSettings.guardArmed === false ? "未布防" : "布防状态待命令");
    setRoom("bath", `水浸 ${waterValue}`, alerts.some((item) => item.room === "bath") ? "安全告警" : "安全监测中");
  }

  function renderSeason(telemetry, live) {
    if (!live || !telemetry || !telemetry.solarTerm) {
      setText(refs.seasonGlyph, "候");
      setText(refs.seasonTitle, "等待节气");
      setText(refs.seasonExplanation, "收到实时数据后，展示温度建议、窗帘偏置和补光阈值。");
      setText(refs.strategyTemperature, "--");
      setText(refs.strategyCurtain, "--");
      setText(refs.strategyLight, "--");
      return;
    }
    const term = telemetry.solarTerm;
    const profile = Core.solarTermProfile(term);
    setText(refs.seasonGlyph, term);
    setText(refs.seasonTitle, `${term} · ${telemetry.mode === "Auto" ? "策略运行中" : "普通策略暂停"}`);
    setText(
      refs.seasonExplanation,
      telemetry.mode === "Auto"
        ? "节气影响窗帘与补光；GPIO17 旋钮仍决定最终温控阈值。"
        : "Sleep 保留本地安全监测，暂不执行普通窗帘和补光策略。",
    );
    setText(refs.strategyTemperature, profile ? `${profile.temperatureC}℃` : "--");
    setText(refs.strategyCurtain, profile ? `${profile.curtainClosePercent}%` : "--");
    setText(refs.strategyLight, profile ? String(profile.lightThreshold) : "--");
    if (Core.SOLAR_TERMS.includes(term)) {
      refs.solarSelect.value = term;
    }
  }

  function renderMetrics(values) {
    setText(refs.metricTemperature, values.temperature);
    setText(refs.metricHumidity, values.humidity);
    setText(refs.metricAir, values.airQuality);
    setText(refs.metricSound, values.sound);
    setText(refs.metricPresence, values.presence);
    setText(refs.metricKnob, values.knob);
    setText(refs.metricThreshold, values.threshold);
    setText(refs.liveDot, values.live ? "实时" : "等待");
    refs.liveDot.classList.toggle("is-live", values.live);
  }

  function renderDevices(telemetry, live) {
    if (!live || !telemetry) {
      setText(refs.deviceFan, "--");
      setText(refs.deviceCurtain, "--");
      setText(refs.deviceRelay, "GPIO 目标 --");
      setText(refs.deviceBuzzer, "--");
      setText(refs.deviceRgb, "--");
      refs.rgbIndicator.dataset.rgb = "off";
      return;
    }
    const actuators = telemetry.actuators || {};
    setText(refs.deviceFan, Number.isFinite(actuators.fanPercent) ? `${actuators.fanPercent}%` : "--");
    const curtain = Number.isFinite(actuators.curtainClosePercent) ? `${actuators.curtainClosePercent}% 关闭` : "--";
    setText(refs.deviceCurtain, actuators.curtainControlEnabled === false ? `${curtain} · 暂停调节` : curtain);
    setText(refs.deviceRelay, typeof actuators.relay === "boolean" ? `GPIO 目标 ${actuators.relay ? "开" : "关"}` : "GPIO 目标 --");
    setText(refs.deviceBuzzer, typeof actuators.buzzer === "boolean" ? (actuators.buzzer ? "目标：报警" : "目标：安静") : "--");
    const rgbLabels = { yellow: "目标：黄色", red: "目标：红色", off: "目标：关闭" };
    setText(refs.deviceRgb, rgbLabels[actuators.rgb] || "--");
    refs.rgbIndicator.dataset.rgb = ["yellow", "red", "off"].includes(actuators.rgb) ? actuators.rgb : "off";
  }

  function renderCommand() {
    const strong = refs.commandStatus.querySelector("strong");
    const small = refs.commandStatus.querySelector("small");
    if (transportError) {
      refs.commandStatus.dataset.state = "rejected";
      setText(strong, `发送失败：${transportError}`);
      setText(small, "页面没有修改家庭模式或设备状态，请检查串口后重试。");
      return;
    }
    if (pendingTransaction) {
      pendingTransaction = Core.expireTransaction(pendingTransaction, Date.now());
    }
    const label = Core.transactionLabel(pendingTransaction);
    refs.commandStatus.dataset.state = pendingTransaction ? pendingTransaction.status : "idle";
    setText(strong, label);
    if (!pendingTransaction) {
      setText(small, "页面只在 ACK 与新遥测同时到达后显示“状态已更新”。");
    } else {
      setText(small, `命令 ID：${pendingTransaction.id}`);
    }
  }

  function renderControls(live) {
    const available = transportConnected() && session.identityLocked && live;
    document.querySelectorAll("[data-command-kind]").forEach((button) => {
      button.disabled = !available;
      if (button.dataset.commandKind === "mode") {
        button.setAttribute("aria-pressed", String(session.telemetry && session.telemetry.mode === button.dataset.commandValue));
      }
    });
    refs.solarSelect.disabled = !available;
    refs.applySolar.disabled = !available;
    setText(refs.controlLock, available ? "目标开发板在线，可安全操作" : "连接目标开发板后可操作");
  }

  function renderConnection(now) {
    const connected = transportConnected();
    const board = Core.connectionState(session, connected, now);
    setStatus(refs.serialStatus, testTransport ? "模拟通道已连接" : serialConnected ? "串口已连接" : "串口未连接", connected ? "connected" : "idle");
    setStatus(refs.boardStatus, identityError || board.label, identityError ? "error" : board.code);
    if (session.dataSource === "mock") {
      setStatus(refs.sourceStatus, "模拟数据", "mock");
    } else if (session.dataSource === "real") {
      setStatus(refs.sourceStatus, "真板数据", board.code === "real" ? "real" : "offline");
    } else {
      setStatus(refs.sourceStatus, "暂无数据", "idle");
    }
    setText(refs.lastFrameTime, currentTimeLabel(session.lastFrameAt));
    refs.mockRibbon.hidden = session.dataSource !== "mock";
    refs.connect.disabled = serialConnected || typeof testTransport === "function" || !serialFeatureAvailable();
    refs.disconnect.disabled = !connected;
  }

  function render(now = Date.now()) {
    const values = Core.liveValues(transportConnected() ? session : Core.createSession(), now);
    const telemetry = values.live ? session.telemetry : null;
    renderConnection(now);
    const alerts = renderAlerts(telemetry, values.live);
    renderHouse(telemetry, values, values.live, alerts, now);
    renderSeason(telemetry, values.live);
    renderMetrics(values);
    renderDevices(telemetry, values.live);
    renderControls(values.live);
    renderCommand();
  }

  function commandId() {
    commandCounter += 1;
    return `web-${Date.now().toString(36)}-${commandCounter.toString(36)}`;
  }

  async function writeCommand(command) {
    if (typeof testTransport === "function") {
      await testTransport(command);
      return;
    }
    if (!serialPort || !serialPort.writable) {
      throw new Error("串口不可写");
    }
    const writer = serialPort.writable.getWriter();
    try {
      const payload = `${JSON.stringify(command)}\n`;
      await writer.write(new TextEncoder().encode(payload));
    } finally {
      writer.releaseLock();
    }
  }

  async function sendCommand(kind, value) {
    if (!transportConnected() || !session.identityLocked || !Core.isFresh(session, Date.now())) {
      transportError = "目标开发板不在线";
      render();
      return;
    }
    let command;
    try {
      command = Core.buildCommand(kind, value, commandId());
    } catch (error) {
      transportError = error.message;
      render();
      return;
    }
    transportError = null;
    pendingTransaction = Core.createTransaction(command, Date.now(), session.telemetry ? Number(session.telemetry.seq) || 0 : 0);
    logEntry("command", `${command.id} · ${kind} → ${String(value)}`);
    renderCommand();
    try {
      await writeCommand(command);
    } catch (error) {
      transportError = error && error.message ? error.message : "串口写入失败";
      logEntry("发送", transportError, true);
      render();
    }
  }

  function serialFeatureAvailable() {
    return Boolean(window.isSecureContext && navigator.serial);
  }

  function configureSerialSupport() {
    if (serialFeatureAvailable()) {
      refs.browserMessage.hidden = true;
      return;
    }
    refs.browserMessage.hidden = false;
    setText(
      refs.browserMessage,
      "当前环境不能使用 Web Serial。请在本机 Chrome 或 Edge 中通过 localhost/HTTPS 打开页面。",
    );
    refs.connect.disabled = true;
  }

  async function readSerialLoop() {
    if (!serialPort || !serialPort.readable) return;
    serialReader = serialPort.readable.getReader();
    const decoder = new TextDecoder();
    inputBuffer = "";
    try {
      while (keepReading) {
        const { value, done } = await serialReader.read();
        if (done) break;
        inputBuffer += decoder.decode(value, { stream: true });
        const lines = inputBuffer.split(/\r?\n/);
        inputBuffer = lines.pop() || "";
        for (const line of lines) {
          if (line.trim()) processLine(line);
        }
      }
    } catch (error) {
      if (keepReading) {
        logEntry("串口", error && error.message ? error.message : "读取中断", true);
      }
    } finally {
      if (serialReader) {
        serialReader.releaseLock();
        serialReader = null;
      }
    }
  }

  async function connectSerial() {
    if (!serialFeatureAvailable()) {
      configureSerialSupport();
      return;
    }
    try {
      const port = await navigator.serial.requestPort();
      await port.open({ baudRate: 115200 });
      serialPort = port;
      serialConnected = true;
      keepReading = true;
      session = Core.createSession();
      identityError = null;
      pendingTransaction = null;
      transportError = null;
      previousMode = null;
      modeFlashUntil = 0;
      localSettings = { guardArmed: null };
      logEntry("串口", "串口已打开，等待目标开发板 hello");
      render();
      serialLoopPromise = readSerialLoop();
    } catch (error) {
      logEntry("串口", error && error.message ? error.message : "连接失败", true);
      render();
    }
  }

  async function disconnectSerial() {
    keepReading = false;
    if (serialReader) {
      try {
        await serialReader.cancel();
      } catch (_error) {
        // Reader cleanup continues below.
      }
    }
    if (serialLoopPromise) {
      try {
        await serialLoopPromise;
      } catch (_error) {
        // The read loop already recorded unexpected failures.
      }
      serialLoopPromise = null;
    }
    if (serialPort) {
      try {
        await serialPort.close();
      } catch (_error) {
        // A detached USB device may already be closed.
      }
    }
    serialPort = null;
    serialConnected = false;
    testTransport = null;
    session = Core.createSession();
    identityError = null;
    previousMode = null;
    modeFlashUntil = 0;
    if (pendingTransaction && !["complete", "rejected", "timeout"].includes(pendingTransaction.status)) {
      transportError = "连接已断开";
    }
    logEntry("串口", "连接已断开");
    render();
  }

  function bindControls() {
    refs.connect.addEventListener("click", connectSerial);
    refs.disconnect.addEventListener("click", disconnectSerial);
    refs.applySolar.addEventListener("click", () => sendCommand("solarTerm", refs.solarSelect.value));
    document.querySelectorAll("[data-command-kind]").forEach((button) => {
      button.addEventListener("click", () => {
        const raw = button.dataset.commandValue;
        const value = raw === "true" ? true : raw === "false" ? false : raw;
        sendCommand(button.dataset.commandKind, value);
      });
    });
    refs.clearLog.addEventListener("click", () => {
      refs.runLog.replaceChildren();
      refs.runLog.dataset.started = "true";
      logEntry("系统", "运行记录已清空");
    });
    if (navigator.serial && typeof navigator.serial.addEventListener === "function") {
      navigator.serial.addEventListener("disconnect", (event) => {
        if (event.target === serialPort) disconnectSerial();
      });
    }
  }

  window.SmartLifeConsoleTest = {
    feedLine(line, timestamp = Date.now()) {
      return processLine(line, timestamp);
    },
    feedFrame(frame, timestamp = Date.now()) {
      return processLine(JSON.stringify(frame), timestamp);
    },
    setTransport(transport) {
      testTransport = transport;
      serialConnected = false;
      session = Core.createSession();
      identityError = null;
      pendingTransaction = null;
      transportError = null;
      previousMode = null;
      modeFlashUntil = 0;
      localSettings = { guardArmed: null };
      render();
    },
    reset() {
      testTransport = null;
      session = Core.createSession();
      identityError = null;
      pendingTransaction = null;
      transportError = null;
      previousMode = null;
      modeFlashUntil = 0;
      render();
    },
    state() {
      return { session, pendingTransaction, localSettings };
    },
    render,
  };

  configureSerialSupport();
  bindControls();
  render();
  window.setInterval(() => render(Date.now()), 400);
})();
