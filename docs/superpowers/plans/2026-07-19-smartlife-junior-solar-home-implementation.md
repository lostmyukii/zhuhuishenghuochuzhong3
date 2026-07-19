# N16R8 节气智居实施计划

> 日期：2026-07-19
>
> 对应规格：`docs/superpowers/specs/2026-07-19-smartlife-junior-solar-home-design.md`
>
> 当前状态：任务一至五已完成软件合同、原生测试和目标板编译；尚未烧录、建设网页或进行真板验收

## 1. 目标

在 `/Users/yukii/Desktop/智慧生活/初中3` 独立完成以下交付链：

```text
固定 GPIO 合同
  → 可测试的本地控制规则
  → N16R8 固件与单行 JSON 协议
  → 本地 Web Serial 评分控制台
  → 语音白名单命令
  → 模拟主板验收
  → 真板逐模块验收
  → 5 分钟满分彩排证据
```

最终目标是达到 `full-score-ready`，但每个状态必须依证据逐级升级，不能提前宣称。

## 2. 实施边界

- 当前仓库是“初中3”的唯一实现源，不把“初中”或“初中2组”的代码状态当作当前完成证据。
- 可从 `/Users/yukii/Desktop/智慧生活/初中/firmware` 复用 `n16r8_esp32s3` 板卡 JSON、PlatformIO 基础参数和经过验证的库选择。
- 不复制旧项目的 `profileId`、场景名称、网页文案、模拟在线状态或测试通过结论。
- 第一阶段只做本地 Web Serial，不建设 WSS、MQTT 或公网部署；评分表只要求电脑软件输出，公网不是满分前置条件。
- 继电器只接低压负载；不点火、不释放可燃气体；真板安全创新使用水浸演示。
- 烧录前必须明确串口设备并完成电气预检；计划授权不等于允许对不明串口或其他开发板烧录。

固定端口在全部任务中保持如下映射：

| 模块 | 固定端口 |
| --- | --- |
| 光照 | `GPIO1` |
| 声音 | `GPIO4` |
| DHT11 | `GPIO14` |
| PIR | `GPIO5` |
| 8 键 AD | `GPIO10` |
| MQ2 | `GPIO2` |
| 水浸 | `GPIO8` |
| 火焰 | `GPIO45` |
| 舵机 | `GPIO9` |
| 风扇 | `GPIO11` |
| 继电器 | `GPIO12` |
| 蜂鸣器 | `GPIO13` |
| RGB | `GPIO46` |
| OLED SDA/SCL | `GPIO41 / GPIO42` |
| 旋钮 | `GPIO17` |

## 3. 目标目录

```text
初中3/
├── firmware/
│   ├── boards/n16r8_esp32s3.json
│   ├── include/
│   │   ├── smartlife_config.h
│   │   ├── smartlife_models.h
│   │   ├── control_engine.h
│   │   ├── solar_terms.h
│   │   ├── hardware_io.h
│   │   ├── display_controller.h
│   │   └── serial_protocol.h
│   ├── src/
│   │   ├── main.cpp
│   │   ├── control_engine.cpp
│   │   ├── solar_terms.cpp
│   │   ├── hardware_io.cpp
│   │   ├── display_controller.cpp
│   │   └── serial_protocol.cpp
│   ├── test/
│   │   ├── test_control_engine/test_main.cpp
│   │   └── test_solar_terms/test_main.cpp
│   └── platformio.ini
├── dashboard/
│   ├── index.html
│   ├── style.css
│   ├── protocol-core.js
│   ├── voice-intent-core.js
│   ├── app.js
│   └── tests/
│       ├── protocol-core.test.js
│       ├── voice-intent-core.test.js
│       └── dashboard-contract.test.js
├── tools/
│   ├── n16r8_mock_board.py
│   ├── test_firmware_contract.py
│   ├── test_mock_board.py
│   └── capture_serial_evidence.py
├── docs/
│   ├── evidence/
│   ├── 评分验收清单.md
│   └── 5分钟演示讲稿.md
└── README.md
```

