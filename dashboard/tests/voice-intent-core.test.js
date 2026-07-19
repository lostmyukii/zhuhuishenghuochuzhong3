const assert = require("assert");
const VoiceCore = require("../voice-intent-core.js");

const tests = [];

function test(name, fn) {
  tests.push({ name, fn });
}

test("maps only the approved Auto phrases", () => {
  for (const phrase of ["打开自动模式", "切换到自动模式"]) {
    const result = VoiceCore.parseIntent(phrase);
    assert.equal(result.ok, true);
    assert.equal(result.intent, "setMode");
    assert.equal(result.mode, "Auto");
  }
});

test("maps only the approved Sleep phrases", () => {
  for (const phrase of ["打开睡眠模式", "切换到睡眠模式"]) {
    const result = VoiceCore.parseIntent(phrase);
    assert.equal(result.ok, true);
    assert.equal(result.intent, "setMode");
    assert.equal(result.mode, "Sleep");
  }
});

test("normalizes harmless whitespace and terminal punctuation", () => {
  assert.equal(VoiceCore.parseIntent("  打开 自动 模式。 ").mode, "Auto");
  assert.equal(VoiceCore.parseIntent("切换到睡眠模式！").mode, "Sleep");
});

test("rejects empty, unrelated and ambiguous speech", () => {
  for (const phrase of [
    "",
    "今天天气怎么样",
    "自动模式",
    "切换模式",
    "自动模式和睡眠模式",
    "打开自动模式然后睡眠",
  ]) {
    assert.deepEqual(VoiceCore.parseIntent(phrase).intent, "unknown", phrase);
  }
});

test("rejects negation and dangerous safety overrides", () => {
  for (const phrase of [
    "不要打开自动模式",
    "关闭安全报警",
    "水浸时关闭蜂鸣器",
    "忽略火焰告警",
    "关闭风扇",
    "打开继电器",
    "把红灯关掉",
  ]) {
    assert.equal(VoiceCore.parseIntent(phrase).ok, false, phrase);
  }
});

test("does not accept approved words as a substring of a longer instruction", () => {
  assert.equal(VoiceCore.parseIntent("请打开自动模式").intent, "unknown");
  assert.equal(VoiceCore.parseIntent("测试打开睡眠模式测试").intent, "unknown");
});

test("builds a voice-origin command only after re-parsing the text", () => {
  assert.deepEqual(VoiceCore.commandFromText("打开自动模式", "voice-001"), {
    ok: true,
    intent: {
      ok: true,
      intent: "setMode",
      mode: "Auto",
      normalizedText: "打开自动模式",
    },
    command: {
      type: "command",
      id: "voice-001",
      origin: "voice",
      mode: "Auto",
    },
  });
});

test("unknown text never creates a command", () => {
  const result = VoiceCore.commandFromText("关闭安全报警", "voice-002");
  assert.equal(result.ok, false);
  assert.equal(result.intent.intent, "unknown");
  assert.equal(Object.prototype.hasOwnProperty.call(result, "command"), false);
});

test("requires a bounded non-empty command id", () => {
  assert.throws(() => VoiceCore.commandFromText("打开自动模式", ""));
  assert.throws(() => VoiceCore.commandFromText("打开自动模式", "x".repeat(65)));
});

test("maps browser recognition failures to truthful local messages", () => {
  assert.match(VoiceCore.recognitionErrorMessage("not-allowed"), /权限/);
  assert.match(VoiceCore.recognitionErrorMessage("audio-capture"), /麦克风/);
  assert.match(VoiceCore.recognitionErrorMessage("no-speech"), /没有识别到/);
  assert.match(VoiceCore.recognitionErrorMessage("network"), /网络/);
  assert.match(VoiceCore.recognitionErrorMessage("something-new"), /something-new/);
});

let passed = 0;
for (const item of tests) {
  try {
    item.fn();
    passed += 1;
    console.log(`ok - ${item.name}`);
  } catch (error) {
    console.error(`not ok - ${item.name}`);
    throw error;
  }
}

console.log(`${passed}/${tests.length} voice intent tests passed`);
