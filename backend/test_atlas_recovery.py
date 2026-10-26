"""Recovery protocol tests with mock firmware and isolated in-memory SQLite."""
import unittest
from uuid import uuid4
import test_api as fixtures
import atlas_runtime as runtime
import atlas_recovery as recovery


class RecoveryTests(unittest.TestCase):
    setUp = fixtures.BackendTests.setUp
    robot = fixtures.BackendTests.robot
    cp = fixtures.BackendTests.cp
    submit = fixtures.BackendTests.submit
    result = fixtures.BackendTests.result
    movement = fixtures.BackendTests.movement
    boot = 'a' * 32

    def poll(self, boot=None, completed=None):
        params = {'boot_id': boot or self.boot}
        if completed:
            params['recovery_completed'] = completed
        return self.client.get('/robot/robot1/command', params=params)

    def online(self):
        self.robot()
        self.assertEqual(self.poll().status_code, 200)

    def queue(self, action='reconnect', **extra):
        body = {'request_id': str(uuid4()), 'action': action} | extra
        response = self.client.post('/robot/robot1/recovery', json=body)
        self.assertEqual(response.status_code, 202, response.text)
        return body

    def ack(self, body, boot=None):
        return self.client.post('/robot/robot1/recovery/ack', json={
            'request_id': body['request_id'], 'boot_id': boot or self.boot})

    def test_requires_recent_compatible_firmware_and_strict_payload(self):
        self.robot()
        body = {'request_id': str(uuid4()), 'action': 'restart'}
        self.assertEqual(self.client.post('/robot/robot1/recovery', json=body).status_code, 409)
        self.poll()
        for invalid in (body | {'action': 'erase'}, body | {'enabled': True},
                        body | {'action': 'auto_reconnect'},
                        body | {'action': 'auto_reconnect', 'enabled': 'false'}):
            self.assertEqual(self.client.post('/robot/robot1/recovery', json=invalid).status_code, 422)
        runtime.state('robot1')['last_command_poll'] -= 4
        self.assertEqual(self.client.post('/robot/robot1/recovery', json=body).status_code, 409)

    def test_redelivery_until_ack_and_explicit_reconnect_completion(self):
        self.online()
        body = self.queue()
        for _ in range(2):
            wire = self.poll().json()['data']
            self.assertEqual(wire['recovery']['request_id'], body['request_id'])
            self.assertEqual(wire['command'], 'stop')
        self.assertEqual(self.ack(body, 'b' * 32).status_code, 409)
        self.assertEqual(self.ack(body).status_code, 200)
        self.assertEqual(self.ack(body).status_code, 200)
        self.assertIsNone(self.poll().json()['data']['recovery'])
        self.assertEqual(runtime.state('robot1')['recovery']['status'], 'accepted')
        self.poll(completed=str(uuid4()))
        self.assertEqual(runtime.state('robot1')['recovery']['status'], 'accepted')
        self.poll(completed=body['request_id'])
        self.assertEqual(runtime.state('robot1')['recovery']['status'], 'recovered')

    def test_restart_confirmed_only_by_new_boot_and_never_resumes(self):
        self.online()
        body = self.queue('restart')
        self.assertEqual(self.ack(body).status_code, 200)
        self.poll(completed=body['request_id'])
        self.assertEqual(runtime.state('robot1')['recovery']['status'], 'accepted')
        wire = self.poll(boot='b' * 32).json()['data']
        self.assertEqual(wire['command'], 'stop')
        state = runtime.state('robot1')
        self.assertEqual(state['recovery']['status'], 'recovered')
        self.assertFalse(state['checkpoint_location_verified'])
        self.assertIsNone(state['active_command_id'])
        self.assertEqual(self.ack(body).status_code, 409)

    def test_policy_waits_for_application_and_request_ids_remain_idempotent(self):
        self.online()
        body = self.queue('auto_reconnect', enabled=False)
        self.ack(body)
        self.assertFalse(self.poll().json()['data']['auto_reconnect'])
        self.assertEqual(runtime.state('robot1')['recovery']['status'], 'accepted')
        self.poll(completed=body['request_id'])
        self.assertEqual(runtime.state('robot1')['recovery']['status'], 'applied')
        next_body = self.queue('auto_reconnect', enabled=True)
        self.assertEqual(self.client.post('/robot/robot1/recovery', json=body).status_code, 202)
        self.assertEqual(runtime.state('robot1')['recovery']['request_id'], next_body['request_id'])
        self.assertEqual(self.client.post('/robot/robot1/recovery', json=body | {'enabled': True}).status_code, 409)

    def test_expired_and_old_boot_requests_are_not_executable(self):
        self.online()
        body = self.queue()
        runtime.state('robot1')['recovery']['created_at'] -= recovery.TTL + 1
        self.assertIsNone(self.poll().json()['data']['recovery'])
        self.assertEqual(self.ack(body).status_code, 409)
        self.assertEqual(runtime.state('robot1')['recovery']['status'], 'expired')
        body = self.queue()
        self.poll(boot='b' * 32)
        self.assertEqual(runtime.state('robot1')['recovery']['status'], 'cancelled')
        self.assertEqual(self.ack(body, 'b' * 32).status_code, 409)

    def test_interrupt_fails_recording_and_rejects_late_success(self):
        self.online()
        start = self.cp()
        recording = self.client.post('/robot/robot1/recordings', json={'start_checkpoint_id': start}).json()['id']
        self.submit(recording)
        wire = self.poll().json()['data']
        body = self.queue()
        self.assertEqual(runtime.recording_for('robot1')['status'], 'failed')
        self.assertEqual(self.client.post('/robot/robot1/command', json={'command': 'forward'}).status_code, 409)
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=self.result(wire)).status_code, 409)
        failed = self.result(wire, status='failed') | {'error': 'Interrupted for recovery'}
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=failed).status_code, 200)
        self.ack(body)
        self.poll(completed=body['request_id'])
        self.assertEqual(runtime.recording_for('robot1')['status'], 'failed')

    def test_spontaneous_reboot_invalidates_recording_and_location(self):
        self.online()
        start = self.cp()
        self.client.post('/robot/robot1/recordings', json={'start_checkpoint_id': start})
        runtime.state('robot1')['checkpoint_location_verified'] = True
        self.poll(boot='b' * 32)
        self.assertEqual(runtime.recording_for('robot1')['status'], 'failed')
        self.assertFalse(runtime.state('robot1')['checkpoint_location_verified'])

    def test_cannot_start_recording_during_pending_recovery(self):
        self.online()
        start = self.cp()
        body = self.queue()
        for acknowledged in (False, True):
            if acknowledged:
                self.ack(body)
            response = self.client.post('/robot/robot1/recordings', json={'start_checkpoint_id': start})
            self.assertEqual(response.status_code, 409)
            self.assertIsNone(runtime.recording_for('robot1'))
        self.poll(completed=body['request_id'])
        self.assertEqual(self.client.post('/robot/robot1/recordings', json={'start_checkpoint_id': start}).status_code, 201)


if __name__ == '__main__':
    unittest.main()
