# 构建与维护

以 `extension.json` 记录的稳定版 Weasel 为基线，保留完整上游 Git 历史和 GPLv3 许可。

## 本地构建

需要 Windows x64、PowerShell 7、Python 3.12、7-Zip、Visual Studio 2022 Build Tools 的 MSVC x64/ATL 和 Windows SDK。
首次准备 Boost 建议使用 ASCII 工作目录。脚本固定 Boost 1.84.0 和 librime 1.13.1 的下载及 SHA-256。

```powershell
./tools/prepare-native.ps1
./tools/build-local.ps1 native-test
./tools/build-local.ps1 first-key-test
./tools/build-local.ps1 tsf
./tools/build-local.ps1 server
./tools/build-manager.ps1
python -m pip install -r tools/requirements-skin.txt
python -m unittest discover -s tests -p 'test_*.py' -v
python tools/make-test-skin.py --output work/test-skin
./work/skin_native.exe "$PWD/work/test-skin/active.ini" "$PWD/work"
./tools/package-extension.ps1
```

包输出在 `work/packages`。发布时保留对应源码 commit；本仓库不提交本机皮肤、配置或二进制输出。
原生测试使用自行生成的几何图片。首次按键测试参数见 `tests/skin_first_key.cpp`；
它测量 UI 初始化和刷新，不能替代从按键到上屏的全链路延迟。

## 上游同步

`sync-upstream.yml` 每周检查官方最新稳定版，也可手动触发。
成功合并时创建草稿 PR；冲突时撤销合并并创建 Issue，列出冲突文件。
不会自动合入主分支或发布版本。仓库 Actions 设置需要允许工作流创建 PR。
自动同步生成的分支会显式触发 `skin-checks.yml`，避免 GITHUB_TOKEN 创建 PR 后不自动运行检查。

每次更新应核对：依赖版本、TSF/IPC 接口、候选窗布局、安装与恢复、真实应用中 DLL 加载、
导入与默认配色、多个 DPI、冷启动与连续组词占用。更新 `extension.json` 仅表示候选基线，
不能代替人工验收。通用性能改动可独立整理向上游提交。

## 组件

- `tools/import_sogou_skin.py`：SSF 校验与静态 H1 转换。
- `tools/skin_store.py`：皮肤库、原子激活和撤销。
- `tools/skin_manager.py`：按需运行的管理窗口，不参与按键处理。
- `WeaselUI/SogouSkin.h`：配置、图片及拉伸缓存。
- `WeaselUI/WeaselPanel.*`：背景绘制、布局边距与点击区域。
- `tools/install-extension.ps1`：版本校验、独立安装目录与注册恢复。

## 性能验证边界

开发机的独立 UI 测试中，冷启动由约 193–217 ms 降至 35–41 ms，私有内存增量由约 52 MiB 降至约 3.8 MiB。
主要改动为软件 D2D 目标、文字格式复用、背景缓存与组词间保留绘制缓冲。
1000 次组词循环的 GDI 对象增量为 0。这是同一开发机的阶段性测量，不是跨硬件保证，
不代表整个输入法只占 3.8 MiB。云端 CI 用于功能回归，性能趋势需在固定机器复测。
