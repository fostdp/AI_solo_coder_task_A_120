class AccuracyCharts {
    constructor() {
        this.charts = {};
        Chart.defaults.color = "#cbd5e1";
        Chart.defaults.borderColor = "rgba(74, 158, 255, 0.15)";
        Chart.defaults.font.family = '"Microsoft YaHei", "PingFang SC", sans-serif';
    }

    createSpreadChart(canvasId) {
        const ctx = document.getElementById(canvasId).getContext("2d");
        this.charts[canvasId] = new Chart(ctx, {
            type: "scatter",
            data: {
                datasets: [
                    {
                        label: "弹着点散布",
                        data: [],
                        backgroundColor: "rgba(74, 158, 255, 0.6)",
                        borderColor: "#4a9eff",
                        borderWidth: 1,
                        pointRadius: 5,
                        pointHoverRadius: 7
                    },
                    {
                        label: "瞄准中心",
                        data: [{ x: 0, y: 0 }],
                        backgroundColor: "#ef4444",
                        borderColor: "#ef4444",
                        pointRadius: 8,
                        pointStyle: "crossRot",
                        borderWidth: 2
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: "弹着点散布图",
                        color: "#4a9eff",
                        font: { size: 14 }
                    },
                    legend: {
                        labels: { color: "#94a3b8", font: { size: 11 } }
                    },
                    tooltip: {
                        callbacks: {
                            label: (ctx) => `X: ${ctx.parsed.x.toFixed(3)}m, Y: ${ctx.parsed.y.toFixed(3)}m`
                        }
                    }
                },
                scales: {
                    x: {
                        title: { display: true, text: "横向偏差 (m)", color: "#94a3b8" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    },
                    y: {
                        title: { display: true, text: "纵向偏差 (m)", color: "#94a3b8" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    }
                }
            },
            plugins: [{
                id: "cepCircle",
                beforeDraw: (chart) => {
                    if (!chart.data.datasets[0].cepRadius) return;
                    const { ctx, chartArea, scales } = chart;
                    const centerX = scales.x.getPixelForValue(0);
                    const centerY = scales.y.getPixelForValue(0);
                    const radiusX = Math.abs(scales.x.getPixelForValue(chart.data.datasets[0].cepRadius) - centerX);
                    const radiusY = Math.abs(scales.y.getPixelForValue(chart.data.datasets[0].cepRadius) - centerY);
                    ctx.save();
                    ctx.beginPath();
                    ctx.ellipse(centerX, centerY, radiusX, radiusY, 0, 0, Math.PI * 2);
                    ctx.strokeStyle = "rgba(251, 191, 36, 0.6)";
                    ctx.lineWidth = 2;
                    ctx.setLineDash([5, 5]);
                    ctx.stroke();
                    ctx.fillStyle = "rgba(251, 191, 36, 0.08)";
                    ctx.fill();
                    ctx.setLineDash([]);
                    ctx.fillStyle = "#fbbf24";
                    ctx.font = "11px Consolas, monospace";
                    ctx.fillText(`CEP: ${chart.data.datasets[0].cepRadius.toFixed(3)}m`, centerX + 5, centerY - radiusY - 5);
                    ctx.restore();
                }
            }]
        });
        return this.charts[canvasId];
    }

    updateSpreadChart(canvasId, spreadData, cep) {
        const chart = this.charts[canvasId];
        if (!chart) return;

        chart.data.datasets[0].data = spreadData.map(d => ({ x: d.x, y: d.y }));
        chart.data.datasets[0].cepRadius = cep;

        let max = cep * 2;
        spreadData.forEach(d => {
            max = Math.max(max, Math.abs(d.x), Math.abs(d.y));
        });
        max *= 1.2;

        chart.options.scales.x.min = -max;
        chart.options.scales.x.max = max;
        chart.options.scales.y.min = -max;
        chart.options.scales.y.max = max;

        chart.update();
    }

    createSightAdjustChart(canvasId) {
        const ctx = document.getElementById(canvasId).getContext("2d");
        this.charts[canvasId] = new Chart(ctx, {
            type: "bar",
            data: {
                labels: [],
                datasets: [{
                    label: "望山刻度调整 (密位)",
                    data: [],
                    backgroundColor: [],
                    borderColor: "#4a9eff",
                    borderWidth: 1,
                    borderRadius: 3
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: "各射程望山刻度调整建议",
                        color: "#f4a460",
                        font: { size: 13 }
                    },
                    legend: { display: false }
                },
                scales: {
                    x: {
                        title: { display: true, text: "射程 (m)", color: "#94a3b8" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    },
                    y: {
                        title: { display: true, text: "调整量 (密位)", color: "#94a3b8" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    }
                }
            }
        });
        return this.charts[canvasId];
    }

    updateSightAdjustChart(canvasId, adjustments) {
        const chart = this.charts[canvasId];
        if (!chart) return;

        chart.data.labels = adjustments.map(a => `${a.range}`);
        chart.data.datasets[0].data = adjustments.map(a => a.adjustment * 1000);
        chart.data.datasets[0].backgroundColor = adjustments.map(a =>
            a.adjustment >= 0
                ? "rgba(74, 222, 128, 0.6)"
                : "rgba(239, 68, 68, 0.6)"
        );
        chart.update();
    }

    createTrajectoryChart(canvasId) {
        const ctx = document.getElementById(canvasId).getContext("2d");
        this.charts[canvasId] = new Chart(ctx, {
            type: "line",
            data: {
                datasets: [
                    {
                        label: "弹道高度 (m)",
                        data: [],
                        borderColor: "#4a9eff",
                        backgroundColor: "rgba(74, 158, 255, 0.1)",
                        fill: true,
                        tension: 0.3,
                        pointRadius: 0,
                        borderWidth: 2
                    },
                    {
                        label: "侧偏 (m)",
                        data: [],
                        borderColor: "#f4a460",
                        backgroundColor: "transparent",
                        tension: 0.3,
                        pointRadius: 0,
                        borderWidth: 2,
                        yAxisID: "y1"
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                interaction: { mode: "index", intersect: false },
                plugins: {
                    title: {
                        display: true,
                        text: "弹道轨迹 - 高度与侧偏",
                        color: "#4a9eff",
                        font: { size: 14 }
                    },
                    legend: {
                        labels: { color: "#94a3b8", font: { size: 11 } }
                    }
                },
                scales: {
                    x: {
                        type: "linear",
                        title: { display: true, text: "水平距离 (m)", color: "#94a3b8" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    },
                    y: {
                        type: "linear",
                        title: { display: true, text: "高度 (m)", color: "#4a9eff" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    },
                    y1: {
                        type: "linear",
                        position: "right",
                        title: { display: true, text: "侧偏 (m)", color: "#f4a460" },
                        grid: { drawOnChartArea: false },
                        ticks: { color: "#64748b" }
                    }
                }
            }
        });
        return this.charts[canvasId];
    }

    updateTrajectoryChart(canvasId, trajectory) {
        const chart = this.charts[canvasId];
        if (!chart) return;

        const horizDist = trajectory.map(p => Math.sqrt(p.x * p.x + p.z * p.z));
        chart.data.datasets[0].data = trajectory.map((p, i) => ({
            x: horizDist[i],
            y: p.y
        }));
        chart.data.datasets[1].data = trajectory.map((p, i) => ({
            x: horizDist[i],
            y: p.z
        }));

        chart.update();
    }

    createVelocityChart(canvasId) {
        const ctx = document.getElementById(canvasId).getContext("2d");
        this.charts[canvasId] = new Chart(ctx, {
            type: "line",
            data: {
                datasets: [{
                    label: "合速度 (m/s)",
                    data: [],
                    borderColor: "#4ade80",
                    backgroundColor: "rgba(74, 222, 128, 0.1)",
                    fill: true,
                    tension: 0.2,
                    pointRadius: 0,
                    borderWidth: 2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: "飞行速度变化",
                        color: "#4ade80",
                        font: { size: 12 }
                    },
                    legend: { display: false }
                },
                scales: {
                    x: {
                        type: "linear",
                        title: { display: true, text: "时间 (s)", color: "#94a3b8" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    },
                    y: {
                        title: { display: true, text: "速度 (m/s)", color: "#94a3b8" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    }
                }
            }
        });
        return this.charts[canvasId];
    }

    updateVelocityChart(canvasId, trajectory) {
        const chart = this.charts[canvasId];
        if (!chart) return;

        chart.data.datasets[0].data = trajectory.map(p => ({
            x: p.t,
            y: Math.sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz)
        }));
        chart.update();
    }

    createAltitudeChart(canvasId) {
        const ctx = document.getElementById(canvasId).getContext("2d");
        this.charts[canvasId] = new Chart(ctx, {
            type: "line",
            data: {
                datasets: [
                    {
                        label: "垂直速度 (m/s)",
                        data: [],
                        borderColor: "#a855f7",
                        backgroundColor: "transparent",
                        tension: 0.2,
                        pointRadius: 0,
                        borderWidth: 2
                    },
                    {
                        label: "动能 (J)",
                        data: [],
                        borderColor: "#ef4444",
                        backgroundColor: "transparent",
                        tension: 0.2,
                        pointRadius: 0,
                        borderWidth: 2,
                        yAxisID: "y1"
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    title: {
                        display: true,
                        text: "垂直速度与动能",
                        color: "#a855f7",
                        font: { size: 12 }
                    },
                    legend: {
                        labels: { color: "#94a3b8", font: { size: 10 } }
                    }
                },
                scales: {
                    x: {
                        type: "linear",
                        title: { display: true, text: "时间 (s)", color: "#94a3b8" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    },
                    y: {
                        title: { display: true, text: "垂直速度 (m/s)", color: "#a855f7" },
                        grid: { color: "rgba(74, 158, 255, 0.08)" },
                        ticks: { color: "#64748b" }
                    },
                    y1: {
                        type: "linear",
                        position: "right",
                        title: { display: true, text: "动能 (J)", color: "#ef4444" },
                        grid: { drawOnChartArea: false },
                        ticks: { color: "#64748b" }
                    }
                }
            }
        });
        return this.charts[canvasId];
    }

    updateAltitudeChart(canvasId, trajectory, arrowMass = 0.07) {
        const chart = this.charts[canvasId];
        if (!chart) return;

        chart.data.datasets[0].data = trajectory.map(p => ({ x: p.t, y: p.vy }));
        chart.data.datasets[1].data = trajectory.map(p => {
            const speed = Math.sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
            return { x: p.t, y: 0.5 * arrowMass * speed * speed };
        });
        chart.update();
    }

    createTensionChart(canvasId) {
        const ctx = document.getElementById(canvasId).getContext("2d");
        this.charts[canvasId] = new Chart(ctx, {
            type: "line",
            data: {
                labels: [],
                datasets: [{
                    label: "弓弦张力 (N)",
                    data: [],
                    borderColor: "#fbbf24",
                    backgroundColor: "rgba(251, 191, 36, 0.1)",
                    fill: true,
                    tension: 0.3,
                    pointRadius: 2,
                    borderWidth: 2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: { duration: 300 },
                plugins: {
                    title: {
                        display: true,
                        text: "弓弦张力趋势",
                        color: "#fbbf24",
                        font: { size: 11 }
                    },
                    legend: { display: false }
                },
                scales: {
                    x: { ticks: { display: false }, grid: { display: false } },
                    y: {
                        ticks: { color: "#64748b", font: { size: 10 } },
                        grid: { color: "rgba(74, 158, 255, 0.06)" }
                    }
                }
            }
        });
        return this.charts[canvasId];
    }

    updateTensionChart(canvasId, labels, values) {
        const chart = this.charts[canvasId];
        if (!chart) return;
        chart.data.labels = labels;
        chart.data.datasets[0].data = values;
        chart.update("none");
    }

    createDeformChart(canvasId) {
        const ctx = document.getElementById(canvasId).getContext("2d");
        this.charts[canvasId] = new Chart(ctx, {
            type: "line",
            data: {
                labels: [],
                datasets: [{
                    label: "弩臂形变 (mm)",
                    data: [],
                    borderColor: "#ef4444",
                    backgroundColor: "rgba(239, 68, 68, 0.1)",
                    fill: true,
                    tension: 0.3,
                    pointRadius: 2,
                    borderWidth: 2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: { duration: 300 },
                plugins: {
                    title: {
                        display: true,
                        text: "弩臂形变趋势",
                        color: "#ef4444",
                        font: { size: 11 }
                    },
                    legend: { display: false }
                },
                scales: {
                    x: { ticks: { display: false }, grid: { display: false } },
                    y: {
                        ticks: { color: "#64748b", font: { size: 10 } },
                        grid: { color: "rgba(74, 158, 255, 0.06)" }
                    }
                }
            }
        });
        return this.charts[canvasId];
    }

    updateDeformChart(canvasId, labels, values) {
        const chart = this.charts[canvasId];
        if (!chart) return;
        chart.data.labels = labels;
        chart.data.datasets[0].data = values;
        chart.update("none");
    }
}
