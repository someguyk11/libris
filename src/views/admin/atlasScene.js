// Lightweight perspective renderer: true world-space cuboids projected to SVG.
// No WebGL dependency; geometry and camera are supplied by the scene component.
const near = 0.15
export function cameraPoint(point, camera) {
  const dx = point.x - camera.x, dz = point.z - camera.z
  return { x: dx * Math.cos(camera.yaw) - dz * Math.sin(camera.yaw), y: point.y - camera.y, z: dx * Math.sin(camera.yaw) + dz * Math.cos(camera.yaw) }
}
function intersection(a, b) {
  const t = (near - a.z) / (b.z - a.z)
  return { x: a.x + t * (b.x - a.x), y: a.y + t * (b.y - a.y), z: near }
}
export function clipFace(points) {
  const output = []
  points.forEach((b, index) => {
    const a = points[(index + points.length - 1) % points.length]
    if ((a.z >= near) !== (b.z >= near)) output.push(intersection(a, b))
    if (b.z >= near) output.push(b)
  })
  return output
}
export function projectPoint(point, camera, width = 1000, height = 650) {
  const focal = width / (2 * Math.tan(camera.fov * Math.PI / 360))
  return [width / 2 + point.x * focal / point.z, height * 0.47 - point.y * focal / point.z]
}
export function sceneFaces(obstacles, camera, scale = 1, viewport = { width: 1000, height: 650 }) {
  const faces = []
  obstacles.forEach(box => {
    const w = box.width * scale / 2, h = box.height * scale, d = box.depth * scale / 2
    const vertices = [
      [-w, 0, -d], [w, 0, -d], [w, h, -d], [-w, h, -d],
      [-w, 0, d], [w, 0, d], [w, h, d], [-w, h, d],
    ].map(([x, y, z]) => cameraPoint({ x: box.x + x, y: (box.y || 0) + y, z: box.z + z }, camera))
    const sides = [[0, 1, 2, 3], [1, 5, 6, 2], [5, 4, 7, 6], [4, 0, 3, 7], [3, 2, 6, 7]]
    // Cull outward-facing surfaces hidden from the camera before depth sorting.
    const visible = [camera.z < box.z - d, camera.x > box.x + w, camera.z > box.z + d, camera.x < box.x - w, camera.y > (box.y || 0) + h]
    sides.forEach((indices, index) => {
      if (!visible[index]) return
      const points = clipFace(indices.map(i => vertices[i]))
      if (points.length < 3) return
      faces.push({ id: `${box.id}-${index}`, shade: index, depth: points.reduce((sum, p) => sum + p.z, 0) / points.length, points: points.map(p => projectPoint(p, camera, viewport.width, viewport.height).join(',')).join(' ') })
    })
  })
  return faces.sort((a, b) => b.depth - a.depth)
}
export function sceneGrid(camera, { extent = 24, spacing = 2 } = {}, viewport = { width: 1000, height: 650 }) {
  const lines = []
  const step = Math.max(0.5, spacing), size = Math.max(2, Math.min(80, extent))
  function line(start, end) {
    let a = cameraPoint(start, camera), b = cameraPoint(end, camera)
    if (a.z < near && b.z < near) return
    if (a.z < near) a = intersection(a, b)
    else if (b.z < near) b = intersection(a, b)
    lines.push([projectPoint(a, camera, viewport.width, viewport.height), projectPoint(b, camera, viewport.width, viewport.height)])
  }
  for (let i = -size; i <= size; i += step) {
    line({ x: i, y: 0, z: -size }, { x: i, y: 0, z: size })
    line({ x: -size, y: 0, z: i }, { x: size, y: 0, z: i })
  }
  return lines
}
export function moveSimulation(pose, command) {
  const next = { ...pose }
  if (command === 'left' || command === 'right') next.yaw += command === 'left' ? -0.07 : 0.07
  if (command === 'forward' || command === 'reverse') {
    const distance = command === 'forward' ? 0.22 : -0.22
    next.x = Math.max(-20, Math.min(20, next.x + Math.sin(next.yaw) * distance))
    next.z = Math.max(-20, Math.min(20, next.z + Math.cos(next.yaw) * distance))
  }
  next.yaw = ((next.yaw + Math.PI) % (Math.PI * 2) + Math.PI * 2) % (Math.PI * 2) - Math.PI
  return next
}
