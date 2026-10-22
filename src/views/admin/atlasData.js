export function validateAtlas(input, units, editingId = null, preserveCase = false) {
  const name = String(input.name || '').trim()
  const rawIdentifier = String(input.identifier || '').trim()
  const identifier = preserveCase ? rawIdentifier : rawIdentifier.toUpperCase()
  if (!name || name.length > 80) throw new Error('Enter an Atlas name of up to 80 characters.')
  if (!/^[A-Z0-9-]{2,40}$/i.test(identifier)) throw new Error('Use 2–40 letters, numbers, or hyphens for the identifier.')
  if (units.some(unit => unit.id !== editingId && (preserveCase ? unit.identifier : unit.identifier.toUpperCase()) === identifier)) throw new Error('That Atlas identifier is already registered.')
  return { name, identifier }
}

export function validateSimulationRoute(input, units) {
  const name = String(input.name || '').trim()
  if (!name || name.length > 100) throw new Error('Enter a route name of up to 100 characters.')
  if (!units.some(unit => unit.id === input.atlasId)) throw new Error('Choose a registered Atlas.')
  if (!Array.isArray(input.steps) || !input.steps.length || input.steps.length > 5000) throw new Error('Record between 1 and 5,000 simulation steps.')
  const commands = ['forward', 'reverse', 'left', 'right']
  let lastAt = -1
  const steps = input.steps.map(step => {
    if (!commands.includes(step.command) || !Number.isFinite(step.at) || step.at < 0 || step.at < lastAt || !['x', 'z', 'yaw'].every(key => Number.isFinite(step.pose?.[key]))) throw new Error('The simulation contains invalid movement data.')
    lastAt = step.at
    return { command: step.command, at: step.at, pose: { x: step.pose.x, z: step.pose.z, yaw: step.pose.yaw } }
  })
  return { name, atlasId: input.atlasId, destination: String(input.destination || ''), steps, simulation: true }
}
