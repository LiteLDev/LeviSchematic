# 独立回归测试

`RefactorStateTests` 同时覆盖选区 `selection.faceColor` 的默认值、RGBA 范围/非有限值校验、透明度边界、延迟发布和旧配置缺失字段补齐；不创建选区 GPU 网格。

先在仓库根目录执行客户端配置，再运行脚本。需要 Windows x64、xmake、clang-cl，以及项目已经安装的 LeviLamina v26.51.3 SDK；不需要启动 Minecraft。

```powershell
xmake f -a x64 -m release -p windows --target_type=client
powershell -ExecutionPolicy Bypass -File src-test/run-tests.ps1
```

也可以分别执行 `xmake build ProjectionStateTests` 和 `xmake run ProjectionStateTests`。七个测试目标均为 `set_default(false)`，显式取消 `NDEBUG`，因此 release 下断言仍会执行。正式 `LeviSchematic` 目标始终包含全部 `src/**.cpp`，没有用测试桩替换生产实现。

| 目标 | 实际覆盖 |
|---|---|
| UiModelTests | 页面标题、重复/空 ID、缺失动作校验、按钮和输入动作传值；文字绑定与多视口布局边界；F7 连续开关、取消打开、关闭时重开、导航请求语义；创造背包式单选标签、灰色滚动区域、编辑栏分离、选中行绑定、grid/行模板与稳定 ID；标签草稿保留、所有权环释放、翻译键唯一性；不创建原版 UI |
| SessionGateTests | 会话关闭、重入、延迟排空与借用生命周期 |
| ChunkCpuTaskTests | CPU 任务提交、完成、异常与销毁等待；完成但尚未发布的任务不占 CPU 并发槽 |
| SchematicRenderAlgorithmsTests | 负坐标、跨区块、旋转尺寸、实体边界容差；三维 AABB 最近距离、边界包含、0 不限距离、非法输入保守保留、近处优先与同距离轮转；选区/校验共用六面几何的单方块、负坐标、跨区块与交换尺寸，检查面覆盖、角点连接及朝外绕序 |
| NativeTextureVariantTests | 当前 MSVC STL / 原版纹理 variant 标签与布局解码；不创建原生纹理 |
| RefactorStateTests | 配置快照隔离、外部线程提交、非法值保留前值、延迟应用；任务取消、旧代次、提交异常、200 次取消/提交竞争；共享截止时间 |
| ProjectionStateTests | 生产坐标变换全部镜像/旋转组合、SDK 镜像枚举映射、位置编码、五类状态显示查询、维度隔离、重叠拥有者、Y 闭区间、实际 PlacementStore 的分配失败回滚、世界坐标覆写与 revision；无覆写移动复用 contentRevision，旋转/镜像及带覆写移动使局部内容失效；十万匹配方块零属性比较、实际状态缓存、区块不可用/恢复及比较异常回滚、单轮复查不回绕、跨 tick 游标与一秒冷却 |
