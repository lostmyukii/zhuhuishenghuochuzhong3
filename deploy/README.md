# 静态部署说明

本目录只提供部署前配置模板，不会自动发布网站。正式运行形态是“静态 HTTPS 页面 + 评委电脑本地 Web Serial”：网页服务器不转发控制命令，不保存开发板数据，也没有 WSS、MQTT 或云端设备代理。

## 1. 构建与核验

在仓库根目录执行：

```bash
python3 tools/build_static_release.py --version 20260720-predeploy.1
python3 tools/verify_static_release.py
```

可上传目录固定为：

```text
dist/smartlife-junior-solar-home-v1/
```

上传的是该目录内的文件，不是整个仓库。`asset-manifest.json` 记录版本、最后一次修改 `dashboard/` 的源提交和每个文件的 SHA-256；发布前、发布后应分别核验。

## 2. 托管要求

- 必须使用 HTTPS。`http://localhost` 仅用于本机演练，不能作为公网地址。
- 页面需要在桌面版 Chrome 或 Edge 中打开；Safari/Firefox 不作为 Web Serial 演示浏览器。
- 串口选择必须由现场用户点击“连接 N16R8”并在浏览器授权窗口中完成，服务器不能预选或远程占用 USB 串口。
- 公网页面只能操作连接到当前电脑的开发板，不能隔空控制赛场外设备。
- 麦克风识别能力由浏览器提供，可能依赖浏览器服务；识别文字仍会在页面内经过四句白名单二次过滤。安全判断始终在 N16R8 本地执行。

## 3. 配置模板

- 自管 Nginx：使用 `nginx-static.conf.example`，替换域名、发布目录和证书路径后先运行 `nginx -t`。
- 支持 `_headers` 的静态托管：复制 `_headers.example` 为发布目录内的 `_headers`，并确认托管平台确实应用了这些响应头。
- GitHub Pages 可以托管当前纯静态文件，但不能使用仓库中的响应头模板；如果选择 Pages，需要在浏览器开发者工具中单独核对实际响应头。

## 4. 实际上线前最后停点

本仓库不会预置自动部署工作流，也不会创建 `gh-pages` 分支，以免一次普通 `git push` 意外上线。选择域名和托管平台、配置 TLS、上传发布目录均属于下一阶段的实际部署动作。
