# .levischematic 文件格式

## 目的

取代原来“多区域 = 每区域一个 `.mcstructure` + `.schemset.json` 清单 + `*.regions-<ticks>/` 代次目录”的保存方式。新格式是单个文件，元数据与 `.litematic` 的 `Metadata` 对齐，并保留原 `.schemset.json` 的子区域信息（id、名称、偏移）。

选用自定义容器而不是 7z/ZIP 的原因：

- 7z 需要引入 LZMA SDK 并手写 7z 写入器；7z 的目录表在文件末尾且通常压缩，浏览器预览要先解压目录。
- 本容器的 header JSON 位于文件开头且不压缩，原理图浏览器只读前缀与 header 即可显示名称、作者、时间、区域数、方块数和尺寸，不解压任何区域数据。
- 区域数据用 gzip，由 LeviLamina `ll::utils::compress_utils` 提供，不新增依赖。

## 字节布局（小端）

| 偏移 | 长度 | 内容 |
| --- | --- | --- |
| 0 | 8 | 魔数 `LVSCHEM\0` |
| 8 | 2 | 容器版本 `u16`，当前为 1 |
| 10 | 2 | 标志 `u16`，当前为 0 |
| 12 | 4 | header 长度 `u32`（1 ~ 16 MiB） |
| 16 | headerLength | header JSON（UTF-8，未压缩） |
| 16 + headerLength | … | payload：各条目字节依次拼接 |

## header JSON

```json
{
  "format": "levischematic",
  "version": 1,
  "metadata": {
    "name": "Example",
    "author": "Player",
    "description": "",
    "timeCreated": 1763700000000,
    "timeModified": 1763700000000,
    "regionCount": 2,
    "totalBlocks": 1234,
    "totalVolume": 4096,
    "enclosingSize": [16, 16, 16],
    "generator": "LeviSchematic",
    "formatVersion": 1
  },
  "origin": [100, 64, -20],
  "regions": [
    {"id": "1", "name": "Main", "offset": [0, 0, 0], "size": [16, 8, 16], "blocks": 1000, "entry": 0},
    {"id": "2", "name": "Tower", "offset": [4, 8, 4], "size": [8, 8, 8], "blocks": 234, "entry": 1}
  ],
  "entries": [
    {"offset": 0, "length": 5120, "rawLength": 40960, "crc32": 305419896, "compression": "gzip"},
    {"offset": 5120, "length": 900, "rawLength": 9000, "crc32": 2596069104, "compression": "gzip"}
  ]
}
```

- `metadata`：字段与 litematic `Metadata` 对应。时间为毫秒；`totalBlocks` 统计第一层非 `air`/`structure_void`；`totalVolume` 为各区域体积之和；`formatVersion` 为区域 mcstructure 的 `format_version`。
- `origin`：保存时的世界原点，选区有自定义原点时为该原点，否则为所有区域最小角。只作记录，加载时不使用。
- `regions[i].offset`：相对全部区域最小角，不为负。`entry` 指向 `entries` 下标。
- `entries`：由写入器生成。`offset` 相对 payload 起点；`crc32` 对解压后的原始 mcstructure 字节计算（IEEE 802.3）。
- 区域内容是原版小端 NBT 的 mcstructure，gzip 压缩。

## 读写规则

- 读取时依次校验魔数、版本、header 长度、`format` 字段、每个条目的边界（不得越过文件末尾），解压时限制为 `rawLength` 并核对 CRC；任何不一致都按损坏文件报错。
- 浏览器预览只调用 `container::readHeader`，不读 payload。
- 修改名称/作者（原理图管理器）只替换 header 并刷新 `timeModified`，payload 原样复制。
- 写入使用 `util::writeAtomic`，取消或失败不会留下半个文件。

## 保存与加载格式

| 扩展名 | 加载 | 保存 |
| --- | --- | --- |
| `.levischematic` | 支持，多区域 | 默认；不写扩展名时使用 |
| `.litematic` | 支持，多区域 | 支持，多区域 |
| `.mcstructure` | 仅单个文件 | 支持；多区域时先确认，再合并为单个结构（外包盒，空隙为 structure_void，重叠处后面的区域覆盖前面的区域） |
| `.schemset.json` | 不再支持 | 不再支持 |

`/schem convert` 的源和目标都使用以上三种格式。

## 代码位置

- 容器层（纯数据，可独立测试）：`src/levischematic/schematic/io/LeviSchematicContainer.{h,cpp}`
- 元数据与区域读写：`src/levischematic/schematic/io/LeviSchematicFile.{h,cpp}`
- 多区域合并为 mcstructure：`src/levischematic/schematic/io/StructureMerge.{h,cpp}`、`MergeExtent.h`
- 选区保存：`src/levischematic/selection/service/SelectionSaveService.cpp`。所有区域先逐个采集到内存，全部完成后在后台写成一个文件。
- 资源保存：`src/levischematic/schematic/io/SchematicSetIO.cpp::saveResource`
