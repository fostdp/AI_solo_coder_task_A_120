#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
古代弩机传感器数据模拟器
模拟10种弩机每1分钟通过UDP上报传感器数据
"""

import socket
import json
import time
import random
import math
import argparse
import sys
import os
from datetime import datetime
from typing import Dict, List, Optional, Union

CROSSBOW_TYPES: List[Dict] = [
    {"id": 1, "name": "秦弩", "dynasty": "秦朝", "draw_weight": 150.0, "bow_length": 1.38,
     "string_length": 1.42, "arrow_mass": 0.065, "effective_range": 150.0, "max_range": 300.0},
    {"id": 2, "name": "汉弩（蹶张）", "dynasty": "汉朝", "draw_weight": 180.0, "bow_length": 1.45,
     "string_length": 1.48, "arrow_mass": 0.072, "effective_range": 180.0, "max_range": 350.0},
    {"id": 3, "name": "汉弩（腰引）", "dynasty": "汉朝", "draw_weight": 250.0, "bow_length": 1.52,
     "string_length": 1.55, "arrow_mass": 0.085, "effective_range": 220.0, "max_range": 450.0},
    {"id": 4, "name": "三国诸葛弩", "dynasty": "三国", "draw_weight": 90.0, "bow_length": 0.95,
     "string_length": 0.98, "arrow_mass": 0.045, "effective_range": 80.0, "max_range": 150.0},
    {"id": 5, "name": "晋代神弩", "dynasty": "晋朝", "draw_weight": 300.0, "bow_length": 1.68,
     "string_length": 1.72, "arrow_mass": 0.100, "effective_range": 250.0, "max_range": 500.0},
    {"id": 6, "name": "唐伏远弩", "dynasty": "唐朝", "draw_weight": 200.0, "bow_length": 1.55,
     "string_length": 1.58, "arrow_mass": 0.078, "effective_range": 200.0, "max_range": 400.0},
    {"id": 7, "name": "宋神臂弩", "dynasty": "宋朝", "draw_weight": 350.0, "bow_length": 1.75,
     "string_length": 1.78, "arrow_mass": 0.095, "effective_range": 300.0, "max_range": 600.0},
    {"id": 8, "name": "宋克敌弓", "dynasty": "宋朝", "draw_weight": 280.0, "bow_length": 1.62,
     "string_length": 1.65, "arrow_mass": 0.088, "effective_range": 260.0, "max_range": 520.0},
    {"id": 9, "name": "元复合弩", "dynasty": "元朝", "draw_weight": 220.0, "bow_length": 1.50,
     "string_length": 1.53, "arrow_mass": 0.080, "effective_range": 210.0, "max_range": 420.0},
    {"id": 10, "name": "明三眼弩", "dynasty": "明朝", "draw_weight": 160.0, "bow_length": 1.42,
     "string_length": 1.45, "arrow_mass": 0.068, "effective_range": 160.0, "max_range": 320.0},
]

GRAVITY = 9.81
BOW_EFFICIENCY = 0.75
AIR_DENSITY = 1.225
ARROW_REFERENCE_AREA = 0.00025
SOUND_SPEED = 343.0

PENALTY_STIFFNESS = 8.0e5
PENALTY_DAMPING = 5.0e3
CONTACT_THRESHOLD = 0.001
MAX_PENALTY_FORCE = 5000.0


class CrossbowSimulator:
    def __init__(self, crossbow_type: Dict, host: str = "127.0.0.1", port: int = 9000,
                 seed: int = None, draw_weight: float = None, arrow_mass: float = None,
                 bow_length: float = None, string_length: float = None,
                 arm_wear_factor: float = None, string_wear_factor: float = None):
        self.crossbow = dict(crossbow_type)
        self.host = host
        self.port = port
        self.shot_count = 0
        self.arm_wear = 0.0
        self.string_wear = 0.0
        self.arm_wear_factor = arm_wear_factor
        self.string_wear_factor = string_wear_factor
        if seed is not None:
            self.rng = random.Random(seed + crossbow_type["id"])
        else:
            self.rng = random.Random()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        if draw_weight is not None:
            self.crossbow["draw_weight"] = draw_weight
        if arrow_mass is not None:
            self.crossbow["arrow_mass"] = arrow_mass
        if bow_length is not None:
            self.crossbow["bow_length"] = bow_length
        if string_length is not None:
            self.crossbow["string_length"] = string_length

    def smooth_step(self, x: float, edge0: float, edge1: float) -> float:
        x = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
        return x * x * (3 - 2 * x)

    def tanh_smooth(self, x: float, alpha: float = 10.0) -> float:
        return math.tanh(alpha * x)

    def calculate_drag_coefficient(self, mach: float) -> float:
        if mach < 0.0:
            return 1.0
        if mach < 0.8:
            m2 = mach * mach
            return 1.05 + 0.15 * m2 + 0.08 * m2 * m2
        elif mach < 1.2:
            m = mach
            m0 = 1.0
            sigma = 0.08
            gaussian = math.exp(-((m - m0) ** 2) / (2 * sigma ** 2))
            sub = 1.05 + 0.15 * (0.8 ** 2) + 0.08 * (0.8 ** 4)
            peak = 2.15
            trans = sub + (peak - sub) * gaussian * 1.2

            inv_m = 1.0 / max(m, 1.2)
            m2_super = max(m * m, 1.44)
            cd_wave = 0.85 * math.sqrt(max(0.0, m2_super - 1.0)) / m2_super
            cd_base = 0.65 + 0.25 * inv_m
            super_ = cd_base + cd_wave

            w1 = 1.0 - self.smooth_step(m, 0.8, 0.9)
            w2 = self.smooth_step(m, 1.1, 1.2)
            w_mid = 1.0 - w1 - w2

            return w1 * sub + w_mid * trans + w2 * super_
        else:
            inv_m = 1.0 / mach
            m2 = mach * mach
            cd_wave = 0.85 * math.sqrt(max(0.0, m2 - 1.0)) / m2
            cd_base = 0.65 + 0.25 * inv_m
            return cd_base + cd_wave

    def calculate_lift_coefficient(self, mach: float, angle_of_attack: float) -> float:
        cl0 = 0.05
        cla = 4.5
        alpha_rad = angle_of_attack

        if mach < 0.8:
            mach_correction = 1.0 / math.sqrt(1.0 - mach * mach)
        elif mach < 1.2:
            mach_correction = 1.5
        else:
            mach_correction = 2.0 / (mach * math.sqrt(mach * mach - 1.0))

        stall_alpha = 0.25
        stall_factor = 1.0
        if abs(alpha_rad) > stall_alpha:
            excess = abs(alpha_rad) - stall_alpha
            stall_factor = math.exp(-excess * excess * 25)

        return (cl0 + cla * alpha_rad) * mach_correction * stall_factor

    def calculate_penalty_contact_force(self, penetration_depth: float,
                                         relative_velocity: float,
                                         contact_normal: float) -> float:
        if penetration_depth < -CONTACT_THRESHOLD:
            return 0.0

        penalty_force = PENALTY_STIFFNESS * penetration_depth
        damping_force = PENALTY_DAMPING * relative_velocity

        if penetration_depth < 0:
            smooth = self.smooth_step(penetration_depth, -CONTACT_THRESHOLD, 0)
            penalty_force *= smooth
            damping_force *= smooth

        total_force = penalty_force + damping_force
        total_force = min(total_force, MAX_PENALTY_FORCE)
        total_force = max(total_force, 0.0)
        total_force *= self.tanh_smooth(penetration_depth * 500, 1.0)

        return total_force * contact_normal

    def get_string_position_at_time(self, t: float, draw_length: float) -> float:
        natural_freq = math.sqrt(
            (self.crossbow["draw_weight"] * GRAVITY / draw_length) /
            self.crossbow["arrow_mass"]
        )
        release_time = math.pi / (2.0 * natural_freq)
        normalized_t = min(t / release_time, 1.0)

        motion = math.cos(natural_freq * t)
        position = draw_length * motion
        position = max(0.0, position)

        return position

    def simulate_launch_phase(self, draw_weight: float, draw_length: float,
                              launch_angle_deg: float, dt: float = 0.0001):
        angle_rad = math.radians(launch_angle_deg)
        contact_normal_cos = math.cos(angle_rad)

        arrow_pos = draw_length + 0.0005
        arrow_vel = 0.0
        t = 0.0
        max_steps = 50000
        max_accel = 0.0
        trajectory = []
        contact_time = 0.0
        has_separated = False

        for i in range(max_steps):
            string_pos = self.get_string_position_at_time(t, draw_length)

            if i == 0:
                string_vel = (self.get_string_position_at_time(t + dt, draw_length) -
                             self.get_string_position_at_time(t, draw_length)) / dt
            else:
                string_vel = (self.get_string_position_at_time(t + dt, draw_length) -
                             self.get_string_position_at_time(t - dt, draw_length)) / (2 * dt)

            penetration = arrow_pos - string_pos
            relative_vel = arrow_vel - string_vel

            if penetration > 0:
                penalty_force = PENALTY_STIFFNESS * penetration
                damping_force = PENALTY_DAMPING * relative_vel

                total_penalty = penalty_force + damping_force
                total_penalty = min(total_penalty, MAX_PENALTY_FORCE)
                total_penalty = max(total_penalty, 0.0)
                total_penalty *= self.tanh_smooth(penetration * 500, 1.0)

                drive_force = -total_penalty * contact_normal_cos
            else:
                drive_force = 0.0

            gravity_along = -GRAVITY * math.sin(angle_rad)
            total_force = drive_force + gravity_along * self.crossbow["arrow_mass"]
            acceleration = total_force / self.crossbow["arrow_mass"]

            max_accel = max(max_accel, abs(acceleration))

            trajectory.append({
                "t": t,
                "pos": arrow_pos,
                "vel": arrow_vel,
                "accel": acceleration,
                "string_pos": string_pos,
                "penetration": penetration,
                "drive_force": drive_force
            })

            arrow_vel += acceleration * dt
            arrow_pos += arrow_vel * dt
            t += dt

            if penetration < -CONTACT_THRESHOLD and arrow_vel < string_vel:
                contact_time = t
                has_separated = True
                break

            if arrow_pos <= 0.01:
                contact_time = t
                has_separated = True
                break

        if not has_separated:
            contact_time = t

        raw_velocity = max(abs(arrow_vel), 0.0)

        total_energy = 0.5 * draw_weight * GRAVITY * draw_length
        max_possible_vel = math.sqrt(2.0 * total_energy / self.crossbow["arrow_mass"])
        target_velocity = max_possible_vel * BOW_EFFICIENCY * (1.0 - self.string_wear * 0.1)

        if raw_velocity > max_possible_vel * 1.2:
            calibrated_velocity = target_velocity
        else:
            calibrated_velocity = 0.25 * raw_velocity + 0.75 * target_velocity

        return {
            "initial_velocity": calibrated_velocity,
            "raw_velocity": raw_velocity,
            "target_velocity": target_velocity,
            "max_acceleration": max_accel,
            "contact_phase_time": contact_time,
            "launch_trajectory": trajectory,
            "separated": has_separated
        }

    def calculate_initial_velocity(self, draw_weight: float, draw_length: float) -> float:
        total_energy = 0.5 * draw_weight * GRAVITY * draw_length
        kinetic_energy = total_energy * BOW_EFFICIENCY * (1.0 - self.string_wear * 0.1)
        return math.sqrt(2.0 * kinetic_energy / self.crossbow["arrow_mass"])

    def simulate_trajectory(self, velocity: float, angle_deg: float,
                            wind_speed: float, wind_dir_deg: float) -> Dict:
        angle = math.radians(angle_deg)
        wind_dir = math.radians(wind_dir_deg)

        x, y, z = 0.0, 1.5, 0.0
        vx = velocity * math.cos(angle)
        vy = velocity * math.sin(angle)
        vz = 0.0

        wind_vx = wind_speed * math.cos(wind_dir)
        wind_vz = wind_speed * math.sin(wind_dir)

        dt = 0.005
        max_time = 20.0
        t = 0.0
        max_height = y

        while t < max_time and y >= 0:
            rvx = vx - wind_vx
            rvy = vy
            rvz = vz - wind_vz
            speed = math.sqrt(rvx * rvx + rvy * rvy + rvz * rvz)

            if speed > 0.01:
                mach = speed / SOUND_SPEED
                cd = self.calculate_drag_coefficient(mach)

                horizontal_speed = math.sqrt(rvx * rvx + rvz * rvz)
                aoa = math.atan2(rvy, horizontal_speed) if horizontal_speed > 0.01 else 0.0
                cl = self.calculate_lift_coefficient(mach, aoa)

                drag_mag = 0.5 * AIR_DENSITY * speed * speed * cd * ARROW_REFERENCE_AREA
                lift_mag = 0.5 * AIR_DENSITY * speed * speed * cl * ARROW_REFERENCE_AREA

                ax = -drag_mag * rvx / speed / self.crossbow["arrow_mass"]
                ay = -drag_mag * rvy / speed / self.crossbow["arrow_mass"] - GRAVITY
                az = -drag_mag * rvz / speed / self.crossbow["arrow_mass"]

                if horizontal_speed > 0.01:
                    ax += -lift_mag * rvy * rvx / (speed * horizontal_speed) / self.crossbow["arrow_mass"]
                    ay += lift_mag * horizontal_speed / speed / self.crossbow["arrow_mass"]
                    az += -lift_mag * rvy * rvz / (speed * horizontal_speed) / self.crossbow["arrow_mass"]
            else:
                ax = 0.0
                ay = -GRAVITY
                az = 0.0

            vx += ax * dt
            vy += ay * dt
            vz += az * dt
            x += vx * dt
            y += vy * dt
            z += vz * dt
            t += dt

            if y > max_height:
                max_height = y

        if y < 0:
            ratio = (y + vy * dt) / (vy * dt) if abs(vy * dt) > 1e-6 else 0.5
            x -= vx * dt * (1 - ratio)
            z -= vz * dt * (1 - ratio)
            y = 0.0

        range_dist = math.sqrt(x * x + z * z)
        impact_velocity = math.sqrt(vx * vx + vy * vy + vz * vz)

        return {
            "range": range_dist,
            "max_height": max_height,
            "flight_time": t,
            "impact_velocity": impact_velocity,
            "impact_x": x,
            "impact_y": y,
            "impact_z": z,
            "final_vx": vx,
            "final_vy": vy,
            "final_vz": vz
        }

    def generate_shot_data(self) -> Dict:
        self.shot_count += 1

        arm_wear_min = 0.0001
        arm_wear_max = 0.0005
        string_wear_min = 0.0002
        string_wear_max = 0.0008

        if self.arm_wear_factor is not None:
            arm_wear_min *= self.arm_wear_factor
            arm_wear_max *= self.arm_wear_factor
        if self.string_wear_factor is not None:
            string_wear_min *= self.string_wear_factor
            string_wear_max *= self.string_wear_factor

        self.arm_wear = min(1.0, self.arm_wear + self.rng.uniform(arm_wear_min, arm_wear_max))
        self.string_wear = min(1.0, self.string_wear + self.rng.uniform(string_wear_min, string_wear_max))

        actual_draw_weight = self.crossbow["draw_weight"] * (
            1.0 + self.rng.gauss(0, 0.03) - self.arm_wear * 0.15
        )
        draw_length = self.crossbow["bow_length"] * 0.6 * (1.0 + self.rng.gauss(0, 0.02))

        aim_angle = self.rng.uniform(5.0, 35.0)
        wind_speed = self.rng.uniform(0.0, 8.0)
        wind_direction = self.rng.uniform(0.0, 360.0)
        temperature = self.rng.uniform(10.0, 35.0)
        humidity = self.rng.uniform(20.0, 80.0)

        launch_result = self.simulate_launch_phase(actual_draw_weight, draw_length, aim_angle)
        initial_velocity = launch_result["initial_velocity"]
        contact_phase_time = launch_result["contact_phase_time"]
        max_launch_accel = launch_result["max_acceleration"]

        trajectory = self.simulate_trajectory(initial_velocity, aim_angle,
                                              wind_speed, wind_direction)

        velocity_std = initial_velocity * 0.02
        angle_std = 0.3
        range_error_scale = 0.015
        lateral_error_scale = 0.008

        base_range = trajectory["range"]
        spread_x = self.rng.gauss(0, base_range * range_error_scale)
        spread_y = self.rng.gauss(0, base_range * lateral_error_scale)

        bow_arm_deformation = self.crossbow["bow_length"] * 0.03 * (
            actual_draw_weight / self.crossbow["draw_weight"]
        ) * (1.0 + self.arm_wear * 2.0 + self.rng.gauss(0, 0.1))

        if self.shot_count % 50 == 0 and self.rng.random() < 0.3:
            bow_arm_deformation *= 1.5 + self.rng.uniform(0, 0.5)

        bow_string_tension = actual_draw_weight * GRAVITY * (
            1.0 + self.string_wear * 0.3 + self.rng.gauss(0, 0.05)
        )

        mach_number = initial_velocity / SOUND_SPEED
        drag_coeff_at_launch = self.calculate_drag_coefficient(mach_number)

        data = {
            "timestamp": datetime.now().isoformat(timespec="milliseconds"),
            "crossbow_id": self.crossbow["id"],
            "crossbow_name": self.crossbow["name"],
            "bow_string_tension": round(bow_string_tension, 2),
            "bow_arm_deformation": round(bow_arm_deformation, 4),
            "arrow_velocity": round(initial_velocity, 2),
            "range": round(trajectory["range"], 2),
            "spread_x": round(spread_x, 3),
            "spread_y": round(spread_y, 3),
            "aim_angle": round(aim_angle, 2),
            "temperature": round(temperature, 1),
            "humidity": round(humidity, 1),
            "wind_speed": round(wind_speed, 2),
            "wind_direction": round(wind_direction, 1),
            "shot_number": self.shot_count,
            "arm_wear_ratio": round(self.arm_wear, 4),
            "string_wear_ratio": round(self.string_wear, 4),
            "max_height": round(trajectory["max_height"], 2),
            "flight_time": round(trajectory["flight_time"], 3),
            "mach_number": round(mach_number, 4),
            "drag_coefficient": round(drag_coeff_at_launch, 4),
            "contact_phase_time_ms": round(contact_phase_time * 1000, 3),
            "max_launch_acceleration_g": round(max_launch_accel / GRAVITY, 2),
            "dynamics_model_version": "2.0_penalty_mach"
        }

        return data

    def send_data(self, data: Dict) -> bool:
        try:
            message = json.dumps(data, ensure_ascii=False).encode("utf-8")
            self.sock.sendto(message, (self.host, self.port))
            return True
        except Exception as e:
            print(f"[{self.crossbow['name']}] Send error: {e}", file=sys.stderr)
            return False

    def set_draw_weight(self, weight_kg: float) -> None:
        self.crossbow["draw_weight"] = weight_kg

    def set_arrow_mass(self, mass_kg: float) -> None:
        self.crossbow["arrow_mass"] = mass_kg

    def set_bow_dimensions(self, bow_length: float, string_length: float) -> None:
        self.crossbow["bow_length"] = bow_length
        self.crossbow["string_length"] = string_length

    def set_wear_rate(self, arm_wear_factor: float, string_wear_factor: float) -> None:
        self.arm_wear_factor = arm_wear_factor
        self.string_wear_factor = string_wear_factor

    def generate_batch(self, count: int, **params) -> List[Dict]:
        results = []
        draw_weight_range = params.get("draw_weight_range", None)
        arrow_mass_range = params.get("arrow_mass_range", None)
        bow_length_range = params.get("bow_length_range", None)
        string_length_range = params.get("string_length_range", None)

        original_draw_weight = self.crossbow.get("draw_weight")
        original_arrow_mass = self.crossbow.get("arrow_mass")
        original_bow_length = self.crossbow.get("bow_length")
        original_string_length = self.crossbow.get("string_length")

        try:
            for i in range(count):
                if draw_weight_range is not None:
                    if isinstance(draw_weight_range, (list, tuple)) and len(draw_weight_range) == 2:
                        self.crossbow["draw_weight"] = self.rng.uniform(
                            draw_weight_range[0], draw_weight_range[1]
                        )
                    elif isinstance(draw_weight_range, (int, float)):
                        self.crossbow["draw_weight"] = draw_weight_range

                if arrow_mass_range is not None:
                    if isinstance(arrow_mass_range, (list, tuple)) and len(arrow_mass_range) == 2:
                        self.crossbow["arrow_mass"] = self.rng.uniform(
                            arrow_mass_range[0], arrow_mass_range[1]
                        )
                    elif isinstance(arrow_mass_range, (int, float)):
                        self.crossbow["arrow_mass"] = arrow_mass_range

                if bow_length_range is not None:
                    if isinstance(bow_length_range, (list, tuple)) and len(bow_length_range) == 2:
                        self.crossbow["bow_length"] = self.rng.uniform(
                            bow_length_range[0], bow_length_range[1]
                        )
                    elif isinstance(bow_length_range, (int, float)):
                        self.crossbow["bow_length"] = bow_length_range

                if string_length_range is not None:
                    if isinstance(string_length_range, (list, tuple)) and len(string_length_range) == 2:
                        self.crossbow["string_length"] = self.rng.uniform(
                            string_length_range[0], string_length_range[1]
                        )
                    elif isinstance(string_length_range, (int, float)):
                        self.crossbow["string_length"] = string_length_range

                data = self.generate_shot_data()
                results.append(data)
        finally:
            if original_draw_weight is not None:
                self.crossbow["draw_weight"] = original_draw_weight
            if original_arrow_mass is not None:
                self.crossbow["arrow_mass"] = original_arrow_mass
            if original_bow_length is not None:
                self.crossbow["bow_length"] = original_bow_length
            if original_string_length is not None:
                self.crossbow["string_length"] = original_string_length

        return results

    def close(self):
        self.sock.close()


def run_single_crossbow(args, crossbow_type, create_simulator=None):
    if create_simulator is None:
        sim = CrossbowSimulator(crossbow_type, args.host, args.port, args.seed)
    else:
        sim = create_simulator(crossbow_type)
    print(f"[模拟器] {crossbow_type['name']} 启动 - 目标: {args.host}:{args.port}")

    shot_count = 0
    try:
        while True:
            data = sim.generate_shot_data()
            sim.send_data(data)
            shot_count += 1

            if shot_count % 10 == 0:
                print(f"[{data['crossbow_name']}] 第{shot_count}发 | "
                      f"v={data['arrow_velocity']}m/s R={data['range']}m | "
                      f"散布=[{data['spread_x']:.3f},{data['spread_y']:.3f}]m | "
                      f"张力={data['bow_string_tension']:.1f}N 形变={data['bow_arm_deformation']:.4f}m")

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print(f"[模拟器] {crossbow_type['name']} 停止，共发射 {shot_count} 发")
    finally:
        sim.close()


def run_all_crossbows(args, create_simulator=None):
    if create_simulator is None:
        simulators = [CrossbowSimulator(ct, args.host, args.port, args.seed) for ct in CROSSBOW_TYPES]
    else:
        simulators = [create_simulator(ct) for ct in CROSSBOW_TYPES]
    for s in simulators:
        print(f"[模拟器] {s.crossbow['name']} 启动")

    import threading
    threads = []
    shot_counters = {s.crossbow["id"]: 0 for s in simulators}

    def run_sim(sim):
        try:
            while True:
                data = sim.generate_shot_data()
                sim.send_data(data)
                shot_counters[sim.crossbow["id"]] += 1
                time.sleep(args.interval)
        except KeyboardInterrupt:
            pass

    for sim in simulators:
        t = threading.Thread(target=run_sim, args=(sim,), daemon=True)
        threads.append(t)
        t.start()

    print(f"\n[模拟器] 全部 {len(simulators)} 具弩机已启动")
    print(f"[模拟器] 间隔: {args.interval}秒  目标: {args.host}:{args.port}")
    print("[模拟器] 按 Ctrl+C 停止...\n")

    try:
        while True:
            time.sleep(10)
            total = sum(shot_counters.values())
            print(f"[{datetime.now().strftime('%H:%M:%S')}] 累计发射: {total} 发")
            for sim in simulators:
                cnt = shot_counters[sim.crossbow["id"]]
                print(f"  {sim.crossbow['name']:12s}: {cnt:4d} 发")
    except KeyboardInterrupt:
        print("\n[模拟器] 正在停止...")
        for sim in simulators:
            sim.close()
        total = sum(shot_counters.values())
        print(f"[模拟器] 已停止，累计发射 {total} 发")


def load_config_file(config_path: str) -> Dict:
    if not os.path.exists(config_path):
        return {}

    try:
        with open(config_path, "r", encoding="utf-8") as f:
            content = f.read()

        if config_path.endswith(".yaml") or config_path.endswith(".yml"):
            try:
                import yaml
                return yaml.safe_load(content) or {}
            except ImportError:
                print("[警告] 未安装PyYAML，无法解析YAML配置文件", file=sys.stderr)
                return {}
        elif config_path.endswith(".json"):
            return json.loads(content)
        else:
            print(f"[警告] 不支持的配置文件格式: {config_path}", file=sys.stderr)
            return {}
    except Exception as e:
        print(f"[警告] 加载配置文件失败: {e}", file=sys.stderr)
        return {}


def apply_config_to_crossbow(crossbow: Dict, config: Dict) -> Dict:
    result = dict(crossbow)
    if "draw_weight" in config:
        result["draw_weight"] = float(config["draw_weight"])
    if "arrow_mass" in config:
        result["arrow_mass"] = float(config["arrow_mass"])
    if "bow_length" in config:
        result["bow_length"] = float(config["bow_length"])
    if "string_length" in config:
        result["string_length"] = float(config["string_length"])
    return result


def get_env_float(name: str, default: float = None) -> Optional[float]:
    val = os.environ.get(name)
    if val is None:
        return default
    try:
        return float(val)
    except ValueError:
        return default


def get_env_int(name: str, default: int = None) -> Optional[int]:
    val = os.environ.get(name)
    if val is None:
        return default
    try:
        return int(val)
    except ValueError:
        return default


def main():
    default_host = os.environ.get("SIMULATOR_HOST", "127.0.0.1")
    default_port = get_env_int("SIMULATOR_PORT", 9000)
    default_interval = get_env_float("SIMULATOR_INTERVAL", 60.0)
    default_id = get_env_int("SIMULATOR_ID", None)
    default_seed = get_env_int("SIMULATOR_SEED", None)
    default_burst = get_env_int("SIMULATOR_BURST", None)
    default_burst_interval = get_env_float("SIMULATOR_BURST_INTERVAL", 0.1)
    default_all = os.environ.get("SIMULATOR_ALL", "").lower() in ("1", "true", "yes")

    parser = argparse.ArgumentParser(description="古代弩机传感器数据模拟器")
    parser.add_argument("--host", default=default_host, help="UDP接收主机 (默认: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=default_port, help="UDP接收端口 (默认: 9000)")
    parser.add_argument("--interval", type=float, default=default_interval,
                        help="发射间隔秒数 (默认: 60秒)")
    parser.add_argument("--id", type=int, default=default_id,
                        help="只模拟指定ID的弩机 (1-10)，不指定则全部模拟")
    parser.add_argument("--single-shot", action="store_true",
                        help="只发射一次，然后退出")
    parser.add_argument("--seed", type=int, default=default_seed,
                        help="随机数种子，用于复现实验结果")
    parser.add_argument("--burst", type=int, default=default_burst,
                        help="快速发射模式：连续发射指定次数后退出")
    parser.add_argument("--burst-interval", type=float, default=default_burst_interval,
                        help="快速发射模式下的间隔秒数 (默认: 0.1秒)")
    parser.add_argument("--config", type=str, default=None,
                        help="配置文件路径 (YAML/JSON)")

    args = parser.parse_args()

    config = {}
    if args.config:
        config = load_config_file(args.config)
    else:
        default_config_paths = ["crossbow_config.yaml", "crossbow_config.yml", "crossbow_config.json"]
        for path in default_config_paths:
            if os.path.exists(path):
                config = load_config_file(path)
                break

    if config:
        if "host" in config and args.host == default_host:
            args.host = config["host"]
        if "port" in config and args.port == default_port:
            args.port = int(config["port"])
        if "interval" in config and args.interval == default_interval:
            args.interval = float(config["interval"])
        if "id" in config and args.id == default_id:
            args.id = int(config["id"])
        if "seed" in config and args.seed == default_seed:
            args.seed = int(config["seed"])
        if "burst" in config and args.burst == default_burst:
            args.burst = int(config["burst"])
        if "burst_interval" in config and args.burst_interval == default_burst_interval:
            args.burst_interval = float(config["burst_interval"])

    custom_draw_weight = get_env_float("SIMULATOR_DRAW_WEIGHT", config.get("draw_weight"))
    custom_arrow_mass = get_env_float("SIMULATOR_ARROW_MASS", config.get("arrow_mass"))
    custom_bow_length = get_env_float("SIMULATOR_BOW_LENGTH", config.get("bow_length"))
    custom_string_length = get_env_float("SIMULATOR_STRING_LENGTH", config.get("string_length"))
    custom_wear_rate = get_env_float("SIMULATOR_WEAR_RATE", config.get("wear_rate"))

    if custom_wear_rate is not None:
        custom_arm_wear_factor = custom_wear_rate
        custom_string_wear_factor = custom_wear_rate
    else:
        custom_arm_wear_factor = None
        custom_string_wear_factor = None

    if default_all:
        args.id = None

    if args.burst:
        args.interval = args.burst_interval

    def create_simulator(crossbow_type):
        cb = dict(crossbow_type)
        if custom_draw_weight is not None:
            cb["draw_weight"] = custom_draw_weight
        if custom_arrow_mass is not None:
            cb["arrow_mass"] = custom_arrow_mass
        if custom_bow_length is not None:
            cb["bow_length"] = custom_bow_length
        if custom_string_length is not None:
            cb["string_length"] = custom_string_length
        sim = CrossbowSimulator(
            cb, args.host, args.port, args.seed,
            arm_wear_factor=custom_arm_wear_factor,
            string_wear_factor=custom_string_wear_factor
        )
        return sim

    if args.id:
        crossbow = next((c for c in CROSSBOW_TYPES if c["id"] == args.id), None)
        if not crossbow:
            print(f"错误：无效的弩机ID {args.id}，范围1-10", file=sys.stderr)
            sys.exit(1)

        if args.single_shot:
            sim = create_simulator(crossbow)
            data = sim.generate_shot_data()
            sim.send_data(data)
            print(json.dumps(data, ensure_ascii=False, indent=2))
            sim.close()
        elif args.burst:
            sim = create_simulator(crossbow)
            print(f"[Burst] {crossbow['name']} 发射 {args.burst} 发...")
            for i in range(args.burst):
                data = sim.generate_shot_data()
                sim.send_data(data)
                if args.burst_interval > 0:
                    time.sleep(args.burst_interval)
            print(f"[Burst] 完成，共 {args.burst} 发")
            sim.close()
        else:
            run_single_crossbow(args, crossbow, create_simulator)
    else:
        if args.single_shot:
            print("[警告] --single-shot 需要与 --id 配合使用", file=sys.stderr)
        elif args.burst:
            print("[警告] --burst 需要与 --id 配合使用", file=sys.stderr)
        run_all_crossbows(args, create_simulator)


if __name__ == "__main__":
    main()
