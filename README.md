# 古代弩机弹射动力学仿真与精准度分析系统

**v2.0 - 工程化重构版**

基于 C++ 后端 + ClickHouse 时序存储 + Three.js 前端可视化的古代弩机动力学仿真系统，
采用模块化架构与消息队列解耦，支持高精度弹道仿真、精度统计分析与实时告警推送。

---

## 目录

- [系统架构](#系统架构)
- [模块说明](#模块说明)
- [技术栈](#技术栈)
- [快速开始](#快速开始)
- [配置说明](#配置说明)
- [API 文档](#api-文档)
- [监控与告警](#监控与告警)
- [开发指南](#开发指南)

---

## 系统架构

### 总体架构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        前端 (Nginx + Gzip)                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│  │ crossbow_3d  │  │accuracy_panel│  │      config.js (外置)     │  │
│  │ Three.js 3D  │  │ Chart.js 7图 │  │  API/弩机/动画/图表配置   │  │
│  └──────┬───────┘  └──────┬───────┘  └──────────────────────────┘  │
└─────────┼─────────────────┼────────────────────────────────────────┘
          │ HTTP API        │ /metrics
┌─────────▼─────────────────▼────────────────────────────────────────┐
│                    C++ Backend (模块化 + 消息队列)                    │
│                                                                        │
│  ┌──────────────┐                                                     │
│  │ UDP Receiver │  9000/UDP  数据采集                                  │
│  └──────┬───────┘                                                     │
│         │ MessageQueue (广播扇出)                                      │
│  ┌──────┼──────┬──────────────┬──────────────┐                       │
│  │ ▼            │ ▼            │ ▼            │                       │
│  │ Ballistic    │ Accuracy     │ Alarm        │  HTTP Server  8080  │
│  │ Simulator    │ Analyzer     │ MQTT         │  REST API + Prometheus│
│  │ 动力学求解   │ 精度统计     │ 告警推送     │                       │
│  └──────┬───────┴──────┬───────┴──────┬───────┘                       │
│         │                │                │                               │
│  ┌──────▼────────────────▼────────────────▼───────┐                    │
│  │          ClickHouse Storage (TTL + MV)           │                   │
│  └───────────────────────┬─────────────────────────┘                    │
└──────────────────────────┼────────────────────────────────────────────┘
                           │
           ┌───────────────┴───────────────┐
           │                               │
  ┌────────▼─────────┐           ┌────────▼─────────┐
  │   Mosquitto      │           │   Prometheus     │
  │   MQTT Broker    │           │   Metrics + Grafana│
  └──────────────────┘           └───────────────────┘
```

### 数据流

```
UDP 传感器数据 → raw_queue → BroadcastingReceiver → 扇出 → ballistic_queue
                                                         → accuracy_queue
                                                         → alarm_queue
                                                                   ↓
                                                     MQTT Broker (告警)
                                                                   ↓
                                                     ClickHouse (持久化)
                                                                   ↓
                                                  HTTP API → 前端可视化
```

---

## 模块说明

### C++ 后端模块

| 模块 | 文件 | 职责 | 关键技术 |
|------|------|------|----------|
| **UDP Receiver** | `backend/src/udp_receiver.cpp` | UDP 端口监听、JSON 校验、数据入队 | 非阻塞Socket、环形缓冲 |
| **Ballistic Simulator** | `backend/src/ballistic_simulator.cpp` | 罚函数接触算法、马赫数三段阻力、弹道求解 | RK4积分、tanh光滑过渡 |
| **Accuracy Analyzer** | `backend/src/accuracy_analyzer_service.cpp` | CEP精度计算、望山刻度优化、线性回归 | Beasley-Springer-Moro算法 |
| **Alarm MQTT Service** | `backend/src/alarm_mqtt_service.cpp` | 三类阈值检测、10秒去抖、MQTT发布 | Paho MQTT C++ |
| **Message Queue** | `backend/include/message_queue.h` | 生产者-消费者队列、容量上限淘汰 | condition_variable、锁保护 |
| **HTTP Server** | `backend/src/http_server.cpp` | REST API 服务、跨域支持 | 原生Socket HTTP |

### 前端模块

| 模块 | 文件 | 职责 |
|------|------|------|
| **crossbow_3d.js** | `frontend/js/crossbow_3d.js` | Three.js 3D场景、骨骼动画弩臂、弹道轨迹粒子 |
| **accuracy_panel.js** | `frontend/js/accuracy_panel.js` | 7个Chart.js图表（CEP散布/望山/弹道/速度/动能/张力/形变） |
| **config.js** | `frontend/js/config.js` | 所有前端参数外置配置 |

### 基础设施

| 组件 | 版本 | 端口 | 用途 |
|------|------|------|------|
| ClickHouse | 23.8 | 8123/9000 | 时序数据存储、TTL自动清理 |
| Mosquitto | 2.0 | 1883/9001 | MQTT消息代理、告警推送 |
| Nginx | Alpine | 80 | 前端静态资源、Gzip压缩、API反向代理 |
| Prometheus | v2.48 | 9090 | 指标采集 |
| Grafana | 10.2 | 3000 | 监控可视化 |

---

## 技术栈

### 后端
- **语言**: C++17
- **构建**: CMake 3.16+
- **日志**: spdlog (多线程安全、结构化日志)
- **监控**: prometheus-cpp (pull模式, /metrics端点)
- **JSON**: nlohmann/json
- **队列**: std::mutex / Boost.Lockfree (可选)
- **存储**: ClickHouse (带 TTL 生命周期管理)
- **消息**: Eclipse Mosquitto + Paho MQTT C++

### 前端
- **3D**: Three.js r128 + OrbitControls
- **图表**: Chart.js 4.x
- **样式**: 原生 CSS + CSS Grid
- **压缩**: Nginx Gzip

### 部署
- **容器化**: Docker + docker-compose
- **CI/CD**: 多阶段构建

---

## 快速开始

### 方式一：Docker Compose 一键部署

```bash
# 克隆仓库
git clone <repo-url>
cd crossbow-simulator

# 启动核心服务（后端 + ClickHouse + MQTT + 前端）
docker-compose up -d

# 启动全部服务（含模拟器 + Prometheus + Grafana）
docker-compose --profile simulator --profile monitoring up -d

# 查看状态
docker-compose ps

# 查看日志
docker-compose logs -f backend
```

访问地址:
- 前端: http://localhost:3000
- API: http://localhost:8080/api/system-status
- Prometheus: http://localhost:9091
- Grafana: http://localhost:3001 (admin/admin)

### 方式二：本地开发

```bash
# 后端编译
cd backend
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./crossbow_backend ../config.json

# 前端（Python 简单服务器）
cd ../frontend
python3 -m http.server 3000

# 启动模拟器
cd ../simulator
python3 crossbow_simulator.py --interval 5
```

---

## 配置说明

### 后端配置 `backend/config.json`

```json
{
  "udp_port": 9000,
  "http_port": 8080,
  "metrics_port": 9090,
  "queue_capacity": 10000,
  "ballistic_threads": 2,

  "log": {
    "level": "info",
    "file": "logs/crossbow_backend.log",
    "console": true
  },

  "mqtt": {
    "broker_host": "mqtt",
    "broker_port": 1883,
    "topic_prefix": "crossbow/alerts"
  },

  "thresholds": {
    "deformation_ratio": 0.08,
    "tension_ratio": 1.5,
    "temperature_c": 60.0,
    "debounce_seconds": 10
  },

  "dynamics": {
    "gravity": 9.81,
    "air_density": 1.225,
    "bow_efficiency": 0.75,
    "sound_speed": 343.0,
    "penalty_stiffness": 800000,
    "penalty_damping": 5000
  },

  "clickhouse": {
    "host": "clickhouse",
    "port": 9000,
    "database": "crossbow_sim",
    "ttl_days": 30
  }
}
```

### 前端配置 `frontend/js/config.js`

```javascript
window.CROSSBOW_CONFIG = {
  api: { baseUrl: "http://localhost:8080/api", pollingIntervalMs: 3000 },
  three: { fov: 60, near: 0.1, far: 2000 },
  crossbow: { /* 弩机几何参数 */ },
  animation: { /* 动画参数 */ },
  chart: { /* Chart.js 样式 */ },
  simulation: { /* 默认模拟参数 */ }
};
```

---

## API 文档

### 基础路径
`http://<host>:8080/api`

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/system-status` | 系统状态、架构信息 |
| GET | `/crossbow-types` | 弩机类型列表 |
| GET | `/sensor-data?crossbow_id=1&limit=50` | 传感器历史数据 |
| GET | `/ballistic-result` | 最新弹道计算结果 |
| GET | `/accuracy?crossbow_id=1` | 精度分析结果 |
| GET | `/alerts` | 活跃告警列表 |
| POST | `/simulate-shot` | 模拟一次发射 |
| POST | `/run-analysis` | 触发精度分析 |
| POST | `/resolve-alert/:id` | 标记告警已处理 |

### Prometheus 指标

`GET http://<host>:9090/metrics`

| 指标 | 类型 | 说明 |
|------|------|------|
| `crossbow_udp_packets_total` | Counter | UDP接收包数（按状态标签） |
| `crossbow_processed_total` | Counter | 处理数据量（按模块标签） |
| `crossbow_alerts_total` | Counter | 告警数量 |
| `crossbow_queue_size` | Gauge | 队列长度（按队列标签） |
| `crossbow_ballistic_latency_ms` | Histogram | 弹道计算延迟 |
| `crossbow_http_requests_total` | Counter | HTTP请求数（按端点标签） |

---

## 监控与告警

### 三类告警阈值

| 告警类型 | 触发条件 | 严重级别 |
|----------|----------|----------|
| ARM_CRACK_RISK | 弩臂形变率 > 8% | HIGH |
| EXCESSIVE_TENSION | 弓弦张力 > 额定1.5倍 | CRITICAL |
| HIGH_TEMPERATURE | 环境温度 > 60°C | WARNING |

### ClickHouse TTL 策略

| 表名 | TTL | 说明 |
|------|-----|------|
| sensor_data | 30天 | 传感器原始数据 |
| trajectory_data | 7天 | 弹道详情（数据量大） |
| shot_records | 90天 | 射击记录 |
| accuracy_analysis | 365天 | 精度分析结果 |
| alerts | 30天 | 告警记录 |

---

## 开发指南

### 项目结构

```
.
├── backend/                    # C++ 后端
│   ├── include/               # 头文件
│   ├── src/                   # 源文件
│   ├── CMakeLists.txt         # CMake构建
│   └── config.json            # 配置文件
├── frontend/                   # 前端
│   ├── js/
│   │   ├── crossbow_3d.js     # 3D渲染模块
│   │   ├── accuracy_panel.js  # 精度面板模块
│   │   ├── config.js          # 参数外置配置
│   │   └── app.js             # 主应用
│   ├── css/style.css
│   └── index.html
├── simulator/                  # Python 模拟器
│   └── crossbow_simulator.py
├── clickhouse/                 # ClickHouse 初始化脚本
│   └── init.sql
├── deploy/                     # 部署配置
│   ├── nginx.conf
│   ├── mosquitto.conf
│   └── prometheus.yml
├── docker-compose.yml
├── Dockerfile.backend
├── Dockerfile.frontend
├── Dockerfile.simulator
└── README.md
```

### 编译选项

```bash
# 可选功能开关
cmake .. \
  -DUSE_BOOST_LOCKFREE=ON    # Boost.Lockfree 高性能队列
  -DUSE_SPDLOG=ON            # spdlog 结构化日志
  -DUSE_PROMETHEUS=ON        # Prometheus 指标
  -DUSE_CLICKHOUSE=ON        # ClickHouse 存储
  -DUSE_MQTT=ON              # MQTT 告警推送
```

### 新增模块步骤

1. 创建 `include/your_module.h` 和 `src/your_module.cpp`
2. 在 `CMakeLists.txt` 的 SOURCES/HEADERS 列表中添加
3. 在 `main.cpp` 中实例化并接入消息队列
4. 添加对应 Prometheus metrics 埋点

---

## License

MIT
