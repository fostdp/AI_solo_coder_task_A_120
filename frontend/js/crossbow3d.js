class Crossbow3D {
    constructor(container) {
        this.container = container;
        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.controls = null;
        this.crossbowGroup = null;
        this.stringLeft = null;
        this.stringRight = null;
        this.arrow = null;
        this.trajectoryLine = null;
        this.arrowPath = null;
        this.animating = false;
        this.currentCrossbow = null;
        this.showTrajectory = true;
        this.showGrid = true;
        this.wireframe = false;

        this.leftArmSkinned = null;
        this.rightArmSkinned = null;
        this.leftArmBones = [];
        this.rightArmBones = [];
        this.leftSkeletonHelper = null;
        this.rightSkeletonHelper = null;
        this.leftBowTip = null;
        this.rightBowTip = null;

        this.armBoneCount = 6;
        this.currentDrawAmount = 0;
        this.targetDrawAmount = 0;

        this.init();
    }

    init() {
        const rect = this.container.getBoundingClientRect();

        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x0f172a);
        this.scene.fog = new THREE.Fog(0x0f172a, 10, 50);

        this.camera = new THREE.PerspectiveCamera(50, rect.width / rect.height, 0.1, 1000);
        this.camera.position.set(5, 3, 6);

        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.setSize(rect.width, rect.height);
        this.renderer.setPixelRatio(window.devicePixelRatio);
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        this.container.appendChild(this.renderer.domElement);

        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.08;
        this.controls.minDistance = 2;
        this.controls.maxDistance = 30;
        this.controls.maxPolarAngle = Math.PI * 0.85;
        this.controls.target.set(0, 1, 0);

        this.setupLights();
        this.setupGround();
        this.createCrossbow();

        window.addEventListener("resize", () => this.onResize());
        this.animate();
    }

    setupLights() {
        const ambient = new THREE.AmbientLight(0x404080, 0.5);
        this.scene.add(ambient);

        const mainLight = new THREE.DirectionalLight(0xffffff, 1.0);
        mainLight.position.set(5, 10, 7);
        mainLight.castShadow = true;
        mainLight.shadow.mapSize.width = 2048;
        mainLight.shadow.mapSize.height = 2048;
        mainLight.shadow.camera.near = 0.5;
        mainLight.shadow.camera.far = 50;
        mainLight.shadow.camera.left = -10;
        mainLight.shadow.camera.right = 10;
        mainLight.shadow.camera.top = 10;
        mainLight.shadow.camera.bottom = -10;
        this.scene.add(mainLight);

        const fillLight = new THREE.DirectionalLight(0x4a9eff, 0.3);
        fillLight.position.set(-5, 5, -5);
        this.scene.add(fillLight);

        const warmLight = new THREE.PointLight(0xf4a460, 0.5, 15);
        warmLight.position.set(0, 2, 0);
        this.scene.add(warmLight);
    }

    setupGround() {
        const groundGeo = new THREE.PlaneGeometry(50, 50);
        const groundMat = new THREE.MeshStandardMaterial({
            color: 0x1a2744,
            roughness: 0.9,
            metalness: 0.1
        });
        const ground = new THREE.Mesh(groundGeo, groundMat);
        ground.rotation.x = -Math.PI / 2;
        ground.receiveShadow = true;
        this.scene.add(ground);

        this.gridHelper = new THREE.GridHelper(40, 40, 0x4a9eff, 0x1e3a5f);
        this.gridHelper.material.opacity = 0.3;
        this.gridHelper.material.transparent = true;
        this.scene.add(this.gridHelper);

        const axesHelper = new THREE.AxesHelper(2);
        axesHelper.position.y = 0.01;
        this.scene.add(axesHelper);

        this.impactMarker = this.createImpactMarker();
        this.scene.add(this.impactMarker);
        this.impactMarker.visible = false;
    }

    createImpactMarker() {
        const group = new THREE.Group();

        const ringGeo = new THREE.RingGeometry(0.3, 0.35, 32);
        const ringMat = new THREE.MeshBasicMaterial({
            color: 0xef4444,
            side: THREE.DoubleSide,
            transparent: true,
            opacity: 0.8
        });
        const ring1 = new THREE.Mesh(ringGeo, ringMat);
        ring1.rotation.x = -Math.PI / 2;
        ring1.position.y = 0.02;
        group.add(ring1);

        const ring2 = new THREE.Mesh(
            new THREE.RingGeometry(0.15, 0.18, 32),
            new THREE.MeshBasicMaterial({
                color: 0xfbbf24,
                side: THREE.DoubleSide,
                transparent: true,
                opacity: 0.9
            })
        );
        ring2.rotation.x = -Math.PI / 2;
        ring2.position.y = 0.02;
        group.add(ring2);

        const center = new THREE.Mesh(
            new THREE.CircleGeometry(0.05, 16),
            new THREE.MeshBasicMaterial({
                color: 0x4ade80,
                side: THREE.DoubleSide,
                transparent: true,
                opacity: 0.9
            })
        );
        center.rotation.x = -Math.PI / 2;
        center.position.y = 0.02;
        group.add(center);

        return group;
    }

    createSkinnedBowArm(armLength, radius, scale, isLeft) {
        const boneCount = this.armBoneCount;
        const segmentLength = armLength / boneCount;

        const geometry = new THREE.CylinderGeometry(
            radius, radius * 0.85,
            armLength,
            12, boneCount,
            false
        );

        const position = geometry.attributes.position;
        const skinIndices = [];
        const skinWeights = [];

        for (let i = 0; i < position.count; i++) {
            const y = position.getY(i);
            const normalizedY = (y + armLength / 2) / armLength;
            const boneIndex = Math.min(boneCount - 1, Math.floor(normalizedY * boneCount));

            let blend = 0;
            if (boneIndex < boneCount - 1) {
                const localY = (normalizedY * boneCount) - boneIndex;
                blend = localY;
            }

            const idx0 = boneIndex;
            const idx1 = Math.min(boneCount - 1, boneIndex + 1);
            const w0 = 1 - blend;
            const w1 = blend;

            skinIndices.push(idx0, idx1, 0, 0);
            skinWeights.push(w0, w1, 0, 0);
        }

        geometry.setAttribute('skinIndex', new THREE.Uint16BufferAttribute(skinIndices, 4));
        geometry.setAttribute('skinWeight', new THREE.Float32BufferAttribute(skinWeights, 4));

        const material = new THREE.MeshStandardMaterial({
            color: 0xc4a882,
            roughness: 0.5,
            metalness: 0.2,
            skinning: true
        });

        const bones = [];
        let prevBone = new THREE.Bone();
        prevBone.position.y = -armLength / 2;
        bones.push(prevBone);

        for (let i = 1; i < boneCount; i++) {
            const bone = new THREE.Bone();
            bone.position.y = segmentLength;
            prevBone.add(bone);
            bones.push(bone);
            prevBone = bone;
        }

        const skeleton = new THREE.Skeleton(bones);
        const skinnedMesh = new THREE.SkinnedMesh(geometry, material);
        skinnedMesh.castShadow = true;
        skinnedMesh.receiveShadow = true;
        skinnedMesh.add(bones[0]);
        skinnedMesh.bind(skeleton);

        const tipBone = bones[bones.length - 1];
        const tip = new THREE.Mesh(
            new THREE.SphereGeometry(0.04 * scale, 12, 12),
            new THREE.MeshStandardMaterial({
                color: 0x8899aa,
                roughness: 0.35,
                metalness: 0.85
            })
        );
        tip.castShadow = true;
        tipBone.add(tip);

        return {
            skinnedMesh,
            bones,
            skeleton,
            tip,
            tipBone
        };
    }

    getBowArmBendingAngles(drawAmount, maxDraw, boneCount, armLength) {
        const angles = [];
        const normalizedDraw = Math.min(Math.max(drawAmount / maxDraw, 0), 1);
        const maxAngle = Math.PI / 4;

        const cumulativeAngle = normalizedDraw * maxAngle;

        for (let i = 0; i < boneCount; i++) {
            const bonePos = i / (boneCount - 1);
            const angleFactor = Math.sin(Math.PI * 0.5 * bonePos);
            const boneAngle = cumulativeAngle * angleFactor * 0.5;
            angles.push(boneAngle);
        }

        return angles;
    }

    updateArmBones(bones, isLeft, drawAmount, maxDraw) {
        const angles = this.getBowArmBendingAngles(drawAmount, maxDraw, bones.length, 1.0);

        for (let i = 0; i < bones.length; i++) {
            const targetAngle = isLeft ? -angles[i] : angles[i];

            const currentAngle = bones[i].rotation.z;
            const newAngle = currentAngle + (targetAngle - currentAngle) * 0.3;

            bones[i].rotation.z = newAngle;

            if (i > 0) {
                bones[i].position.y = bones[i - 1].position.y + (1.0 / bones.length);
            }
        }
    }

    getBowTipPosition(bones, isLeft, armLength, drawAmount) {
        const angle = this.getBowArmBendingAngles(drawAmount, armLength * 0.6, 10, armLength);
        let totalAngle = angle.reduce((a, b) => a + b, 0) * (isLeft ? -1 : 1);
        totalAngle *= 0.8;

        const basePos = new THREE.Vector3(
            isLeft ? -armLength / 2 : armLength / 2,
            0,
            0
        );

        const tipOffset = new THREE.Vector3(
            isLeft ? -armLength / 2 : armLength / 2,
            Math.sin(Math.abs(totalAngle)) * armLength * 0.25,
            0
        );

        return tipOffset;
    }

    createCrossbow(crossbowData = null) {
        if (this.crossbowGroup) {
            this.scene.remove(this.crossbowGroup);
        }

        this.currentCrossbow = crossbowData;

        const scale = crossbowData ? Math.min(crossbowData.bow_length / 1.4, 1.2) : 1.0;
        const bowLength = crossbowData ? crossbowData.bow_length : 1.4;
        const stringLength = crossbowData ? crossbowData.string_length : 1.45;

        this.bowLength = bowLength;
        this.bowScale = scale;
        this.maxDraw = bowLength * 0.6 * scale;

        this.crossbowGroup = new THREE.Group();

        const woodMat = new THREE.MeshStandardMaterial({
            color: 0x8b5a2b,
            roughness: 0.7,
            metalness: 0.15
        });

        const darkWoodMat = new THREE.MeshStandardMaterial({
            color: 0x5c3317,
            roughness: 0.8,
            metalness: 0.1
        });

        const metalMat = new THREE.MeshStandardMaterial({
            color: 0x8899aa,
            roughness: 0.35,
            metalness: 0.85
        });

        const stockLen = 1.2 * scale;
        const stockGeo = new THREE.BoxGeometry(0.08 * scale, 0.06 * scale, stockLen);
        const stock = new THREE.Mesh(stockGeo, woodMat);
        stock.castShadow = true;
        stock.receiveShadow = true;
        this.crossbowGroup.add(stock);

        const gripGeo = new THREE.BoxGeometry(0.1 * scale, 0.2 * scale, 0.12 * scale);
        const grip = new THREE.Mesh(gripGeo, darkWoodMat);
        grip.position.y = -0.12 * scale;
        grip.position.z = 0.15 * scale;
        grip.castShadow = true;
        this.crossbowGroup.add(grip);

        const triggerGuardGeo = new THREE.TorusGeometry(0.05 * scale, 0.015 * scale, 8, 16, Math.PI);
        const triggerGuard = new THREE.Mesh(triggerGuardGeo, metalMat);
        triggerGuard.position.y = -0.08 * scale;
        triggerGuard.position.z = 0.18 * scale;
        triggerGuard.rotation.x = Math.PI;
        this.crossbowGroup.add(triggerGuard);

        const bowArmLen = bowLength / 2 * scale;
        const bowArmRadius = 0.035 * scale;

        const leftArmResult = this.createSkinnedBowArm(bowArmLen, bowArmRadius, scale, true);
        leftArmResult.skinnedMesh.position.z = -stockLen / 2 + 0.05;
        leftArmResult.skinnedMesh.rotation.z = -Math.PI / 12;
        this.crossbowGroup.add(leftArmResult.skinnedMesh);

        const rightArmResult = this.createSkinnedBowArm(bowArmLen, bowArmRadius, scale, false);
        rightArmResult.skinnedMesh.position.z = -stockLen / 2 + 0.05;
        rightArmResult.skinnedMesh.rotation.z = Math.PI / 12;
        this.crossbowGroup.add(rightArmResult.skinnedMesh);

        this.leftArmSkinned = leftArmResult.skinnedMesh;
        this.rightArmSkinned = rightArmResult.skinnedMesh;
        this.leftArmBones = leftArmResult.bones;
        this.rightArmBones = rightArmResult.bones;
        this.leftBowTip = leftArmResult.tip;
        this.rightBowTip = rightArmResult.tip;

        this.stringAnchorL = new THREE.Vector3(-bowArmLen * 0.95, bowArmLen * 0.15, -stockLen / 2 + 0.05);
        this.stringAnchorR = new THREE.Vector3(bowArmLen * 0.95, bowArmLen * 0.15, -stockLen / 2 + 0.05);

        const stringMat = new THREE.LineBasicMaterial({
            color: 0xd4c4a8,
            linewidth: 2
        });

        this.stringRestZ = -stockLen / 2 + 0.15;
        this.stringDrawnZ = 0.2;

        const stringGeoL = new THREE.BufferGeometry().setFromPoints([
            this.stringAnchorL,
            new THREE.Vector3(0, 0, this.stringRestZ)
        ]);
        this.stringLeft = new THREE.Line(stringGeoL, stringMat);
        this.crossbowGroup.add(this.stringLeft);

        const stringGeoR = new THREE.BufferGeometry().setFromPoints([
            this.stringAnchorR,
            new THREE.Vector3(0, 0, this.stringRestZ)
        ]);
        this.stringRight = new THREE.Line(stringGeoR, stringMat);
        this.crossbowGroup.add(this.stringRight);

        this.createArrow(scale);

        const sightHeight = 0.12 * scale;
        const sightPoleGeo = new THREE.CylinderGeometry(0.005 * scale, 0.005 * scale, sightHeight, 8);
        const sightPole = new THREE.Mesh(sightPoleGeo, metalMat);
        sightPole.position.y = sightHeight / 2;
        sightPole.position.z = 0.1;
        this.crossbowGroup.add(sightPole);

        const sightPinGeo = new THREE.BoxGeometry(0.02 * scale, 0.003 * scale, 0.003 * scale);
        for (let i = 0; i < 5; i++) {
            const pin = new THREE.Mesh(sightPinGeo, metalMat);
            pin.position.y = 0.03 * scale + i * 0.02 * scale;
            pin.position.z = 0.1;
            this.crossbowGroup.add(pin);
        }

        this.crossbowGroup.position.y = 1.2;
        this.crossbowGroup.rotation.y = Math.PI / 6;
        this.scene.add(this.crossbowGroup);

        this.currentDrawAmount = 0;
        this.targetDrawAmount = 0;
        this.updateArmDeformation(0);
        this.setStringPosition(0, 0, this.stringRestZ);
    }

    updateArmDeformation(drawAmount) {
        this.targetDrawAmount = drawAmount;

        const smoothDraw = this.currentDrawAmount + (this.targetDrawAmount - this.currentDrawAmount) * 0.15;
        this.currentDrawAmount = smoothDraw;

        this.updateArmBones(this.leftArmBones, true, smoothDraw, this.maxDraw);
        this.updateArmBones(this.rightArmBones, false, smoothDraw, this.maxDraw);

        this.updateStringAnchors();
    }

    updateStringAnchors() {
        const worldPosL = new THREE.Vector3();
        const worldPosR = new THREE.Vector3();

        if (this.leftBowTip) {
            this.leftBowTip.getWorldPosition(worldPosL);
            const localL = this.crossbowGroup.worldToLocal(worldPosL.clone());
            this.stringAnchorL.copy(localL);
        }

        if (this.rightBowTip) {
            this.rightBowTip.getWorldPosition(worldPosR);
            const localR = this.crossbowGroup.worldToLocal(worldPosR.clone());
            this.stringAnchorR.copy(localR);
        }
    }

    createArrow(scale) {
        const arrowGroup = new THREE.Group();

        const shaftLen = 0.7 * scale;
        const shaftGeo = new THREE.CylinderGeometry(0.005 * scale, 0.005 * scale, shaftLen, 8);
        const shaftMat = new THREE.MeshStandardMaterial({
            color: 0xb8860b,
            roughness: 0.7,
            metalness: 0.1
        });
        const shaft = new THREE.Mesh(shaftGeo, shaftMat);
        shaft.rotation.x = Math.PI / 2;
        shaft.position.z = -shaftLen / 2;
        shaft.castShadow = true;
        arrowGroup.add(shaft);

        const headGeo = new THREE.ConeGeometry(0.015 * scale, 0.06 * scale, 8);
        const headMat = new THREE.MeshStandardMaterial({
            color: 0x777777,
            roughness: 0.3,
            metalness: 0.9
        });
        const head = new THREE.Mesh(headGeo, headMat);
        head.rotation.x = -Math.PI / 2;
        head.position.z = -shaftLen - 0.03 * scale;
        head.castShadow = true;
        arrowGroup.add(head);

        const fletchingMat = new THREE.MeshStandardMaterial({
            color: 0xdc2626,
            roughness: 0.8,
            side: THREE.DoubleSide,
            transparent: true,
            opacity: 0.9
        });
        for (let i = 0; i < 3; i++) {
            const fletchGeo = new THREE.PlaneGeometry(0.04 * scale, 0.02 * scale);
            const fletch = new THREE.Mesh(fletchGeo, fletchingMat);
            fletch.position.z = 0.02 * scale;
            fletch.rotation.z = (i * Math.PI * 2) / 3;
            fletch.rotation.y = Math.PI / 2;
            const radius = 0.015 * scale;
            fletch.position.x = Math.cos(i * Math.PI * 2 / 3) * radius;
            fletch.position.y = Math.sin(i * Math.PI * 2 / 3) * radius;
            arrowGroup.add(fletch);
        }

        this.arrow = arrowGroup;
        this.arrowRestZ = -0.1;
        this.arrowDrawnZ = 0.15;
        this.arrow.position.z = this.arrowRestZ;
        this.crossbowGroup.add(this.arrow);
    }

    setStringPosition(x, y, z) {
        const centerPoint = new THREE.Vector3(x, y, z);

        const drawAmount = (z - this.stringRestZ) / (this.stringDrawnZ - this.stringRestZ) * this.maxDraw;
        this.updateArmDeformation(Math.max(0, drawAmount));

        const posL = this.stringLeft.geometry.attributes.position;
        posL.setXYZ(0, this.stringAnchorL.x, this.stringAnchorL.y, this.stringAnchorL.z);
        posL.setXYZ(1, centerPoint.x, centerPoint.y, centerPoint.z);
        posL.needsUpdate = true;

        const posR = this.stringRight.geometry.attributes.position;
        posR.setXYZ(0, this.stringAnchorR.x, this.stringAnchorR.y, this.stringAnchorR.z);
        posR.setXYZ(1, centerPoint.x, centerPoint.y, centerPoint.z);
        posR.needsUpdate = true;

        if (this.arrow) {
            this.arrow.position.z = z;
            this.arrow.position.x = x;
            this.arrow.position.y = y;
        }
    }

    playDrawAnimation(callback) {
        if (this.animating) return;
        this.animating = true;

        const duration = 800;
        const start = performance.now();
        const restZ = this.stringRestZ;
        const drawnZ = this.stringDrawnZ;

        const animate = (now) => {
            const elapsed = now - start;
            let t = Math.min(elapsed / duration, 1);
            t = t * t * (3 - 2 * t);

            const z = restZ + (drawnZ - restZ) * t;
            this.setStringPosition(0, 0, z);

            if (t < 1) {
                requestAnimationFrame(animate);
            } else {
                setTimeout(() => {
                    this.playReleaseAnimation(callback);
                }, 300);
            }
        };

        requestAnimationFrame(animate);
    }

    playReleaseAnimation(callback) {
        const duration = 150;
        const start = performance.now();
        const drawnZ = this.stringDrawnZ;
        const restZ = this.stringRestZ;

        const animate = (now) => {
            const elapsed = now - start;
            let t = Math.min(elapsed / duration, 1);
            t = 1 - Math.pow(1 - t, 3);

            const z = drawnZ + (restZ - drawnZ) * t;
            const vibrate = t < 0.3 ? Math.sin(t * 30) * (0.3 - t) * 0.02 : 0;
            this.setStringPosition(0, vibrate, z);

            if (t < 1) {
                requestAnimationFrame(animate);
            } else {
                this.animating = false;
                if (callback) callback();
            }
        };

        requestAnimationFrame(animate);
    }

    async playShotAnimation(trajectoryData) {
        if (this.animating) return;
        this.animating = true;

        const duration = 1200;
        const start = performance.now();
        const drawnZ = this.stringDrawnZ;
        const restZ = this.stringRestZ;

        await new Promise(resolve => {
            const drawAnim = (now) => {
                const elapsed = now - start;
                let t = Math.min(elapsed / 600, 1);
                t = t * t * (3 - 2 * t);
                const z = restZ + (drawnZ - restZ) * t;
                this.setStringPosition(0, 0, z);
                if (t < 1) requestAnimationFrame(drawAnim);
                else resolve();
            };
            requestAnimationFrame(drawAnim);
        });

        await new Promise(resolve => setTimeout(resolve, 200));

        if (this.arrow) {
            this.arrow.visible = true;
        }

        const releaseStart = performance.now();
        const points = trajectoryData;

        if (points && points.length > 1) {
            this.showTrajectoryPath(points);

            await new Promise(resolve => {
                const flyAnim = () => {
                    const elapsed = performance.now() - releaseStart;
                    const totalTime = points[points.length - 1].t;
                    let currentT = (elapsed / duration) * totalTime;

                    let idx = 0;
                    while (idx < points.length - 1 && points[idx + 1].t < currentT) {
                        idx++;
                    }

                    if (idx >= points.length - 1) {
                        const last = points[points.length - 1];
                        this.updateArrowPosition(last.x, last.y, last.z, last.vx, last.vy, last.vz);
                        this.impactMarker.position.set(last.x, 0.01, last.z);
                        this.impactMarker.visible = true;

                        if (this.arrow) this.arrow.visible = false;

                        setTimeout(() => {
                            this.animating = false;
                            this.setStringPosition(0, 0, restZ);
                            resolve();
                        }, 500);
                        return;
                    }

                    const p0 = points[idx];
                    const p1 = points[idx + 1];
                    const segmentT = (currentT - p0.t) / (p1.t - p0.t);

                    const x = p0.x + (p1.x - p0.x) * segmentT;
                    const y = p0.y + (p1.y - p0.y) * segmentT;
                    const z = p0.z + (p1.z - p0.z) * segmentT;
                    const vx = p0.vx + (p1.vx - p0.vx) * segmentT;
                    const vy = p0.vy + (p1.vy - p0.vy) * segmentT;
                    const vz = p0.vz + (p1.vz - p0.vz) * segmentT;

                    const releaseT = Math.min((performance.now() - releaseStart) / 100, 1);
                    const stringZ = drawnZ + (restZ - drawnZ) * releaseT;
                    const vibrate = releaseT < 0.3 ? Math.sin(releaseT * 40) * (0.3 - releaseT) * 0.015 : 0;
                    this.setStringPosition(0, vibrate, stringZ);

                    this.updateArrowPosition(x, y, z, vx, vy, vz);

                    requestAnimationFrame(flyAnim);
                };
                requestAnimationFrame(flyAnim);
            });
        } else {
            this.animating = false;
        }
    }

    updateArrowPosition(x, y, z, vx, vy, vz) {
        if (!this.arrow || !this.crossbowGroup) return;

        const worldPos = new THREE.Vector3();
        this.crossbowGroup.getWorldPosition(worldPos);

        this.arrow.position.set(
            x - worldPos.x,
            y - worldPos.y,
            z - worldPos.z
        );

        const speed = Math.sqrt(vx * vx + vy * vy + vz * vz);
        if (speed > 0.1) {
            this.arrow.lookAt(
                x - worldPos.x + vx / speed,
                y - worldPos.y + vy / speed,
                z - worldPos.z + vz / speed
            );
            this.arrow.rotateY(Math.PI / 2);
        }
    }

    showTrajectoryPath(points) {
        if (this.trajectoryLine) {
            this.scene.remove(this.trajectoryLine);
        }
        if (this.arrowPath) {
            this.scene.remove(this.arrowPath);
        }

        if (!this.showTrajectory) return;

        const geoPoints = points.map(p => new THREE.Vector3(p.x, p.y, p.z));
        const curve = new THREE.CatmullRomCurve3(geoPoints);

        const tubeGeo = new THREE.TubeGeometry(curve, Math.min(points.length * 2, 200), 0.015, 6, false);
        const tubeMat = new THREE.MeshBasicMaterial({
            color: 0x4ade80,
            transparent: true,
            opacity: 0.6
        });
        this.trajectoryLine = new THREE.Mesh(tubeGeo, tubeMat);
        this.scene.add(this.trajectoryLine);

        const lineGeo = new THREE.BufferGeometry().setFromPoints(geoPoints);
        const lineMat = new THREE.LineDashedMaterial({
            color: 0xfbbf24,
            dashSize: 0.15,
            gapSize: 0.1,
            transparent: true,
            opacity: 0.7
        });
        this.arrowPath = new THREE.Line(lineGeo, lineMat);
        this.arrowPath.computeLineDistances();
        this.scene.add(this.arrowPath);
    }

    clearTrajectory() {
        if (this.trajectoryLine) {
            this.scene.remove(this.trajectoryLine);
            this.trajectoryLine = null;
        }
        if (this.arrowPath) {
            this.scene.remove(this.arrowPath);
            this.arrowPath = null;
        }
        this.impactMarker.visible = false;
        if (this.arrow) {
            this.arrow.visible = true;
            this.arrow.position.z = this.arrowRestZ;
            this.arrow.position.x = 0;
            this.arrow.position.y = 0;
        }
        this.currentDrawAmount = 0;
        this.targetDrawAmount = 0;
        this.updateArmDeformation(0);
    }

    setShowTrajectory(show) {
        this.showTrajectory = show;
        if (!show) {
            this.clearTrajectory();
        }
    }

    setShowGrid(show) {
        this.showGrid = show;
        if (this.gridHelper) {
            this.gridHelper.visible = show;
        }
    }

    setShowSkeleton(show) {
        if (this.leftSkeletonHelper) {
            this.leftSkeletonHelper.visible = show;
        }
        if (this.rightSkeletonHelper) {
            this.rightSkeletonHelper.visible = show;
        }
    }

    setWireframe(enabled) {
        this.wireframe = enabled;
        this.scene.traverse((obj) => {
            if (obj.isMesh && obj.material) {
                if (Array.isArray(obj.material)) {
                    obj.material.forEach(m => m.wireframe = enabled);
                } else {
                    obj.material.wireframe = enabled;
                }
            }
        });
    }

    resetView() {
        this.camera.position.set(5, 3, 6);
        this.controls.target.set(0, 1, 0);
        this.controls.update();
    }

    updateCrossbow(crossbowData) {
        this.clearTrajectory();
        this.createCrossbow(crossbowData);
    }

    onResize() {
        const rect = this.container.getBoundingClientRect();
        this.camera.aspect = rect.width / rect.height;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(rect.width, rect.height);
    }

    animate() {
        requestAnimationFrame(() => this.animate());
        this.controls.update();
        this.renderer.render(this.scene, this.camera);
    }
}
