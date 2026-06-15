class CrossbowApp {
    constructor() {
        this.crossbowTypes = [];
        this.selectedCrossbow = null;
        this.currentTrajectory = null;
        this.recentData = [];
        this.crossbow3d = null;
        this.accuracyPanel = null;
        this.config = window.CROSSBOW_CONFIG;
        this.init();
    }

    async init() {
        this.init3D();
        this.initCharts();
        this.bindEvents();
        await this.loadCrossbowTypes();
        this.startClock();
        this.startDataPolling();
    }

    init3D() {
        const container = document.getElementById("three-container");
        this.crossbow3d = new Crossbow3D(container);
    }

    initCharts() {
        this.accuracyPanel = new AccuracyPanel();
        this.accuracyPanel.createAllCharts();
    }

    bindEvents() {
        document.getElementById("crossbow-select").addEventListener("change", (e) => {
            this.selectCrossbow(parseInt(e.target.value));
        });

        document.getElementById("angle-slider").addEventListener("input", (e) => {
            document.getElementById("angle-value").textContent = parseFloat(e.target.value).toFixed(1);
        });
        document.getElementById("wind-slider").addEventListener("input", (e) => {
            document.getElementById("wind-value").textContent = parseFloat(e.target.value).toFixed(1);
        });
        document.getElementById("wind-dir-slider").addEventListener("input", (e) => {
            document.getElementById("wind-dir-value").textContent = e.target.value;
        });

        document.getElementById("btn-simulate").addEventListener("click", () => this.simulateShot());
        document.getElementById("btn-animate").addEventListener("click", () => this.crossbow3d.playDrawAnimation());
        document.getElementById("btn-run-analysis").addEventListener("click", () => this.runAccuracyAnalysis());
        document.getElementById("btn-reset-view").addEventListener("click", () => this.crossbow3d.resetView());
        document.getElementById("show-trajectory").addEventListener("change", (e) => this.crossbow3d.setShowTrajectory(e.target.checked));
        document.getElementById("show-grid").addEventListener("change", (e) => this.crossbow3d.setShowGrid(e.target.checked));
        document.getElementById("show-wireframe").addEventListener("change", (e) => this.crossbow3d.setWireframe(e.target.checked));

        document.querySelectorAll(".tab-btn").forEach(btn => {
            btn.addEventListener("click", () => {
                const tabId = btn.dataset.tab;
                document.querySelectorAll(".tab-btn").forEach(b => b.classList.remove("active"));
                document.querySelectorAll(".tab-content").forEach(c => c.classList.remove("active"));
                btn.classList.add("active");
                document.getElementById(`tab-${tabId}`).classList.add("active");
                if (tabId === "3d-view") setTimeout(() => this.crossbow3d.onResize(), 50);
            });
        });
    }

    async loadCrossbowTypes() {
        try {
            const res = await fetch(`${this.config.apiBase}/crossbow-types`);
            this.crossbowTypes = await res.json();
        } catch (e) {
            console.error("加载弩机类型失败:", e);
            this.crossbowTypes = [
                { id: 1, name: "秦弩", dynasty: "秦朝", draw_weight: 150.0, bow_length: 1.38, string_length: 1.42, arrow_mass: 0.065, effective_range: 150.0, max_range: 300.0 },
                { id: 2, name: "汉弩（蹶张）", dynasty: "汉朝", draw_weight: 180.0, bow_length: 1.45, string_length: 1.48, arrow_mass: 0.072, effective_range: 180.0, max_range: 350.0 },
                { id: 3, name: "汉弩（腰引）", dynasty: "汉朝", draw_weight: 250.0, bow_length: 1.52, string_length: 1.55, arrow_mass: 0.085, effective_range: 220.0, max_range: 450.0 },
                { id: 4, name: "三国诸葛弩", dynasty: "三国", draw_weight: 90.0, bow_length: 0.95, string_length: 0.98, arrow_mass: 0.045, effective_range: 80.0, max_range: 150.0 },
                { id: 5, name: "晋代神弩", dynasty: "晋朝", draw_weight: 300.0, bow_length: 1.68, string_length: 1.72, arrow_mass: 0.100, effective_range: 250.0, max_range: 500.0 },
                { id: 6, name: "唐伏远弩", dynasty: "唐朝", draw_weight: 200.0, bow_length: 1.55, string_length: 1.58, arrow_mass: 0.078, effective_range: 200.0, max_range: 400.0 },
                { id: 7, name: "宋神臂弩", dynasty: "宋朝", draw_weight: 350.0, bow_length: 1.75, string_length: 1.78, arrow_mass: 0.095, effective_range: 300.0, max_range: 600.0 },
                { id: 8, name: "宋克敌弓", dynasty: "宋朝", draw_weight: 280.0, bow_length: 1.62, string_length: 1.65, arrow_mass: 0.088, effective_range: 260.0, max_range: 520.0 },
                { id: 9, name: "元复合弩", dynasty: "元朝", draw_weight: 220.0, bow_length: 1.50, string_length: 1.53, arrow_mass: 0.080, effective_range: 210.0, max_range: 420.0 },
                { id: 10, name: "明三眼弩", dynasty: "明朝", draw_weight: 160.0, bow_length: 1.42, string_length: 1.45, arrow_mass: 0.068, effective_range: 160.0, max_range: 320.0 }
            ];
        }

        const select = document.getElementById("crossbow-select");
        select.innerHTML = '<option value="">请选择弩机类型</option>';
        this.crossbowTypes.forEach(ct => {
            const opt = document.createElement("option");
            opt.value = ct.id;
            opt.textContent = `${ct.name} (${ct.dynasty})`;
            select.appendChild(opt);
        });
        if (this.crossbowTypes.length > 0) {
            select.value = this.crossbowTypes[0].id;
            this.selectCrossbow(this.crossbowTypes[0].id);
        }
    }

    selectCrossbow(id) {
        this.selectedCrossbow = this.crossbowTypes.find(c => c.id === id);
        if (!this.selectedCrossbow) return;
        const info = this.selectedCrossbow;
        document.getElementById("info-dynasty").textContent = info.dynasty;
        document.getElementById("info-draw-weight").textContent = `${info.draw_weight} kg`;
        document.getElementById("info-bow-length").textContent = `${info.bow_length.toFixed(2)} m`;
        document.getElementById("info-string-length").textContent = `${info.string_length.toFixed(2)} m`;
        document.getElementById("info-arrow-mass").textContent = `${(info.arrow_mass * 1000).toFixed(1)} g`;
        document.getElementById("info-effective-range").textContent = `${info.effective_range} m`;
        document.getElementById("info-max-range").textContent = `${info.max_range} m`;
        this.crossbow3d.updateCrossbow(info);
        this.refreshData();
    }

    async simulateShot() {
        if (!this.selectedCrossbow) { alert("请先选择弩机"); return; }
        const angle = parseFloat(document.getElementById("angle-slider").value);
        const windSpeed = parseFloat(document.getElementById("wind-slider").value);
        const windDir = parseFloat(document.getElementById("wind-dir-slider").value);

        try {
            const res = await fetch(
                `${this.config.apiBase}/simulate-shot?crossbow_id=${this.selectedCrossbow.id}` +
                `&angle=${angle}&wind_speed=${windSpeed}&wind_direction=${windDir}`
            );
            const data = await res.json();
            this.applyShotResult(data);
        } catch (e) {
            console.error("模拟失败,使用本地模型:", e);
            this.simulateShotLocal(angle, windSpeed, windDir);
        }
    }

    applyShotResult(data) {
        this.currentTrajectory = data;
        this.updateRealtimeMetrics(data);
        this.crossbow3d.playShotAnimation(data.trajectory);
        this.accuracyPanel.updateTrajectoryChart(data.trajectory);
        this.accuracyPanel.updateVelocityChart(data.trajectory);
        this.accuracyPanel.arrowMass = this.selectedCrossbow.arrow_mass;
        this.accuracyPanel.updateAltitudeChart(data.trajectory);
    }

    simulateShotLocal(angle, windSpeed, windDir) {
        if (!this.selectedCrossbow) return;
        const cb = this.selectedCrossbow;
        const GRAVITY = 9.81, BOW_EFFICIENCY = 0.75, AIR_DENSITY = 1.225;
        const ARROW_DRAG_COEFFICIENT = 1.2, ARROW_REFERENCE_AREA = 0.00025;

        const totalEnergy = 0.5 * cb.draw_weight * GRAVITY * (cb.bow_length * 0.6);
        const initialVelocity = Math.sqrt(2 * totalEnergy * BOW_EFFICIENCY / cb.arrow_mass);
        const angleRad = angle * Math.PI / 180, windRad = windDir * Math.PI / 180;
        let x = 0, y = 1.5, z = 0;
        let vx = initialVelocity * Math.cos(angleRad);
        let vy = initialVelocity * Math.sin(angleRad);
        let vz = 0;
        const windVx = windSpeed * Math.cos(windRad), windVz = windSpeed * Math.sin(windRad);

        const trajectory = [];
        const dt = 0.01; let t = 0; let maxHeight = y;
        while (t < 20 && y >= 0) {
            const rvx = vx - windVx, rvy = vy, rvz = vz - windVz;
            const speed = Math.sqrt(rvx * rvx + rvy * rvy + rvz * rvz);
            let ax = 0, ay = -GRAVITY, az = 0;
            if (speed > 0.01) {
                const drag = 0.5 * AIR_DENSITY * speed * speed * ARROW_DRAG_COEFFICIENT * ARROW_REFERENCE_AREA;
                ax = -drag * rvx / speed / cb.arrow_mass;
                ay = -drag * rvy / speed / cb.arrow_mass - GRAVITY;
                az = -drag * rvz / speed / cb.arrow_mass;
            }
            trajectory.push({ t, x, y, z, vx, vy, vz });
            if (y > maxHeight) maxHeight = y;
            vx += ax * dt; vy += ay * dt; vz += az * dt;
            x += vx * dt; y += vy * dt; z += vz * dt; t += dt;
        }
        if (y < 0) y = 0;
        trajectory.push({ t, x, y, z, vx, vy, vz });

        const impactVel = Math.sqrt(vx * vx + vy * vy + vz * vz);
        this.applyShotResult({
            initial_velocity: initialVelocity, launch_angle: angle, max_height: maxHeight,
            flight_time: t, impact_point: { x, y, z }, impact_velocity: impactVel,
            kinetic_energy: 0.5 * cb.arrow_mass * impactVel * impactVel, trajectory
        });
    }

    updateRealtimeMetrics(data) {
        const range = Math.sqrt(
            data.impact_point.x * data.impact_point.x +
            data.impact_point.z * data.impact_point.z
        );
        document.getElementById("rt-velocity").textContent = data.initial_velocity.toFixed(1);
        document.getElementById("rt-range").textContent = range.toFixed(1);
        document.getElementById("rt-maxh").textContent = data.max_height.toFixed(1);
        document.getElementById("rt-time").textContent = data.flight_time.toFixed(2);
        if (this.selectedCrossbow) {
            const tension = this.selectedCrossbow.draw_weight * 9.81 * 1.1;
            const deform = this.selectedCrossbow.bow_length * 0.035;
            document.getElementById("rt-tension").textContent = tension.toFixed(0);
            document.getElementById("rt-deform").textContent = deform.toFixed(4);
        }
    }

    async runAccuracyAnalysis() {
        if (!this.selectedCrossbow) return;
        const analysis = await this.accuracyPanel.runAccuracyAnalysis(this.selectedCrossbow.id);
        if (analysis) {
            this.displayAccuracyAnalysis(analysis);
        } else {
            this.runLocalAnalysis();
        }
    }

    runLocalAnalysis() {
        if (this.recentData.length < 5) this.simulateAccuracyData();
        const spreads = this.recentData.map(d => ({ x: d.spread_x, y: d.spread_y }));
        const velocities = this.recentData.map(d => d.arrow_velocity);
        const ranges = this.recentData.map(d => d.range);
        const mean = arr => arr.reduce((a, b) => a + b, 0) / arr.length;
        const std = (arr, m) => Math.sqrt(arr.reduce((s, v) => s + (v - m) ** 2, 0) / (arr.length - 1 || 1));
        const mx = mean(spreads.map(s => s.x)), my = mean(spreads.map(s => s.y));
        const sx = std(spreads.map(s => s.x), mx), sy = std(spreads.map(s => s.y), my);
        const mv = mean(velocities), mr = mean(ranges);
        const cep = Math.sqrt(sx * sx + sy * sy) * Math.sqrt(2 * Math.log(2));
        const sightAdjustments = [];
        [50, 100, 150, 200, 250, 300, 400, 500].forEach(r =>
            sightAdjustments.push({ range: r, adjustment: (Math.random() - 0.5) * 0.005 })
        );
        this.displayAccuracyAnalysis({
            total_shots: this.recentData.length, mean_spread_x: mx, mean_spread_y: my,
            std_spread_x: sx, std_spread_y: sy, circular_error_probable: cep,
            mean_velocity: mv, std_velocity: std(velocities, mv), mean_range: mr,
            optimal_sight_scale: 0.00156, sight_adjustments: sightAdjustments
        });
    }

    simulateAccuracyData() {
        this.recentData = [];
        const sigma = (this.selectedCrossbow?.effective_range || 150) * 0.01;
        for (let i = 0; i < 50; i++) {
            this.recentData.push({
                spread_x: this.gaussRandom(0, sigma),
                spread_y: this.gaussRandom(0, sigma * 0.8),
                arrow_velocity: this.gaussRandom(70, 5),
                range: this.gaussRandom(this.selectedCrossbow?.effective_range || 150, 15)
            });
        }
    }

    gaussRandom(mean, std) {
        const u1 = Math.random(), u2 = Math.random();
        return mean + Math.sqrt(-2 * Math.log(u1)) * Math.cos(2 * Math.PI * u2) * std;
    }

    displayAccuracyAnalysis(a) {
        document.getElementById("acc-shots").textContent = a.total_shots;
        document.getElementById("acc-cep").textContent = a.circular_error_probable.toFixed(3);
        document.getElementById("acc-stdx").textContent = a.std_spread_x.toFixed(4);
        document.getElementById("acc-stdy").textContent = a.std_spread_y.toFixed(4);
        document.getElementById("acc-vel").textContent = a.mean_velocity.toFixed(1);
        document.getElementById("acc-range").textContent = a.mean_range.toFixed(0);
        const spreads = this.recentData.map(d => ({ x: d.spread_x, y: d.spread_y }));
        this.accuracyPanel.updateSpreadChart(spreads, a.circular_error_probable);
        this.accuracyPanel.updateSightAdjustChart(a.sight_adjustments);

        const recEl = document.getElementById("sight-scale-recommendation");
        let html = `<div>最佳望山刻度系数: <strong>${a.optimal_sight_scale.toFixed(5)} 密位/米</strong></div>`;
        html += `<table><thead><tr><th>射程 (m)</th><th>刻度调整 (密位)</th></tr></thead><tbody>`;
        a.sight_adjustments.forEach(adj => {
            html += `<tr><td>${adj.range}</td><td>${adj.adjustment >= 0 ? "+" : ""}${(adj.adjustment * 1000).toFixed(2)}</td></tr>`;
        });
        html += `</tbody></table>`;
        html += `<div style="margin-top:10px;color:#94a3b8;font-size:11px;">`;
        html += `分析基于 ${a.total_shots} 发射击数据，CEP=${a.circular_error_probable.toFixed(3)}m</div>`;
        recEl.innerHTML = html;
    }

    startClock() {
        const update = () => {
            const now = new Date();
            document.getElementById("system-time").textContent =
                `${String(now.getHours()).padStart(2, "0")}:${String(now.getMinutes()).padStart(2, "0")}:${String(now.getSeconds()).padStart(2, "0")}`;
        };
        update();
        setInterval(update, 1000);
    }

    startDataPolling() {
        setInterval(() => this.pollSensorData(), this.config.pollingIntervalMs);
    }

    async pollSensorData() {
        if (!this.selectedCrossbow) return;
        const data = await this.accuracyPanel.fetchSensorData(this.selectedCrossbow.id, 50);
        if (data && data.length > 0) {
            this.recentData = data;
            data.forEach(d => this.accuracyPanel.addHistoryPoint(d.timestamp || Date.now(), d.bow_string_tension, d.bow_arm_deformation));
            this.updateShotHistory(data);
            const last = data[data.length - 1];
            document.getElementById("rt-tension").textContent = last.bow_string_tension.toFixed(0);
            document.getElementById("rt-deform").textContent = last.bow_arm_deformation.toFixed(4);
        }
        const alerts = await this.accuracyPanel.fetchAlerts();
        this.updateAlerts(alerts);
        const status = await this.accuracyPanel.fetchSystemStatus();
        if (status && status.architecture) {
            const el = document.getElementById("sys-status");
            if (el) el.textContent = status.architecture;
        }
    }

    refreshData() {
        this.recentData = [];
        this.pollSensorData();
    }

    updateShotHistory(data) {
        const container = document.getElementById("shot-history");
        const recent = data.slice(-10).reverse();
        if (recent.length === 0) {
            container.innerHTML = '<div class="history-empty">暂无历史记录</div>';
            return;
        }
        container.innerHTML = recent.map(d => {
            const spread = Math.sqrt(d.spread_x * d.spread_x + d.spread_y * d.spread_y);
            const time = d.timestamp ? new Date(d.timestamp).toLocaleTimeString("zh-CN", { hour12: false }) : "";
            return `
                <div class="shot-record">
                    <div class="shot-header">
                        <span>${time}</span>
                        <span>v=${d.arrow_velocity?.toFixed(1) || 0}m/s</span>
                    </div>
                    <div class="shot-stats">
                        <span>R=${d.range?.toFixed(1) || 0}m</span>
                        <span>散布=${spread.toFixed(3)}m</span>
                    </div>
                </div>
            `;
        }).join("");
    }

    updateAlerts(alerts) {
        const container = document.getElementById("alerts-container");
        if (!alerts || alerts.length === 0) {
            container.innerHTML = '<div class="alert-empty">暂无告警</div>';
            return;
        }
        container.innerHTML = alerts.slice(0, 10).map(a => `
            <div class="alert-item ${a.severity?.toLowerCase() || 'warning'}">
                <div class="alert-title">${a.alert_type || '告警'} - ${a.crossbow_name || ''}</div>
                <div class="alert-desc">${a.message || ''}</div>
            </div>
        `).join("");
    }
}

document.addEventListener("DOMContentLoaded", () => {
    window.app = new CrossbowApp();
});
