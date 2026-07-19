const fs = require("fs");
const path = require("path");
const { test, expect } = require("@playwright/test");

const baseUrl = process.env.PREDEPLOY_URL || "http://127.0.0.1:8770/";
const evidenceDirectory = path.resolve(__dirname, "../../docs/evidence/preflight");
const frames = fs
  .readFileSync(
    path.resolve(__dirname, "../../docs/evidence/mock/task10-g1-frames-20260720.jsonl"),
    "utf8",
  )
  .trim()
  .split("\n")
  .map((line) => JSON.parse(line));

test.use({ channel: "chrome" });

function watchPage(page) {
  const consoleProblems = [];
  const failedRequests = [];
  page.on("console", (message) => {
    if (["error", "warning"].includes(message.type())) {
      consoleProblems.push(`${message.type()}: ${message.text()}`);
    }
  });
  page.on("requestfailed", (request) => {
    failedRequests.push(`${request.method()} ${request.url()}: ${request.failure()?.errorText}`);
  });
  return { consoleProblems, failedRequests };
}

async function replayThrough(page, telemetrySequence) {
  const selected = frames.filter(
    (frame) => frame.type !== "telemetry" || frame.seq <= telemetrySequence,
  );
  await page.evaluate((inputFrames) => {
    window.SmartLifeConsoleTest.setTransport(() => undefined);
    const started = Date.now();
    inputFrames.forEach((frame, index) => {
      window.SmartLifeConsoleTest.feedFrame(frame, started + index * 10);
    });
  }, selected);
}

test("built release loads only versioned local assets and replays recovery", async ({ page, request }) => {
  const observed = watchPage(page);
  const response = await page.goto(baseUrl, { waitUntil: "networkidle" });
  expect(response.status()).toBe(200);
  expect(await page.title()).toContain("全屋运行台");
  expect(await page.evaluate(() => window.isSecureContext)).toBe(true);
  expect(await page.locator("body").innerText()).not.toMatch(/评分|得分|满分|评委/);

  const resourceUrls = await page.evaluate(() =>
    performance.getEntriesByType("resource").map((entry) => entry.name),
  );
  for (const asset of ["style.css", "protocol-core.js", "alert-core.js", "voice-intent-core.js", "app.js"]) {
    expect(resourceUrls.some((url) => url.includes(`${asset}?v=20260720-predeploy.1`))).toBe(true);
  }

  const manifestResponse = await request.get(new URL("asset-manifest.json", baseUrl).toString());
  expect(manifestResponse.status()).toBe(200);
  const manifest = await manifestResponse.json();
  expect(manifest.releaseVersion).toBe("20260720-predeploy.1");
  expect(manifest.runtime).toBe("static-https-web-serial");

  await replayThrough(page, 12);
  await expect(page.locator("#source-status")).toContainText("模拟");
  await expect(page.locator("#mock-ribbon")).toBeVisible();
  await expect(page.locator("#alert-title")).toContainText("没有安全告警");
  await expect(page.locator("#device-fan")).toHaveText("35%");
  await page.screenshot({
    path: path.join(evidenceDirectory, "task12-static-release-desktop-20260720.png"),
    fullPage: true,
  });

  expect(observed.consoleProblems).toEqual([]);
  expect(observed.failedRequests).toEqual([]);
});

test("built release keeps the water safety state readable at 390px", async ({ page }) => {
  const observed = watchPage(page);
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto(baseUrl, { waitUntil: "networkidle" });
  await replayThrough(page, 9);
  await expect(page.locator("#alert-title")).toContainText("P1 安全联动");
  await expect(page.locator('[data-room="bath"]')).toHaveClass(/is-alert/);
  await expect(page.locator("#device-fan")).toHaveText("100%");

  const dimensions = await page.evaluate(() => ({
    innerWidth: window.innerWidth,
    bodyWidth: document.body.scrollWidth,
    documentWidth: document.documentElement.scrollWidth,
  }));
  expect(dimensions.bodyWidth).toBeLessThanOrEqual(dimensions.innerWidth);
  expect(dimensions.documentWidth).toBeLessThanOrEqual(dimensions.innerWidth);
  await page.screenshot({
    path: path.join(evidenceDirectory, "task12-static-release-mobile-390-20260720.png"),
    fullPage: true,
  });

  expect(observed.consoleProblems).toEqual([]);
  expect(observed.failedRequests).toEqual([]);
});
