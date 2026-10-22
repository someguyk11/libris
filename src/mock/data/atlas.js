// Illustrative fixtures only. These are not live robots or sensor readings.
export function createAtlasSeed() {
  return {
    version: 1,
    units: [
      { id: 'atlas-01', name: 'Atlas-01', identifier: 'AT-001', snapshot: { online: true, battery: 78, lastSeen: '2 minutes ago', task: 'No active task', mode: 'Idle' } },
      { id: 'atlas-02', name: 'Atlas-02', identifier: 'AT-002', snapshot: { online: false, battery: 42, lastSeen: '1 hour ago', task: 'Connection lost', mode: 'Standby' } },
      { id: 'atlas-03', name: 'Atlas-03', identifier: 'AT-003', snapshot: { online: true, battery: 65, lastSeen: '5 minutes ago', task: 'Navigating to shelf SH-A01', mode: 'Moving' } },
    ],
    routes: [],
  }
}

// World coordinates: x = right, y = up, z = forward. Units are illustrative.
export const atlasSceneDefaults = {
  camera: { x: 0, y: 1.25, z: 0, yaw: 0, fov: 90 },
  grid: { extent: 24, spacing: 2 },
  obstacles: [
    { id: 'left-near', x: -4.4, z: 4.5, width: 2.5, height: 3.1, depth: 3.5 },
    { id: 'right-near', x: 4.7, z: 5.7, width: 2.7, height: 3.5, depth: 3.5 },
    { id: 'left-middle', x: -3.5, z: 11, width: 2.2, height: 1.8, depth: 2.5 },
    { id: 'left-far', x: -4.8, z: 16, width: 2, height: 1.1, depth: 2 },
    { id: 'right-middle', x: 3.4, z: 15, width: 1.8, height: 1.7, depth: 2.2 },
    { id: 'far', x: -2.5, z: 24, width: 2, height: 1.1, depth: 2 },
  ],
}
