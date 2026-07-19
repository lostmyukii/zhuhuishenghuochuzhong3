# 节气智居 · 初中组

基于 N16R8 和现有“两室两厅一卫”沙盘的智慧生活项目。固定传感器类型与 GPIO 接线不变，创新集中在二十四节气场景、家庭模式、安全优先级和可审计的 `command → ack → telemetry` 闭环。

## 当前状态

- G0 规格已冻结。
- G1 软件与模拟全场景已达到 `mock-passed`。
- G2 只读预检已完成，但数据串口和六项现场电气检查仍待确认，所以没有烧录、没有真板通过。
- 静态网页发布包可重复构建；仓库本身不会自动上线。

权威状态见 [docs/验收状态.md](docs/验收状态.md)，固定接线见 [硬件接线记录.md](硬件接线记录.md)。

## 本地运行台

```bash
python3 -m http.server 8767 --directory dashboard
```

然后用桌面版 Chrome/Edge 打开 `http://127.0.0.1:8767/`。Web Serial 只在 localhost 或 HTTPS 安全上下文中可用。

## 验证

```bash
python3 -m unittest discover -s tools -p 'test_*.py'
/Users/yukii/.platformio/penv/bin/pio test -d firmware -e native
/Users/yukii/.platformio/penv/bin/pio run -d firmware
node dashboard/tests/protocol-core.test.js
node dashboard/tests/alert-core.test.js
node dashboard/tests/voice-intent-core.test.js
node dashboard/tests/dashboard-contract.test.js
node dashboard/tests/mock-replay.test.js
git diff --check
```

## 构建部署目录（不发布）

```bash
python3 tools/build_static_release.py --version 20260720-predeploy.1
python3 tools/verify_static_release.py
```

产物位于 `dist/smartlife-junior-solar-home-v1/`。实际托管要求和停止边界见 [deploy/README.md](deploy/README.md)。
