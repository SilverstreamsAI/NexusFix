# Research Summary: Performance Libraries for NexusFix

**Date**: 2025-01-29
**Status**: Research Complete (Deep Dive)
**Last Updated**: 2025-01-29

---

## Executive Summary

本文档汇总了 5 个性能优化库的研究结果，用于指导 NexusFix 后续优化工作。

| 优先级 | 项目 | 推荐度 | 理由 |
|--------|------|--------|------|
| High | Google Highway | **强烈推荐** | 零性能损失，支持 ARM 部署 |
| High | iceoryx2 | **推荐** | 无 daemon，零拷贝 IPC |
| Medium | Disruptor++ | **推荐** | Header-only，< 50ns 延迟 |
| Low | SBE | 按需 | 与 IPC 配合使用 |
| Low | Aeron | 按需 | 仅当需要 UDP/Multicast |

---

## 1. Google Highway (TICKET_200)

### 结论: 强烈推荐

**关键发现**:
- 官方承诺 "no performance gap vs intrinsics"
- 外部评估: "most suitable SIMD library for many software projects"
- NumPy 正在考虑采用 (NEP 54)

**迁移成本**: 中等 (~600 行代码)

**收益**:
- 支持 ARM (AWS Graviton, Apple Silicon)
- 代码量减少 50% (1 个实现 vs 2-3 个)
- 未来可自动支持新指令集 (AVX-10, SVE2)

**风险**: 低
- 编译时间略增
- 已有大量生产案例 (Chrome, libjxl)

---

## 2. Zero-Copy IPC (TICKET_201)

### 结论: 推荐 iceoryx2

**对比分析**:

| 方面 | Flow-IPC | iceoryx | iceoryx2 |
|------|----------|---------|----------|
| 中央 Daemon | 不需要 | **需要** | 不需要 |
| 语言 | C++ | C++ | Rust + C++ |
| Cap'n Proto | 内置 | 无 | 无 |
| 成熟度 | 较新 | 成熟 | 较新但活跃 |
| 跨平台 | Linux only | 多平台 | 多平台 |

**推荐 iceoryx2 原因**:
1. 无需运行中央 daemon (简化部署)
2. Rust core 提供内存安全
3. 零拷贝延迟 ~25ns
4. 活跃开发 (2025 年更新)

**备选**: Flow-IPC (如需 Cap'n Proto 序列化)

---

## 3. Lock-Free Queue (TICKET_202)

### 结论: 推荐 Disruptor++

**现状分析**:
- NexusFix 已有 SPSC queue (设计良好)
- 缺少 MPSC 支持

**Disruptor++ 优势**:
- Header-only (易集成)
- 无 CAS 操作 (单生产者模式)
- 吞吐量 2500 万 msg/s
- 延迟 < 50ns

**实施建议**:
1. 保持现有 SPSC (已足够好)
2. 参考 Disruptor++ 添加 MPSC
3. 添加可配置 Wait Strategy

---

## 4. SBE Binary Encoding (TICKET_203)

### 结论: 按需实施

**适用场景**:
- 与 IPC 配合 (SBE over SHM)
- 对接 CME/Eurex 原生协议
- 二进制日志存储

**当前状态**: Schema 已设计，可随时生成代码

---

## 5. Aeron Messaging (TICKET_204)

### 结论: 仅当需要 UDP/Multicast

**决策矩阵**:

| 需求 | 推荐方案 |
|------|----------|
| 仅 IPC | iceoryx2 |
| IPC + 序列化 | Flow-IPC |
| IPC + UDP | Aeron |
| Multicast | Aeron (唯一选择) |

---

## 实施优先级

### Phase 1 (立即可做)
1. **TICKET_200**: Highway 集成
   - 收益: ARM 支持
   - 风险: 低
   - 工作量: 2-3 天

### Phase 2 (需求驱动)
2. **TICKET_201**: iceoryx2 评估
   - 前提: 有多进程架构需求
   - 工作量: 1 周 (评估 + PoC)

3. **TICKET_202**: MPSC Queue
   - 前提: 有多生产者场景
   - 工作量: 2-3 天

### Phase 3 (长期)
4. **TICKET_203**: SBE
   - 前提: TICKET_201 完成
   - 工作量: 1 周

5. **TICKET_204**: Aeron
   - 前提: 需要 UDP/Multicast
   - 工作量: 2 周

---

## 资源链接

### 官方文档
- [Highway Documentation](https://google.github.io/highway/)
- [iceoryx2 GitHub](https://github.com/eclipse-iceoryx/iceoryx2)
- [Disruptor++ GitHub](https://github.com/lewissbaker/disruptorplus)
- [SBE Wiki](https://github.com/real-logic/simple-binary-encoding/wiki)
- [Aeron Wiki](https://github.com/real-logic/aeron/wiki)

### 技术论文
- [LMAX Disruptor Paper](https://lmax-exchange.github.io/disruptor/disruptor.html)
- [Highway Design Philosophy](https://github.com/google/highway/blob/master/g3doc/design_philosophy.md)

### 社区讨论
- [Flow-IPC vs iceoryx (HN)](https://news.ycombinator.com/item?id=39996723)
- [NumPy Highway Adoption (NEP 54)](https://numpy.org/neps/nep-0054-simd-cpp-highway.html)
