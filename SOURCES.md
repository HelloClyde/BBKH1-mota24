# 资料与素材来源

本项目从 [BBK9588-mota24](https://github.com/HelloClyde/BBK9588-mota24) 的 `0.1.0-alpha.6` 移植到 H1，上游提交为 `29df390c922e9aded9ec67f62ea15b52ed0f1314`。以下保留上游的来源记录。本项目是非官方适配。

## 游戏资料与像素素材

- 原游戏：胖老鼠工作室《24层魔塔》。
- 地图、怪物属性、道具数值与像素素材核对来源：[Mota Mod 的 24 层魔塔](https://motamod.com/games/24)，发布者大化术士，上游于 2026-09-22 获取。
- 参考版本：[Mota Mod 运行版本](https://f641fb3d-a955-49c5-b019-c7bdcdf92b59.play.motamod.com/v/0bc66a1a-5f69-429b-9ba0-4a6cd2bfdcc5/)。该链接是来源记录，不是构建时下载依赖。
- 原 9588 工程的 `src/reference/` 保存地图、楼梯和属性的裁剪快照，本项目沿用其生成后的 `src/data.h`。
- 原 9588 工程的 `src/assets/` 中，`animates.png`、`enemys.png`、`items.png`、`npcs.png`、`terrains.png` 来自该参考版本的 `project/materials/materials.h5data`；`brave.png`、`ground.png`、`hero.png`、`icons.js` 来自对应的 `project/` 路径。
- `src/sprites.h` 沿用上游从 32 像素素材转换的 20 像素 RGB565 数据，H1 绘制时使用最近邻等比例放大到 24 像素。

上游于 2026-09-23 核对该版本 `project/data.js` 的 `firstData.startText` 与 `project/floors/MT0.js` 的仙子事件。参考版片头包含约 10 秒滚动叙事，仙子对白是独立事件。`src/story.h` 为适配重新编写的分页文案，不包含原版完整逐字对白。

上游于 2026-09-30 核对同一版本的 `project/enemys.js` 与 `project/functions.js`：白衣武士 `vampire=0.25`，灵法师 `vampire=1/3`，均在战前一次性扣除并向下取整；此前裁剪快照遗漏的字段已补齐，游戏规则和战斗展示共用计算。新增 NPC 对话为小屏改写。

## 字模

`src/font.h` 为 Windows 宋体栅格化生成的有限 12 像素字形数据，沿用原 9588 版的字模并补充 H1 界面所需字符。没有附带宋体字体文件，也不声称重新许可微软字体。

正常构建使用已提交的数据。补充字模可运行 `python tools/update_font.py --font PATH`，指定自行提供且有权使用的字体。该工具只补充缺字，并不会替换已有字形；替换整个字模集需要另外完成转换和小屏排版检查。

## 应用图标

`assets/mota24-icon.png` 是原 9588 项目使用内置 imagegen 新生成的像素插画，H1 版沿用该图标，并非原版游戏素材。生成提示和处理记录见 [assets/icon-design.md](assets/icon-design.md)。

## SDK 与原生接口

H1 版使用 [bbk-h1-bda-sdk](https://github.com/MrDefinition1999/bbk-h1-bda-sdk)，固定提交 `067fe072477861dfc8949d7b1a55279fb92d2548`，位于 `sdk/` 子模块。SDK 使用自身的 Apache-2.0 许可证，详见子模块内的 `LICENSE` 与 `NOTICE`。

H1 平台适配包括帧缓冲呈现、设备输入、定时器与存档接口，替换了原 9588 运行时。原生接口实现参考了移植开发工作区中的 `src/platform/game_video.h`、`gui_context.h` 和 `h1-debug/files.c`；这些路径用于保留开发来源说明，不是本仓库的构建依赖。自编写适配代码随 `src/` 提供。

## 许可边界

| 内容 | 许可或当前已知状态 |
| --- | --- |
| 自编写应用、H1 适配、构建与测试代码 | [Apache-2.0](LICENSE)，保留 HelloClyde 署名 |
| 地图与属性资料及其生成表示（`src/data.h`） | 来自上述参考版，未取得可核验的独立开源素材许可证 |
| 像素素材及其生成表示（`src/sprites.h`） | 来自上述参考版，未取得可核验的独立开源素材许可证 |
| 宋体生成的有限字模（`src/font.h`） | 不包含完整字体文件；未核实该子集重新分发的独立授权，不适用本项目的 Apache-2.0 声明 |
| SDK 子模块与工具链 | 依各项目自己的许可证使用 |

第三方游戏资料、像素素材、字体和商标的权利归各权利人，不因本项目或 SDK 的 Apache-2.0 许可证而改变。再次分发或其他用途所需权限应向原权利人核实。这里公开许可的是自编写代码，不代表随附第三方内容都获得同样授权。

仓库和发行包不包含设备固件、NAND 镜像、原机应用转储、个人存档或完整字体文件。感谢 **QQ 群「步步高电子词典游戏群」830340878**；致谢不替代许可说明。