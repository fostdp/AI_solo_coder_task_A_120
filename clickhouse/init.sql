-- 弩机动力学仿真系统 - ClickHouse 初始化脚本
-- 包含 TTL 数据生命周期管理

CREATE DATABASE IF NOT EXISTS crossbow_sim;

USE crossbow_sim;

-- ==================== 弩机类型表 ====================
CREATE TABLE IF NOT EXISTS crossbow_types (
    id UInt32,
    name String,
    dynasty String,
    draw_weight Float64,
    bow_length Float64,
    string_length Float64,
    arrow_mass Float64,
    effective_range Float64,
    max_range Float64,
    created_at DateTime DEFAULT now()
) ENGINE = ReplacingMergeTree(created_at)
ORDER BY id
SETTINGS index_granularity = 8192;

-- ==================== 传感器数据表（TTL: 30天） ====================
CREATE TABLE IF NOT EXISTS sensor_data (
    timestamp DateTime64(3),
    crossbow_id UInt32,
    crossbow_name String,
    bow_string_tension Float64,
    bow_arm_deformation Float64,
    arrow_velocity Float64,
    range Float64,
    spread_x Float64,
    spread_y Float64,
    aim_angle Float64,
    temperature Float64,
    humidity Float64,
    wind_speed Float64,
    wind_direction Float64,
    mach_number Float64,
    drag_coefficient Float64,
    max_launch_acceleration_g Float64,
    contact_phase_time_ms Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (crossbow_id, timestamp)
TTL timestamp + INTERVAL 30 DAY
SETTINGS index_granularity = 8192;

-- ==================== 弹道详情表（TTL: 7天） ====================
CREATE TABLE IF NOT EXISTS trajectory_data (
    timestamp DateTime64(3),
    crossbow_id UInt32,
    shot_id UUID,
    time_step Float64,
    position_x Float64,
    position_y Float64,
    position_z Float64,
    velocity_x Float64,
    velocity_y Float64,
    velocity_z Float64,
    acceleration_x Float64,
    acceleration_y Float64,
    acceleration_z Float64,
    drag_force Float64,
    lift_force Float64,
    mach_number Float64,
    drag_coefficient Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (crossbow_id, shot_id, time_step)
TTL timestamp + INTERVAL 7 DAY
SETTINGS index_granularity = 8192;

-- ==================== 射击记录表（TTL: 90天） ====================
CREATE TABLE IF NOT EXISTS shot_records (
    timestamp DateTime64(3),
    crossbow_id UInt32,
    shot_id UUID,
    initial_velocity Float64,
    launch_angle Float64,
    max_height Float64,
    flight_time Float64,
    impact_point_x Float64,
    impact_point_y Float64,
    impact_point_z Float64,
    impact_velocity Float64,
    kinetic_energy Float64,
    mach_number Float64,
    drag_coefficient Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (crossbow_id, timestamp)
TTL timestamp + INTERVAL 90 DAY
SETTINGS index_granularity = 8192;

-- ==================== 精度分析表（TTL: 1年） ====================
CREATE TABLE IF NOT EXISTS accuracy_analysis (
    id UUID DEFAULT generateUUIDv4(),
    crossbow_id UInt32,
    crossbow_name String,
    analysis_date Date,
    total_shots UInt32,
    mean_spread_x Float64,
    mean_spread_y Float64,
    std_spread_x Float64,
    std_spread_y Float64,
    circular_error_probable Float64,
    mean_velocity Float64,
    std_velocity Float64,
    mean_range Float64,
    optimal_sight_scale Float64,
    sight_adjustments Array(Tuple(Float64, Float64)),
    created_at DateTime DEFAULT now()
) ENGINE = ReplacingMergeTree(created_at)
ORDER BY (crossbow_id, analysis_date)
TTL created_at + INTERVAL 365 DAY
SETTINGS index_granularity = 8192;

-- ==================== 告警表（TTL: 30天） ====================
CREATE TABLE IF NOT EXISTS alerts (
    id UUID DEFAULT generateUUIDv4(),
    timestamp DateTime64(3),
    crossbow_id UInt32,
    crossbow_name String,
    alert_type String,
    severity String,
    message String,
    threshold_value Float64,
    actual_value Float64,
    resolved UInt8 DEFAULT 0,
    resolved_at DateTime64(3)
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (crossbow_id, timestamp)
TTL timestamp + INTERVAL 30 DAY
SETTINGS index_granularity = 8192;

-- ==================== 物化视图：每分钟传感器聚合 ====================
CREATE MATERIALIZED VIEW IF NOT EXISTS sensor_data_1min_mv
ENGINE = SummingMergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (crossbow_id, timestamp)
AS SELECT
    toStartOfMinute(timestamp) AS timestamp,
    crossbow_id,
    count() AS shot_count,
    avg(arrow_velocity) AS avg_velocity,
    stddevPop(arrow_velocity) AS std_velocity,
    avg(`range`) AS avg_range,
    avg(bow_string_tension) AS avg_tension,
    avg(bow_arm_deformation) AS avg_deformation
FROM sensor_data
GROUP BY crossbow_id, timestamp;

-- ==================== 初始数据：弩机类型 ====================
INSERT INTO crossbow_types (id, name, dynasty, draw_weight, bow_length, string_length, arrow_mass, effective_range, max_range) VALUES
(1, '秦弩', '秦朝', 150.0, 1.38, 1.42, 0.065, 150.0, 300.0),
(2, '汉弩', '汉朝', 180.0, 1.45, 1.50, 0.068, 180.0, 350.0),
(3, '魏武卒弩', '三国', 200.0, 1.50, 1.55, 0.070, 200.0, 380.0),
(4, '诸葛连弩', '三国蜀', 120.0, 1.20, 1.25, 0.055, 100.0, 200.0),
(5, '隋大弩', '隋朝', 220.0, 1.55, 1.60, 0.072, 220.0, 400.0),
(6, '唐伏远弩', '唐朝', 250.0, 1.60, 1.65, 0.075, 250.0, 450.0),
(7, '宋神臂弩', '宋朝', 350.0, 1.75, 1.80, 0.080, 300.0, 550.0),
(8, '金铁鹞子弩', '金朝', 280.0, 1.65, 1.70, 0.077, 280.0, 480.0),
(9, '元神风弩', '元朝', 300.0, 1.68, 1.73, 0.078, 290.0, 500.0),
(10, '明三眼弩', '明朝', 260.0, 1.60, 1.65, 0.076, 260.0, 460.0);
