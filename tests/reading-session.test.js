import test from 'node:test'
import assert from 'node:assert/strict'
import { durationInSeconds, formatCountdown, secondsRemaining } from '../src/composables/readingSession.js'

test('room durations support minutes and hours', () => {
  assert.equal(durationInSeconds('42 min'), 2520)
  assert.equal(durationInSeconds('1 h 10 min'), 4200)
  assert.equal(durationInSeconds('05 min'), 300)
})
test('deadline-based timers account for sleeping tabs without becoming negative', () => {
  const session = { running: true, deadline: 100000, seconds: 90 }
  assert.equal(secondsRemaining(session, 50000), 50)
  assert.equal(secondsRemaining(session, 99999), 1)
  assert.equal(secondsRemaining(session, 500000), 0)
  assert.equal(secondsRemaining({ ...session, running: false }, 500000), 90)
})
test('countdown keeps minutes and seconds unambiguous', () => {
  assert.equal(formatCountdown(2520), '42:00')
  assert.equal(formatCountdown(61), '01:01')
  assert.equal(formatCountdown(0), '00:00')
})
