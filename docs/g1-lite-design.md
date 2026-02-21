# G1-lite 设计与实现说明

## 1. 目标与约束

本实现面向脚本 VM 的实时场景，目标是：

- 减少长暂停（避免一次性全堆扫描）
- 控制每帧 GC 开销（支持固定 slice 预算）
- 兼顾移动端低 CPU / 低内存设备

本版本是 **G1-lite**：

- 分代（Young / Old）
- 增量标记 + 增量清扫（按对象预算推进）
- 记忆集（remembered set）
- 写屏障（old -> young 引用跟踪）

不包含完整 G1 的 region 回收优先级模型、并发标记线程、压缩整理。

---

## 2. 数据结构

主要新增在 VM 执行上下文内：

- `gcMeta`: `objectId -> GcObjectMeta`
  - `generation`: `Young` / `Old`
  - `age`: 年龄计数，达到阈值后晋升
  - `marked`: 本轮标记位
  - `regionId`: 轻量 region 概念（按对象 id 粗分）
- `objectPtrToId`: `Object* -> objectId`
  - 支持 Ref 指针模型下的对象定位与有效性校验
- `gc`: `GcState`
  - `phase`: `Idle / MinorMark / MinorSweep / MajorMark / MajorSweep`
  - `markQueue`, `sweepList`, `rememberedSet`
  - 关键阈值：
    - `minorYoungThreshold`
    - `majorObjectThreshold`
    - `promotionAge`
    - `sliceBudgetObjects`

---

## 3. 回收流程

### 3.1 触发

- 若总对象数超过 `majorObjectThreshold`（或 `requestMajor=true`）→ 启动 Major
- 否则若 Young 对象数超过 `minorYoungThreshold` → 启动 Minor

### 3.2 标记阶段

- 从根集合出发：
  - 所有栈帧 locals / stack / constructorInstance
  - `returnValue`
- Minor 时额外使用 remembered set，把 old 持有的 young 引用纳入扫描入口
- 通过 `markQueue` 增量追踪对象子引用

### 3.3 清扫阶段

- 逐对象处理 `sweepList`：
  - 未标记对象：回收并移除元数据/映射
  - 已标记对象：清标记；Young 在 Minor 后年龄 +1，达到阈值晋升 Old

### 3.4 切片执行

- 在解释循环每步后执行 `runGcSlice(context, sliceBudgetObjects)`
- 每次只处理有限对象，避免长时间阻塞主执行路径

---

## 4. 写屏障与记忆集

写屏障规则：

- 当对象字段赋值发生 `old -> young` 引用时，将 owner 放入 `rememberedSet`

已接入路径：

- 属性写入（`StoreAttr`）
- 模块导出缓存写入（`LoadAttr` 中 module exports 懒加载）
- 列表/字典变更方法（`push/set`）
- 实例字段中 callable 回写

这样可保证 Minor GC 不全堆扫描 Old，也不会漏掉 Old 指向 Young 的存活边。

---

## 5. 与现有生命周期的兼容

- 现有语义要求 `__delete__` 在 `runDeleteHooks` 阶段运行
- 为避免提前回收脚本实例，清扫时对 `ScriptInstanceObject` 做保护：
  - 若 delete hooks 尚未跑完，先跳过该实例回收

这保证了行为兼容，但会延后部分实例释放，属于当前版本的可接受折中。

---

## 6. 默认参数建议（移动端优先）

可作为初始配置：

- `sliceBudgetObjects = 8 ~ 24`
- `minorYoungThreshold = 128 ~ 384`
- `majorObjectThreshold = 2048 ~ 8192`
- `promotionAge = 2 ~ 3`

调参建议：

- 若帧抖动明显：先降低 `sliceBudgetObjects`
- 若内存增长偏快：降低 `minorYoungThreshold`，并适当降低 `majorObjectThreshold`
- 若 Old 区膨胀快：提高 `promotionAge`

---

## 7. 当前边界与后续优化

当前边界：

- 非并发（暂停仍发生在解释循环内，但已切片）
- 无压缩（可能产生碎片）
- region 仅用于轻量分组，不做收益排序回收

建议下一步：

1. 增加 GC 统计（每帧回收数、扫描数、slice 耗时）
2. 将阈值外置为配置（脚本或启动参数）
3. 引入“暂停预算自适应”策略（根据帧耗时动态调节 `sliceBudgetObjects`）
4. 做压力脚本：大量短命对象 + 少量长寿对象，验证吞吐与峰值暂停

---

## 8. 代码映射

- GC 状态与元数据定义：`include/gs/vm.hpp`
- GC 主流程 / 写屏障 / 分配注册 / 执行切片：`src/vm.cpp`
- Ref 指针有效性保护（stale 检查）：`src/vm.cpp`

该实现已可运行并通过当前 demo 回归，后续重点应放在“指标可观测 + 参数化 + 压测调优”。
