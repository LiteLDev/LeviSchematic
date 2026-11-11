## LeviSchematic - 基岩版投影（类比litematica）

使用Levilamina客户端版本作为API提供方的Minecraft基岩版投影mod

## 技术栈

- 使用C++20
- 使用LeviLamina作为SDK

## 常用构建指令

- `xmake f --target_type=client` —— 设置编译为客户端版本
- `xmake` —— 构建指令
- `xmake repo -u` —— 更新远程包版本信息

## 编程约定

- 代码格式以仓库 `.clang-format` 为准，静态检查使用 `.clang-tidy`，命名和组织沿用所在模块

## 项目范围与构建配置

- 当前项目为 Windows x64 客户端原生动态库，构建目标名为 `LeviSchematic`；`target_type` 默认且仅允许 `client`。
- 结构文件使用 `.mcstructure`，不要将类比 Litematica 理解为已经支持 `.litematic` 格式。
- 使用 C++20、xmake 和 LeviLamina；Windows 工具链在 `xmake.lua` 中指定为 `clang-cl`，通过 `levibuildscript` 链接和打包。
- 当前 `xmake.lua` 与安装依赖锁定 LeviLamina `v26.60.4`；入口通过 SDK `ll::getLoaderVersion` 检查运行时至少为 26.60.4 且属于 26.60 系列。
- 与 CI 对齐的完整配置命令：`xmake f -a x64 -m release -p windows --target_type=client`。
- 配置完成后运行 `xmake`；需要详细构建日志时使用 `xmake -v`。`xmake repo -u` 用于更新包仓库索引，不表示已升级项目声明的依赖版本。
- 正式目标递归编译 `src/levischematic/**.cpp`。不得为绕过编译错误将构建范围缩减为入口文件或测试桩。
