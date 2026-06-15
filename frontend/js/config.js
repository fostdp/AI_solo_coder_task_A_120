window.CROSSBOW_CONFIG = {
    apiBase: "http://localhost:8080/api",
    pollingIntervalMs: 3000,

    three: {
        backgroundColor: 0x0f172a,
        fogNear: 10,
        fogFar: 50,
        cameraFov: 50,
        cameraNear: 0.1,
        cameraFar: 1000,
        cameraInitPos: { x: 5, y: 3, z: 6 },
        cameraTarget: { x: 0, y: 1, z: 0 },
        controlsDamping: 0.08,
        controlsMinDistance: 2,
        controlsMaxDistance: 30,
        controlsMaxPolarAngle: Math.PI * 0.85
    },

    crossbow: {
        armBoneCount: 6,
        armRadius: 0.035,
        armLength: 1.2,
        stringColor: 0xd1d5db,
        armColor: 0x8b4513,
        armMetalColor: 0x708090,
        bodyColor: 0x5c3d2e,
        arrowColor: 0xdeb887,
        arrowHeadColor: 0xc0c0c0
    },

    animation: {
        boneRotationLerp: 0.3,
        drawAnimationDurationMs: 800,
        releaseAnimationDurationMs: 200,
        flightAnimationSpeedScale: 2.0
    },

    charts: {
        textColor: "#cbd5e1",
        borderColor: "rgba(74, 158, 255, 0.15)",
        fontFamily: '"Microsoft YaHei", "PingFang SC", sans-serif',
        maxHistoryPoints: 100
    },

    simulation: {
        defaultLaunchAngle: 15.0,
        defaultWindSpeed: 0.0,
        defaultWindDirection: 0.0,
        arrowMassKg: 0.07
    }
};
