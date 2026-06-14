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
from datetime import datetime
from typing import Dict, List

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
ARROW_DRAG_COEFFICIENT = 1.2
ARROW_REFERENCE_AREA = 0.00025


class CrossbowSimulator:
    def __init__(self, crossbow_type: Dict, host: str = "127.0.0.1", port: int = 9000,
                 seed: int = None):
        self.crossbow = crossbow_type
        self.host = host
        self.port = port
        self.shot_count = 0
        self.arm_wear = 0.0
        self.string_wear = 0.0
        if seed is not None:
            self.rng = random.Random(seed + crossbow_type["id"])
        else:
            self.rng = random.Random()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

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
                drag = 0.5 * AIR_DENSITY * speed * speed * ARROW_DRAG_COEFFICIENT * ARROW_REFERENCE_AREA
                ax = -drag * rvx / speed / self.crossbow["arrow_mass"]
                ay = -drag * rvy / speed / self.crossbow["arrow_mass"] - GRAVITY
                az = -drag * rvz / speed / self.crossbow["arrow_mass"]
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

        self.arm_wear = min(1.0, self.arm_wear + self.rng.uniform(0.0001, 0.0005))
        self.string_wear = min(1.0, self.string_wear + self.rng.uniform(0.0002, 0.0008))

        actual_draw_weight = self.crossbow["draw_weight"] * (
            1.0 + self.rng.gauss(0, 0.03) - self.arm_wear * 0.15
        )
        draw_length = self.crossbow["bow_length"] * 0.6 * (1.0 + self.rng.gauss(0, 0.02))

        initial_velocity = self.calculate_initial_velocity(actual_draw_weight, draw_length)

        aim_angle = self.rng.uniform(5.0, 35.0)
        wind_speed = self.rng.uniform(0.0, 8.0)
        wind_direction = self.rng.uniform(0.0, 360.0)
        temperature = self.rng.uniform(10.0, 35.0)
        humidity = self.rng.uniform(20.0, 80.0)

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
            "flight_time": round(trajectory["flight_time"], 3)
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

    def close(self):
        self.sock.close()


def run_single_crossbow(args, crossbow_type):
    sim = CrossbowSimulator(crossbow_type, args.host, args.port, args.seed)
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


def run_all_crossbows(args):
    simulators = [CrossbowSimulator(ct, args.host, args.port, args.seed) for ct in CROSSBOW_TYPES]
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


def main():
    parser = argparse.ArgumentParser(description="古代弩机传感器数据模拟器")
    parser.add_argument("--host", default="127.0.0.1", help="UDP接收主机 (默认: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=9000, help="UDP接收端口 (默认: 9000)")
    parser.add_argument("--interval", type=float, default=60.0,
                        help="发射间隔秒数 (默认: 60秒)")
    parser.add_argument("--id", type=int, default=None,
                        help="只模拟指定ID的弩机 (1-10)，不指定则全部模拟")
    parser.add_argument("--single-shot", action="store_true",
                        help="只发射一次，然后退出")
    parser.add_argument("--seed", type=int, default=None,
                        help="随机数种子，用于复现实验结果")
    parser.add_argument("--burst", type=int, default=None,
                        help="快速发射模式：连续发射指定次数后退出")
    parser.add_argument("--burst-interval", type=float, default=0.1,
                        help="快速发射模式下的间隔秒数 (默认: 0.1秒)")

    args = parser.parse_args()

    if args.burst:
        args.interval = args.burst_interval

    if args.id:
        crossbow = next((c for c in CROSSBOW_TYPES if c["id"] == args.id), None)
        if not crossbow:
            print(f"错误：无效的弩机ID {args.id}，范围1-10", file=sys.stderr)
            sys.exit(1)

        if args.single_shot:
            sim = CrossbowSimulator(crossbow, args.host, args.port, args.seed)
            data = sim.generate_shot_data()
            sim.send_data(data)
            print(json.dumps(data, ensure_ascii=False, indent=2))
            sim.close()
        elif args.burst:
            sim = CrossbowSimulator(crossbow, args.host, args.port, args.seed)
            print(f"[Burst] {crossbow['name']} 发射 {args.burst} 发...")
            for i in range(args.burst):
                data = sim.generate_shot_data()
                sim.send_data(data)
                if args.burst_interval > 0:
                    time.sleep(args.burst_interval)
            print(f"[Burst] 完成，共 {args.burst} 发")
            sim.close()
        else:
            run_single_crossbow(args, crossbow)
    else:
        if args.single_shot:
            print("[警告] --single-shot 需要与 --id 配合使用", file=sys.stderr)
        elif args.burst:
            print("[警告] --burst 需要与 --id 配合使用", file=sys.stderr)
        run_all_crossbows(args)


if __name__ == "__main__":
    main()