每个模块只承担一种职责：纯控制规则不直接访问 GPIO，硬件层不决定业务优先级，协议层不修改执行器，网页只渲染主板帧并发送白名单命令。

## 4. 验收闸门

| 闸门 | 通过条件 | 未通过时禁止 |
| --- | --- | --- |
| G0 规格确认 | 已完成 | 禁止实施未确认设计 |
| G1 软件模拟 | 固件纯逻辑、协议、模拟主板和网页测试全绿 | 禁止标记 `mock-passed` |
| G2 真板烧录前 | 串口明确、电压/共地/MQ2分压/继电器空载检查完成 | 禁止上传固件 |
| G3 真板验收 | 真 `hello/telemetry/event/ack`、所有固定 GPIO 和实物动作通过 | 禁止标记 `real-board-passed` |
| G4 满分就绪 | 100 分清单和 5 分钟单人彩排通过 | 禁止标记 `full-score-ready` |

## 5. 任务一：建立工程骨架和固定合同

### 文件

- 创建 `firmware/platformio.ini`
- 创建 `firmware/boards/n16r8_esp32s3.json`
- 创建 `firmware/include/smartlife_config.h`
- 创建 `firmware/include/smartlife_models.h`
- 创建最小可编译的 `firmware/src/main.cpp`，任务四再扩展真实硬件采样
- 创建 `tools/test_firmware_contract.py`
- 更新 `.gitignore`

### 实施步骤

1. 先写 `test_firmware_contract.py`，要求以下内容存在且唯一：
   - `project=smartlife-junior`
   - `profileId=smartlife-junior-solar-home-v1`
   - `protocolVersion=1`
   - 板卡 `n16r8_esp32s3`、波特率 `115200`、RFID 禁用。
   - 16 个固定 GPIO：1、4、14、5、10、2、8、45、9、11、12、13、46、41、42、17。
2. 运行测试并确认因文件缺失而失败。
3. 从参考工程只复制板卡 JSON 和 PlatformIO 基础参数，库版本固定为经过验证的 DHT、SSD1306、GFX、ESP32Servo、ArduinoJson、NeoPixel。
4. 在配置头文件冻结采样周期、迟滞、MQ2 映射、Sleep 风扇 `35%`、告警清除时间和开机安全初值。
5. 重新运行合同测试并编译空骨架。

### 验证命令

```bash
python3 -m unittest tools/test_firmware_contract.py
/Users/yukii/.platformio/penv/bin/pio run -d firmware
```

### 提交点

```text
chore: scaffold n16r8 solar-home firmware
```

## 6. 任务二：测试驱动实现纯控制状态机

### 文件

- 创建 `firmware/include/control_engine.h`
- 创建 `firmware/src/control_engine.cpp`
- 创建 `firmware/test/test_control_engine/test_main.cpp`
- 更新 `firmware/platformio.ini`，增加只编译纯逻辑的 `native` 测试环境

### 先写的失败测试

1. P0 上电输出：风扇关、继电器关、蜂鸣器静音、RGB 安全态。
2. Auto：`T >= YZ+0.5` 风扇 100%，`T <= YZ-0.5` 风扇 0%，迟滞区保持。
3. Sleep：风扇 35%、RGB 关闭。
4. 水浸、火焰、MQ2 告警覆盖 Auto/Sleep：风扇 100%、RGB 红、继电器关、蜂鸣器按允许状态报警。
5. 布防 PIR：RGB 红、蜂鸣器报警，但风扇不被虚构为 100%。
6. 安全期间手动执行器命令被 `safety_lock` 拒绝。
7. 安全期间 Auto/Sleep 命令只更新目标模式并标记 `deferredBy=safety`。
8. 告警解除后从当前模式和当前传感器重新计算，不恢复旧 GPIO 快照。

### 最小实现

- `ControlInputs` 只包含传感器、阈值、模式、节气、布防和健康状态。
- `ControlOutputs` 只包含执行器目标、活动告警和延迟命令标记。
- `evaluateControl()` 必须是无 GPIO、无串口、无时间阻塞的纯函数。

