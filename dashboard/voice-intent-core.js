(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  }
  root.SmartLifeVoiceIntentCore = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
  "use strict";

  const COMMAND_ID_MAX_LENGTH = 64;
  const PHRASE_TO_MODE = new Map([
    ["打开自动模式", "Auto"],
    ["切换到自动模式", "Auto"],
    ["打开睡眠模式", "Sleep"],
    ["切换到睡眠模式", "Sleep"],
  ]);

  function normalizeText(text) {
    if (typeof text !== "string") {
      return "";
    }
    return text
      .normalize("NFKC")
      .trim()
      .replace(/\s+/g, "")
      .replace(/[。！!？?，,、；;：:]+$/g, "");
  }

  function parseIntent(text) {
    const normalizedText = normalizeText(text);
    const mode = PHRASE_TO_MODE.get(normalizedText);
    if (!mode) {
      return {
        ok: false,
        intent: "unknown",
        reason: normalizedText ? "not_whitelisted" : "empty_text",
        normalizedText,
      };
    }
    return {
      ok: true,
      intent: "setMode",
      mode,
      normalizedText,
    };
  }

  function assertCommandId(id) {
    if (typeof id !== "string" || id.length === 0) {
      throw new Error("voice command id is required");
    }
    if (id.length > COMMAND_ID_MAX_LENGTH) {
      throw new Error("voice command id is too long");
    }
  }

  function commandFromText(text, id) {
    assertCommandId(id);
    const intent = parseIntent(text);
    if (!intent.ok) {
      return { ok: false, intent };
    }
    return {
      ok: true,
      intent,
      command: {
        type: "command",
        id,
        origin: "voice",
        mode: intent.mode,
      },
    };
  }

  function recognitionErrorMessage(code) {
    const messages = {
      "not-allowed": "麦克风权限未允许；没有发送任何命令。",
      "service-not-allowed": "浏览器语音服务权限未允许；没有发送任何命令。",
      "audio-capture": "没有找到可用麦克风；没有发送任何命令。",
      "no-speech": "没有识别到有效语音；没有发送任何命令。",
      "aborted": "本次语音识别已停止；没有发送任何命令。",
      "network": "浏览器语音服务网络异常；没有发送任何命令。",
    };
    return messages[code] || `语音识别失败（${code || "unknown"}）；没有发送任何命令。`;
  }

  return {
    COMMAND_ID_MAX_LENGTH,
    normalizeText,
    parseIntent,
    commandFromText,
    recognitionErrorMessage,
  };
});
