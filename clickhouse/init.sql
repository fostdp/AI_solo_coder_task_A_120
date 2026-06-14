CREATE DATABASE IF NOT EXISTS crossbow_sim;

USE crossbow_sim;

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
) ENGINE = MergeTree()
ORDER BY id;

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
    wind_direction Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (crossbow_id, timestamp);

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
    lift_force Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (crossbow_id, shot_id, time_step);

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
) ENGINE = MergeTree()
ORDER BY (crossbow_id, analysis_date);

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
ORDER BY (crossbow_id, timestamp);

INSERT INTO crossbow_types (id, name, dynasty, draw_weight, bow_length, string_length, arrow_mass, effective_range, max_range) VALUES
(1, '秦弩', '秦朝', 150.0, 1.38, 1.42, 0.065, 150.0, 300.0),
(2, '汉弩（蹶张）', '汉朝', 180.0, 1.45, 1.48, 0.072, 180.0, 350.0),
(3, '汉弩（腰引）', '汉朝', 250.0, 1.52, 1.55, 0.085, 220.0, 450.0),
(4, '三国诸葛弩', '三国', 90.0, 0.95, 0.98, 0.045, 80.0, 150.0),
(5, '晋代神弩', '晋朝', 300.0, 1.68, 1.72, 0.100, 250.0, 500.0),
(6, '唐伏远弩', '唐朝', 200.0, 1.55, 1.58, 0.078, 200.0, 400.0),
(7, '宋神臂弩', '宋朝', 350.0, 1.75, 1.78, 0.095, 300.0, 600.0),
(8, '宋克敌弓', '宋朝', 280.0, 1.62, 1.65, 0.088, 260.0, 520.0),
(9, '元复合弩', '元朝', 220.0, 1.50, 1.53, 0.080, 210.0, 420.0),
(10, '明三眼弩', '明朝', 160.0, 1.42, 1.45, 0.068, 160.0, 320.0);

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
    kinetic_energy Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (crossbow_id, timestamp);