### 验证命令

```bash
/Users/yukii/.platformio/penv/bin/pio test -d firmware -e native
```

### 提交点

```text
feat: implement tested safety-first control engine
```

## 7. 任务三：实现旋钮映射和二十四节气表

### 文件

- 创建 `firmware/include/solar_terms.h`
- 创建 `firmware/src/solar_terms.cpp`
- 创建 `firmware/test/test_solar_terms/test_main.cpp`
- 扩展 `firmware/test/test_control_engine/test_main.cpp`

### 先写的失败测试

1. 24 个节气准确、无重复、顺序完整。
2. 小暑：建议温度 26、窗帘 80%、补光阈值 30。
3. 大寒：建议温度 24、窗帘 20%、补光阈值 45。
4. 旋钮 `0→18℃`、`4095→35℃`、中点约 `27℃`。
5. 端点死区和滤波不会导致阈值连续跳动。
6. 节气只改变建议温度、窗帘和补光，不覆盖旋钮产生的最终 `YZ`。
7. Auto 且有人且光照低于节气阈值才允许低压灯打开；Sleep 或 P1 时关闭。

### 验证命令

```bash
/Users/yukii/.platformio/penv/bin/pio test -d firmware -e native
```

### 提交点

```text
feat: add knob mapping and 24 solar-term strategies
```

## 8. 任务四：接入真实传感器和开机安全输出

### 文件

- 创建 `firmware/include/hardware_io.h`
- 创建 `firmware/src/hardware_io.cpp`
- 创建 `firmware/src/main.cpp`
- 扩展 `tools/test_firmware_contract.py`

### 实施步骤

1. 合同测试先检查所有 `pinMode`、GPIO 常量和安全初始化顺序。
2. `setup()` 第一阶段只执行安全输出，不等待传感器、不启动业务动作。
3. 快速传感器按 `200ms` 节拍采样；DHT11 独立按 `2000ms` 采样，超过 `6000ms` 无有效值才置 stale。
4. MQ2 前 `60000ms` 标记 warming，Q 为 null，不产生预热假告警。
5. GPIO17 使用多样本滤波和有效值检查；无历史有效值时 YZ 为 27℃。
6. 所有逻辑使用 `millis()` 调度，不允许长时间 `delay()` 阻塞。
7. 执行器层只接收 `ControlOutputs`，不能自行覆盖 P1 结果。

### 验证命令

```bash
python3 -m unittest tools/test_firmware_contract.py
/Users/yukii/.platformio/penv/bin/pio run -d firmware
```

### 提交点

```text
feat: wire fixed sensors and boot-safe actuators
```

## 9. 任务五：实现 OLED、A 键和本地事件

### 文件

- 创建 `firmware/include/display_controller.h`
- 创建 `firmware/src/display_controller.cpp`
- 扩展 `firmware/include/hardware_io.h`
- 扩展 `firmware/src/hardware_io.cpp`
- 扩展 `tools/test_firmware_contract.py`

### 实施步骤

1. 合同测试先锁定 OLED 评分格式：`T:数值 c`、`Q:数值 ppm`、`N:数值`、`H:数值`、`XN:数值`、`YZ:数值`。
2. 正常页固定四行；阈值页显示 XN/YZ；故障用 `--`，不得用模拟正常值填充。
3. GPIO10 A 键实现消抖和单击事件，切换顺序固定 `Auto→Sleep→Auto`。
4. A 键原始 ADC 窗口先放在可配置常量中；真板阶段用串口采样确定中心值和容差，再冻结记录。
5. GPIO17 阈值变化和 A 键切换输出 `event`，不伪装为远程命令 `ack`。

### 验证命令

```bash
python3 -m unittest tools/test_firmware_contract.py
/Users/yukii/.platformio/penv/bin/pio run -d firmware
```

### 提交点

```text
feat: add score-format oled and local controls
```

