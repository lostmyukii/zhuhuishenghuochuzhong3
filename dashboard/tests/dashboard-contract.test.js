const assert = require("assert");
const fs = require("fs");
const path = require("path");

const dashboard = path.resolve(__dirname, "..");
const read = (name) => fs.readFileSync(path.join(dashboard, name), "utf8");

const required = ["index.html", "style.css", "protocol-core.js", "alert-core.js", "app.js"];
for (const file of required) {
  assert.ok(fs.existsSync(path.join(dashboard, file)), `missing ${file}`);
}

const html = read("index.html");
const css = read("style.css");
const app = read("app.js");
const visibleSources = `${html}\n${app}`;

assert.match(html, /节气智居/);
assert.match(html, /全屋运行台/);
assert.match(html, /MQ2 等效估算，非标准检测仪读数/);
assert.match(html, /GPIO 目标/);
assert.doesNotMatch(visibleSources, /评分|得分|满分|评委/);

for (const room of ["主卧", "次卧\/书房", "客厅", "餐厅", "卫生间", "厨房附区", "入户附区", "阳台附区"]) {
  assert.match(html, new RegExp(room));
}

assert.match(html, /role="status"/);
assert.match(html, /aria-live="polite"/);
assert.match(html, /aria-atomic="true"/);
assert.match(html, /id="connect-serial"/);
assert.match(html, /id="disconnect-serial"/);
assert.ok(html.indexOf("protocol-core.js") < html.indexOf("app.js"));
assert.ok(html.indexOf("alert-core.js") < html.indexOf("app.js"));

assert.match(app, /navigator\.serial/);
assert.match(app, /baudRate:\s*115200/);
assert.match(app, /TextDecoder/);
assert.match(app, /lastAppliedCommandId/);
assert.match(app, /SmartLifeConsoleTest/);
assert.doesNotMatch(app, /innerHTML\s*=/);

for (const token of ["--moon", "--ink", "--celadon", "--sunlight", "--wire", "--danger"]) {
  assert.match(css, new RegExp(token));
}
assert.match(css, /overflow-x:\s*hidden/);
assert.match(css, /@media\s*\(max-width:\s*620px\)/);
assert.match(css, /prefers-reduced-motion:\s*reduce/);
assert.match(css, /:focus-visible/);

console.log("dashboard contract passed");