## 10. 任务六：实现单行 JSON 协议和命令幂等

### 文件

- 创建 `firmware/include/serial_protocol.h`
- 创建 `firmware/src/serial_protocol.cpp`
- 扩展 `firmware/src/main.cpp`
- 扩展 `tools/test_firmware_contract.py`

### 先写的失败测试

1. `hello` 精确包含项目、配置标识、板卡、波特率、协议版本和 RFID 状态。
2. `telemetry` 包含 seq、uptime、mode、solarTerm、sensors、thresholds、actuators、display、alerts、health 和 lastAppliedCommandId。
3. `command.id` 缺失、模式错误、节气错误、origin 错误和数值越界均返回失败 ack，且不改变状态。
4. 相同命令 ID 返回首次 ack，不重复操作。
5. P1 期间模式命令返回成功 deferred ack；手动执行器命令返回 `safety_lock`。
6. 串口每行只有一个 JSON 对象，不混入调试文字。

### 验证命令

```bash
python3 -m unittest tools/test_firmware_contract.py
/Users/yukii/.platformio/penv/bin/pio test -d firmware -e native
/Users/yukii/.platformio/penv/bin/pio run -d firmware
```

### 提交点

```text
feat: add idempotent serial json protocol
```

## 11. 任务七：建立可重复的模拟主板

### 文件

- 创建 `tools/n16r8_mock_board.py`
- 创建 `tools/test_mock_board.py`
- 创建 `tools/capture_serial_evidence.py`

### 先写的失败测试

1. 启动后输出正确 hello 和递增 seq 的 telemetry。
2. 模拟命令产生 ack，下一帧带相同 lastAppliedCommandId。
3. 模拟 A 键、旋钮、水浸、小暑/大寒事件。
4. 重复命令不重复动作。
5. 停止遥测后可验证网页 `3500ms` 离线逻辑。

### 实施原则

- 模拟器必须明确标记 `source=mock`。
- 模拟数据不能写入真板验收记录。
- `capture_serial_evidence.py` 保存原始 JSONL 和时间戳，不篡改帧内容。

### 验证命令

```bash
python3 -m unittest tools/test_mock_board.py
python3 tools/n16r8_mock_board.py --scenario water-demo
```

### 提交点

```text
test: add deterministic smartlife mock board
```

## 12. 任务八：建设本地 Web Serial 评分控制台

### 文件

- 创建 `dashboard/index.html`
- 创建 `dashboard/style.css`
- 创建 `dashboard/protocol-core.js`
- 创建 `dashboard/app.js`
- 创建 `dashboard/tests/protocol-core.test.js`
- 创建 `dashboard/tests/dashboard-contract.test.js`

### 先写的失败测试

1. hello/telemetry/event/ack 分类正确，坏 JSON 不改变最后真状态。
2. 只把 `profileId=smartlife-junior-solar-home-v1` 识别为目标主板。
3. T/Q/N/H 按评分格式输出，Q 旁明确“MQ2 等效估算”。
4. `3500ms` 无新帧后显示离线并清除实时结论。
5. 命令 UI 等待 `ack + 新 telemetry`；只收到 ack 时显示“已接受，等待实物状态”。
6. 告警只来自 `telemetry.alerts`，未知代码可见，不从 PIR 原始值伪造入侵。
7. 继电器卡片显示“GPIO 目标状态”，不写成触点反馈。
8. 390px 宽度无水平溢出，键盘可操作，颜色之外还有文字状态。

### 页面最小范围

- 连接区：选择串口、断开、真板/模拟标识、最新帧时间。
- 评分数据台：T/Q/N/H。
- 模式区：Auto/Sleep、A 键最近事件、黄色灯和风扇状态。
- 阈值区：XN/YZ 和温控迟滞解释。
- 节气区：24 节气选择及小暑/大寒对比。
- 安全区：告警原因、GPIO、当前值、阈值和真实联动摘要。
- 证据区：command、ack、lastAppliedCommandId、新 telemetry、实物待确认状态。

### 验证命令

```bash
node dashboard/tests/protocol-core.test.js
node dashboard/tests/dashboard-contract.test.js
python3 -m http.server 8767 --directory dashboard
```

### 提交点

```text
feat: build evidence-first web serial score console
```

## 13. 任务九：接入语音白名单

### 文件

- 创建 `dashboard/voice-intent-core.js`
- 创建 `dashboard/tests/voice-intent-core.test.js`
- 扩展 `dashboard/app.js`
- 扩展 `dashboard/index.html`

### 先写的失败测试

1. “打开/切换自动模式”只映射为 `mode=Auto`。
2. “打开/切换睡眠模式”只映射为 `mode=Sleep`。
3. 含糊、无关或危险话语返回 `unknown`，不发送命令。
4. 识别文字必须经过本地白名单二次过滤。
5. 语音识别成功但板端 ack 失败时，页面显示失败，不假装切换成功。

### 实施路线

- 基线使用 Chrome/Edge 的浏览器语音识别接口，识别结果只进入纯白名单解析器。
- 同时保留“文字测试”用于调试，但文档明确它不算语音评分证据。
- 浏览器不支持、权限拒绝或没有识别结果时明确显示原因，不修改模式。
- 不在仓库中保存语音 API 密钥；若现场浏览器语音不稳定，再单独设计服务器 STT 扩展，不阻塞当前本地闭环。

### 验证命令

```bash
node dashboard/tests/voice-intent-core.test.js
node dashboard/tests/dashboard-contract.test.js
```

### 提交点

```text
feat: add whitelisted voice mode control
```

## 14. 任务十：完成 G1 软件模拟验收

### 执行清单

1. 运行全部固件合同、纯逻辑、模拟主板和网页测试。
2. 本地打开 Dashboard，连接模拟主板路径。
3. 完成 Auto/Sleep、旋钮跨阈值、小暑/大寒、水浸触发与恢复。
4. 主动停止模拟遥测，确认 3.5 秒后离线。
5. 保存测试输出到 `docs/evidence/mock/`，文件名带日期时间。
6. 只有全部通过后，在验收清单中标记 `mock-passed`。

### 汇总命令

```bash
python3 -m unittest discover -s tools -p 'test_*.py'
/Users/yukii/.platformio/penv/bin/pio test -d firmware -e native
/Users/yukii/.platformio/penv/bin/pio run -d firmware
node dashboard/tests/protocol-core.test.js
node dashboard/tests/voice-intent-core.test.js
node dashboard/tests/dashboard-contract.test.js
git diff --check
```

### 提交点

```text
test: record mock-passed smartlife evidence
```

## 15. 任务十一：G2 电气预检和真板烧录

### 开始条件

- 用户已经授权执行到真板阶段。
- 通过只读枚举确定目标 `/dev/cu.usbserial-*`，不能凭旧记录猜测端口。
- 所有改线在 USB 和外部电源断开时完成。

### 预检顺序

1. 拍照记录模块、电源和固定 GPIO 标签。
2. 万用表确认 GPIO17 最大 `3.3V`。
3. 万用表确认 MQ2 分压后 AO 最大 `3.3V`。
4. 确认风扇、舵机、继电器外部电源共地。
5. 继电器不接负载，先验证高/低触发极性和启动不吸合。
6. 舵机断开机械连杆，先确认逻辑开合方向和端点。
7. 编译通过后再上传，不执行通用固件整片擦写或猜测 flash offset。

### 命令

```bash
find /dev -maxdepth 1 -type c \( -name 'cu.usbserial-*' -o -name 'cu.wchusbserial-*' -o -name 'cu.SLAB_USBtoUART*' \) -print
/Users/yukii/.platformio/penv/bin/pio run -d firmware
/Users/yukii/.platformio/penv/bin/pio run -d firmware -t upload --upload-port <已确认串口>
/Users/yukii/.platformio/penv/bin/pio device monitor -b 115200 -p <已确认串口>
```

`<已确认串口>` 是执行时必须解析的参数，不是允许原样运行的占位命令。

### 提交点

烧录本身不自动产生代码提交；只有校准常量或文档发生可审计变化时才提交。

## 16. 任务十二：真板逐模块与闭环验收

### 验收顺序

1. **启动**：无误吸合、无蜂鸣器长鸣、收到正确 hello。
2. **基础采集**：DHT11、光照、声音、PIR、MQ2、水浸、火焰、GPIO17 均有符合语义的真值或明确 health 状态。
3. **OLED**：T/Q/N/H 与电脑端一致；阈值页 XN/YZ 正确。
4. **A 键**：采样并冻结真实 A 键 ADC 窗口，验证单击序列和消抖。
5. **旋钮**：记录最小、中点、最大值；在最终 Web Serial/联网组合状态下复测稳定性。
6. **Auto/Sleep**：验证风扇 0/35/100 三种肉眼差异和黄色灯规则。
7. **节气**：小暑/大寒改变舵机和补光策略；旋钮 YZ 不被覆盖。
8. **安全**：只用水浸完成 Sleep `35→100`、RGB 红、蜂鸣器、继电器关闭和 3 秒恢复。
9. **命令闭环**：保存 command、ack、新 telemetry、lastAppliedCommandId 和实物视频时间点。
10. **离线**：关闭网页或拔掉数据连接，本地按键、旋钮和水浸规则仍工作。

### 证据文件

- 创建 `docs/evidence/real-board/接线与供电预检.md`
- 创建 `docs/evidence/real-board/传感器与执行器验收.md`
- 创建 `docs/evidence/real-board/协议闭环记录.jsonl`
- 创建 `docs/evidence/real-board/视频时间点索引.md`

只有上述项目全部通过后，才能标记 `real-board-passed`。

### 提交点

```text
test: record real-board acceptance evidence
```

## 17. 任务十三：满分材料和 5 分钟彩排

### 文件

- 创建 `docs/评分验收清单.md`
- 创建 `docs/5分钟演示讲稿.md`
- 创建 `README.md`

### 实施步骤

1. 把评分表 C5:E28 逐行映射为“要求、操作、预期、真板证据、状态”。
2. 讲稿严格使用规格中的 5 分钟时间轴。
3. 台词明确 MQ2 是等效估算、声音是相对强度、PIR 不识别人、继电器无触点反馈。
4. 完成至少三次连续单人彩排；记录每次用时、卡顿点和修正。
5. 做一次断网彩排和一次断电重启彩排。
6. 最终检查线号、松动、舵机运动空间、MQ2 预热和水浸擦干工具。
7. 只有 100 分清单无未验证项且演示连续成功，才标记 `full-score-ready`。

### 提交点

```text
docs: finalize full-score demo and acceptance pack
```

## 18. 每阶段通用规则

- 每个任务先看到目标测试失败，再做最小实现，最后运行相关测试和全量回归。
- 每个提交只包含本任务文件；提交前运行 `git diff --check` 和 `git status --short`。
- 不删除或覆盖用户的实物照片、接线记录和原始证据。
- 模拟证据保存在 `docs/evidence/mock/`，真板证据保存在 `docs/evidence/real-board/`，不得混放。
- 未通过的项目写“未通过/待实测”，不改成模糊的“基本完成”。
- 任何真板异常先断开执行器负载、回到开机安全状态，再诊断；不通过提高电压或绕过限流解决。
- 推送到 `main` 前必须保证本地提交可构建、测试状态与文档声明一致。

## 19. 首个可执行批次

用户授权“开始实施”后，只执行任务一至任务三：

1. 工程骨架和固定合同。
2. 安全优先控制状态机。
3. 旋钮映射和二十四节气表。

批次结束时交付可编译骨架、纯逻辑测试和三次独立提交，不接触真板、不烧录、不建设网页。用户审核该批次后再进入任务四。

## 20. 首批执行结果（2026-07-19）

- 任务一：固定项目身份、16 个 GPIO、时序/阈值和 N16R8 板卡合同；Python 合同测试 `6/6` 通过。
- 任务二：安全优先纯逻辑状态机；Auto 迟滞、Sleep、MQ2/火焰/水浸、布防入侵、蜂鸣器静音、安全锁和恢复测试 `9/9` 通过。
- 任务三：24 节气表、GPIO17 的 `18~35℃` 映射、中值滤波、死区及小暑/大寒联动测试 `7/7` 通过。
- PlatformIO `n16r8_esp32s3` 环境编译通过；生成的固件只属于软件构建产物，没有上传到开发板。
- 当前完成状态仍低于 `mock-passed`：硬件采样、OLED、协议帧、模拟主板、网页和真板均未进入本批次。

## 21. 任务四执行结果（2026-07-19）

- 新增真实硬件适配层，按固定 GPIO 读取光照、声音、DHT11、PIR、8 键 AD、MQ2、水浸、火焰和 GPIO17 旋钮。
- 快速传感器采用 `200ms` 非阻塞节拍；DHT11 采用 `2000ms` 节拍，并在连续 `6000ms` 无有效读数后标记失效。
- MQ2 上电前 `60000ms` 只标记预热，控制引擎不把预热读数当告警；等效 ppm 仍是演示估算值，待真板校准。
- GPIO17 使用 5 点中值滤波和 ADC 原始值死区，历史有效值建立前保持默认 `27℃`。
- 风扇、继电器、蜂鸣器、RGB 和舵机均只接收 `ControlOutputs`；上电先将风扇、继电器、蜂鸣器和 RGB 写入安全状态。
- 固件合同测试 `11/11`、纯逻辑原生测试 `16/16` 通过，PlatformIO `n16r8_esp32s3` 编译通过；固件约占 Flash `4.5%`、RAM `5.9%`。
- 本次没有上传固件。继电器高/低触发极性、风扇 PWM 方向、PIR/水浸/火焰有效电平和舵机 `20°~110°` 端点只是当前可配置默认值，必须在 G2 空载或断开机械连杆后逐项实测，不能视为真板通过。
- 当前完成状态仍低于 `mock-passed`：OLED、A 键事件、单行 JSON 协议、模拟主板、网页和全部真板证据尚未完成。

## 22. 任务五执行结果（2026-07-19）

- 新增 OLED 控制器，固定使用 `SDA=GPIO41`、`SCL=GPIO42`，软件默认地址为 `0x3C`；初始化失败时保持其他本地规则继续运行，并为后续 `health.oled` 保留 `ready()` 状态。
- 正常页固定显示模式、`T:数值 c`、`Q:数值 ppm`、`N:数值 H:数值`；DHT 无效时显示 `T:--`，MQ2 预热时显示 `Q:---- ppm`，不填充模拟正常值。
- 温度阈值发生整数变化后，OLED 在 `2500ms` 内切换到阈值页，显示 GPIO17 的 `XN` 和当前有效 `YZ`；旋钮尚未建立有效历史时支持 `XN:--` 和安全默认 `YZ:27`。
- GPIO10 A 键软件默认 ADC 窗口为 `1950~2300`，按 `60ms` 状态稳定条件消抖；一次持续按下只产生一次单击，完整释放后才允许下一次 `Auto→Sleep→Auto` 切换。
- A 键和旋钮变化进入定长本地事件队列，并输出 `type=event`；本地变化不伪装成远程命令 `ack`。完整 `hello/telemetry/event/ack` 协议归任务六统一封装。
- 固件合同测试 `15/15`、纯逻辑原生测试 `19/19` 通过，PlatformIO `n16r8_esp32s3` 编译通过；固件约占 Flash `4.9%`、RAM `6.1%`。
- 本次没有上传固件。OLED 地址、四行在真屏上的可读性、GPIO10 A 键真实 ADC 中心值和容差均是待 G2/G3 实测项；软件默认值不能视为实体通过。
- 当前完成状态仍低于 `mock-passed`：统一协议、命令幂等、模拟主板、网页和全部真板证据尚未完成。
