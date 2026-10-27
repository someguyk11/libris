"""Mock-device HTTP checks. Does not claim physical replay or use a real database."""
import os
import time
import unittest
from types import SimpleNamespace
from unittest.mock import patch
from uuid import uuid4
from sqlalchemy import text, inspect, create_engine
from sqlalchemy.schema import CreateTable
import test_api
from test_api import database, app, runtime, TestClient
from guidance_access import reader, staff, operational_guard
from guidance_models import GuidanceTask, GuidanceBase
from guidance_migrate import upgrade_guidance
import guidance


class GuidanceMigrationTests(unittest.TestCase):
    def test_retires_legacy_codes_preserves_sessions_and_is_idempotent(self):
        engine = create_engine('sqlite://')
        try:
            with engine.begin() as c:
                c.execute(text('CREATE TABLE schema_migrations(version TEXT PRIMARY KEY)'))
                c.execute(text('CREATE TABLE shelves(id INTEGER, destination_checkpoint_id TEXT)'))
                c.execute(text('CREATE TABLE robots(id TEXT PRIMARY KEY, home_checkpoint_id TEXT, guidance_state TEXT)'))
                ddl = str(CreateTable(GuidanceTask.__table__).compile(engine))
                ddl = ddl.replace("'waiting_for_atlas','queued'", "'awaiting_otp','waiting_for_atlas','queued'")
                c.exec_driver_sql(ddl)
                for definition in ('atlas_otp_hash TEXT', 'otp_expires_at DATETIME', 'otp_attempts INTEGER DEFAULT 0', 'verified_at DATETIME'):
                    c.exec_driver_sql('ALTER TABLE guidance_tasks ADD COLUMN ' + definition)
                for task_id, status, session in [('pending', 'awaiting_otp', None), ('valid', 'running', 'keep-session'), ('unsafe', 'running', None), ('history', 'completed', 'past-session')]:
                    c.execute(text('INSERT INTO robots VALUES (:id, NULL, :state)'), {'id': task_id, 'state': 'awaiting_otp' if task_id == 'pending' else 'guiding'})
                    c.execute(text('''INSERT INTO guidance_tasks
                        (id,user_id,book_id,shelf_id,robot_id,route_id,route_fingerprint,status,progress_index,created_at,updated_at,session_id)
                        VALUES (:id,:id,1,1,:id,:route,'recorded',:status,0,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,:session)'''),
                        {'id': task_id, 'route': uuid4().hex, 'status': status, 'session': session})
            upgrade_guidance(engine)
            upgrade_guidance(engine)
            with engine.connect() as c:
                rows = {row.id: row for row in c.execute(text('SELECT id,status,session_id,route_fingerprint FROM guidance_tasks'))}
                self.assertEqual(rows['pending'].status, 'cancelled')
                self.assertEqual(rows['unsafe'].status, 'failed')
                self.assertEqual((rows['valid'].status, rows['valid'].session_id), ('running', 'keep-session'))
                self.assertEqual(rows['history'].status, 'completed')
                self.assertTrue(all(row.route_fingerprint == 'recorded' for row in rows.values()))
                states = dict(c.execute(text('SELECT id,guidance_state FROM robots')).all())
                self.assertEqual(states['pending'], 'unknown')
                self.assertEqual(states['unsafe'], 'unknown')
                self.assertEqual(states['valid'], 'guiding')
                self.assertFalse({'atlas_otp_hash', 'otp_expires_at', 'otp_attempts', 'verified_at'} & {col['name'] for col in inspect(c).get_columns('guidance_tasks')})
        finally:
            engine.dispose()

class GuidanceTests(unittest.TestCase):
    robot = test_api.BackendTests.robot
    cp = test_api.BackendTests.cp
    connect = test_api.BackendTests.connect
    movement = test_api.BackendTests.movement
    segment = test_api.BackendTests.segment
    route = test_api.BackendTests.route

    def setUp(self):
        database.Base.metadata.create_all(database.engine)
        with database.engine.begin() as c:
            c.execute(text('CREATE TABLE IF NOT EXISTS schema_migrations(version TEXT PRIMARY KEY)'))
        upgrade_guidance(database.engine)
        with database.engine.begin() as c:
            for name in ('guidance_tasks', 'books', 'shelves', 'routes', 'robots', 'checkpoints'):
                c.execute(text(f'DELETE FROM {name}'))
        runtime.clear(); guidance.deliveries.clear(); guidance.arrivals.clear()
        self.client = TestClient(app)
        app.dependency_overrides[operational_guard] = lambda: None
        self.robot()
        self.start, self.end = self.cp('Home'), self.cp('Shelf')
        self.connect(self.start, [self.end])
        self.connect(self.end, [self.start])
        self.route_id = self.client.post('/robots/robot1/routes', json=self.route(self.start, self.end, status='active')).json()['id']
        self.return_id = self.client.post('/robots/robot1/routes', json=self.route(self.end, self.start, status='active', name='Return home')).json()['id']
        shelf = self.client.post('/shelves/', json={'shelf_name': 'Fiction'}).json()
        self.shelf = shelf['shelf_id']
        self.book = self.client.post('/book/register', json={'isbn': '9780140328721', 'title': 'Matilda', 'author': 'Roald Dahl', 'shelf_id': self.shelf}).json()['id']
        self.env = patch.dict(os.environ, {'GUIDANCE_ENABLED': 'true', 'ATLAS_DEVICE_KEYS': '{"robot1":"' + 'x' * 40 + '"}'})
        self.env.start()
        self.user = SimpleNamespace(user_id=str(uuid4()), email='reader@example.com')
        app.dependency_overrides[reader] = lambda: self.user
        app.dependency_overrides[staff] = lambda: self.user
        self.auth = patch('guidance_access.authenticate', return_value=self.user)
        self.auth.start()
        self.config = patch('guidance_access.get_config', return_value=SimpleNamespace(enabled=True))
        self.config.start()
        app.dependency_overrides.pop(operational_guard)
        # functional tests bypass only browser identity, not device authentication.
        self.guard = patch('guidance_access.staff_ids', return_value={self.user.user_id})
        self.guard.start()
        self.rates = patch('guidance.throttle')
        self.rates.start()
        self.client.headers['origin'] = 'http://localhost:5173'
        response = self.client.patch(f'/shelves/{self.shelf}/destination', json={'destination_checkpoint_id': self.end})
        self.assertEqual(response.status_code, 200, response.text)
        self.poll()
        self.assertEqual(self.client.patch('/robots/robot1/home', json={'home_checkpoint_id': self.start}).status_code, 200)
        self.confirm(self.start)

    def tearDown(self):
        app.dependency_overrides.clear()
        self.rates.stop(); self.guard.stop(); self.auth.stop(); self.env.stop(); self.config.stop()
        with database.engine.begin() as c:
            c.execute(text('DELETE FROM guidance_tasks'))
        runtime.clear(); guidance.deliveries.clear()

    def poll(self, stopped=True):
        return self.client.get('/robot/robot1/guidance/command', params={'stopped': str(stopped).lower(), 'protocol': 2}, headers={'x-atlas-key': 'x' * 40})

    def confirm(self, point, task=None):
        response = self.client.post('/robots/robot1/checkpoint-confirmation', json={'checkpoint_id': point, 'operator_confirmed': True, 'task_id': task})
        self.assertEqual(response.status_code, 200, response.text)

    def request(self, dispatch=True):
        response = self.client.post('/guidance-requests', json={'book_id': self.book})
        self.assertEqual(response.status_code, 201, response.text)
        task = response.json()
        if dispatch and task['status'] == 'waiting_for_atlas':
            self.poll()
            response = self.client.get(f"/guidance-requests/{task['id']}")
            self.assertEqual(response.status_code, 200, response.text)
            task = response.json()
        return task

    def ack(self, wire, status, **extra):
        body = {key: wire[key] for key in ('task_id', 'session_id', 'route_id', 'command_id', 'sequence')}
        return self.client.post('/robot/robot1/guidance/result', json=body | {'status': status} | extra, headers={'x-atlas-key': 'x' * 40})

    def execute(self):
        wire = self.poll().json()
        self.assertEqual(self.ack(wire, 'started').status_code, 200)
        response = self.ack(wire, 'completed', movement=wire['movement'])
        self.assertEqual(response.status_code, 200, response.text)
        return wire

    def arrival(self, task, point, route):
        self.poll(); self.confirm(point, task['id'])
        return self.client.post('/robot/robot1/guidance/arrival', headers={'x-atlas-key': 'x' * 40}, json={
            'task_id': task['id'], 'session_id': task['session_id'], 'route_id': route,
            'checkpoint_id': point, 'stopped': True, 'confirmation_method': 'operator_attestation'})

    def test_exact_route_acknowledged_replay_requires_operator_arrival(self):
        task = self.request()
        self.execute(); self.execute()
        result = self.client.get('/guidance-requests/' + task['id']).json()
        self.assertEqual(result['status'], 'running')
        self.assertTrue(result['awaiting_confirmation'])
        self.assertEqual(self.arrival(task, self.end, self.route_id).json()['robot_state'], 'returning_home')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'returning_home')
        self.execute(); self.execute()
        self.assertEqual(self.arrival(task, self.start, self.return_id).json()['robot_state'], 'at_home')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'completed')

    def test_recovery_interrupts_guidance_and_requires_device_ack(self):
        boot = 'a' * 32
        headers = {'x-atlas-key': 'x' * 40}
        self.assertEqual(self.client.get('/robot/robot1/command', params={'boot_id': boot}, headers=headers).status_code, 200)
        self.confirm(self.start)
        task = self.request()
        wire = self.poll().json()
        self.assertEqual(self.ack(wire, 'started').status_code, 200)
        body = {'request_id': str(uuid4()), 'action': 'restart'}
        response = self.client.post('/robot/robot1/recovery', json=body)
        self.assertEqual(response.status_code, 202, response.text)
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')
        self.assertEqual(self.poll().json()['command'], 'stop')
        self.assertFalse(runtime.state('robot1')['checkpoint_location_verified'])
        ack = {'request_id': body['request_id'], 'boot_id': boot}
        self.assertEqual(self.client.post('/robot/robot1/recovery/ack', json=ack).status_code, 401)
        self.assertEqual(self.client.post('/robot/robot1/recovery/ack', json=ack, headers=headers).status_code, 200)
        with patch('guidance_access.staff_ids', return_value=set()):
            self.assertEqual(self.client.post('/robot/robot1/recovery', json=body, headers=headers).status_code, 403)

    def test_poll_redelivery_and_duplicate_results_do_not_advance_twice(self):
        task = self.request()
        wire = self.poll().json()
        self.assertEqual(wire['command_id'], self.poll().json()['command_id'])
        self.ack(wire, 'accepted')
        self.assertFalse(self.poll().json()['execute'])
        self.ack(wire, 'started')
        self.ack(wire, 'completed', movement=wire['movement'])
        self.assertTrue(self.ack(wire, 'completed', movement=wire['movement']).json()['duplicate'])
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['progress_index'], 1)

    def test_request_retry_deduplicates_and_reserves_robot(self):
        first = self.request()
        self.assertEqual(first['id'], self.request()['id'])
        self.user = SimpleNamespace(user_id=str(uuid4()), email='another@example.com')
        self.assertEqual(self.request(dispatch=False)['status'], 'waiting_for_atlas')

    def test_missing_mapping_and_wrong_start_are_unavailable(self):
        with database.SessionLocal() as db:
            db.get(database.ShelfModel, self.shelf).destination_checkpoint_id = None
            db.commit()
        self.assertEqual(self.client.post('/guidance-requests', json={'book_id': self.book}).status_code, 409)
        self.client.patch(f'/shelves/{self.shelf}/destination', json={'destination_checkpoint_id': self.end})
        runtime.state('robot1')['checkpoint_location_verified'] = False
        self.assertEqual(self.request(dispatch=False)['status'], 'waiting_for_atlas')
        self.assertEqual(self.poll().json()['command'], 'stop')

    def test_cancellation_only_before_device_delivery(self):
        task = self.request(dispatch=False)
        self.assertEqual(self.client.post(f"/guidance-requests/{task['id']}/cancel").json()['status'], 'cancelled')
        self.confirm(self.start)
        task = self.request(); self.poll()
        self.assertEqual(self.client.post(f"/guidance-requests/{task['id']}/cancel").status_code, 409)

    def test_task_ownership_hides_other_users_records(self):
        task = self.request()
        self.user = SimpleNamespace(user_id=str(uuid4()), email='another@example.com')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).status_code, 404)
        self.assertEqual(self.client.post(f"/guidance-requests/{task['id']}/cancel").status_code, 404)

    def test_device_auth_is_per_robot_and_protocol_required(self):
        self.assertEqual(self.client.get('/robot/robot1/guidance/command?stopped=true&protocol=1').status_code, 401)
        self.assertEqual(self.client.get('/robot/robot2/guidance/command?stopped=true&protocol=1', headers={'x-atlas-key': 'x' * 40}).status_code, 401)
        self.assertEqual(self.client.get('/robot/robot1/guidance/command?stopped=true&protocol=1', headers={'x-atlas-key': 'x' * 40}).status_code, 409)

    def test_lost_connection_fails_and_never_resumes(self):
        task = self.request(); self.poll()
        runtime.state('robot1')['guidance_poll_at'] -= 10
        self.assertEqual(self.poll().json()['command'], 'stop')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')
        self.assertFalse(runtime.state('robot1')['checkpoint_location_verified'])

    def test_reconnect_after_firmware_lease_expiry_never_resumes(self):
        task = self.request(); wire = self.poll().json(); self.ack(wire, 'started')
        runtime.state('robot1')['guidance_poll_at'] -= guidance.LEASE_SECONDS + .1
        self.assertEqual(self.poll().json()['command'], 'stop')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')

    def test_late_fault_is_not_ignored_as_a_duplicate_completion(self):
        task = self.request(); wire = self.execute()
        self.ack(wire, 'failed', error='late fault')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')

    def test_conflicting_duplicate_encoder_result_fails_closed(self):
        task = self.request(); wire = self.execute()
        movement = wire['movement'] | {'left_encoder_ticks': wire['movement']['left_encoder_ticks'] + 1}
        self.assertEqual(self.ack(wire, 'completed', movement=movement).status_code, 409)
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')

    def test_bad_measurement_or_completion_without_start_fails(self):
        task = self.request(); wire = self.poll().json()
        self.assertEqual(self.ack(wire, 'completed', movement=wire['movement']).status_code, 409)
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')

    def test_obstacle_and_emergency_stop_fail_task(self):
        task = self.request(); wire = self.poll().json()
        self.ack(wire, 'failed', error='obstacle')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')
        self.confirm(self.start)
        task = self.request()
        self.assertEqual(self.client.post('/robots/robot1/guidance-stop').status_code, 200)
        self.assertEqual(self.poll().json()['command'], 'stop')

    def test_route_changes_fail_replay_and_manual_drive_is_blocked(self):
        task = self.request()
        self.client.get('/robot/robot1/command', headers={'x-atlas-key': 'x' * 40})
        self.assertEqual(self.client.post('/robot/robot1/command', json={'command': 'forward'}).status_code, 409)
        with database.SessionLocal() as db:
            db.get(database.RouteModel, self.route_id).status = 'disabled'; db.commit()
        self.assertEqual(self.poll().json()['command'], 'stop')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')

    def test_restart_marks_task_failed_not_resumable(self):
        task = self.request()
        runtime.clear()
        with database.SessionLocal() as db:
            guidance.recover(db)
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')

    def test_migration_additive_idempotent_and_active_claim_unique(self):
        task = self.request(); upgrade_guidance(database.engine)
        with database.SessionLocal() as db:
            self.assertIsNotNone(db.get(database.BookModel, self.book))
            row = db.get(GuidanceTask, task['id'])
            db.add(GuidanceTask(user_id=str(uuid4()), book_id=row.book_id, shelf_id=row.shelf_id, robot_id=row.robot_id, route_id=row.route_id, route_fingerprint=row.route_fingerprint, status='queued'))
            from sqlalchemy.exc import IntegrityError
            with self.assertRaises(IntegrityError):
                db.commit()
        self.assertIn('destination_checkpoint_id', [c['name'] for c in inspect(database.engine).get_columns('shelves')])

    def test_unverified_browser_and_nonstaff_cannot_control_legacy_routes(self):
        app.dependency_overrides.clear()
        with patch('guidance_access.authenticate', side_effect=guidance.HTTPException(401, 'Sign in')):
            self.assertEqual(self.client.post('/guidance-requests', json={'book_id': self.book}).status_code, 401)
        with patch('guidance_access.staff_ids', return_value=set()):
            self.assertEqual(self.client.post('/robot/robot1/command', json={'command': 'forward'}).status_code, 403)
            self.assertEqual(self.client.get('/robots/').status_code, 403)
        self.assertEqual(self.client.get('/robot/robot1/command').status_code, 401)

    def test_segment_timeout_and_confirmation_timeout_request_stop(self):
        task = self.request(); self.poll()
        guidance.deliveries[task['id']]['issued_at'] -= 61
        self.assertEqual(self.poll().json()['command'], 'stop')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')
        self.confirm(self.start)
        task = self.request(); self.execute(); self.execute()
        from datetime import timedelta
        with database.SessionLocal() as db:
            row = db.get(GuidanceTask, task['id']); row.updated_at -= timedelta(seconds=121); db.commit()
        self.assertEqual(self.poll().json()['command'], 'stop')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')

    def test_identity_mismatch_and_inexact_measured_ticks_fail_closed(self):
        task = self.request(); wire = self.poll().json()
        bad = wire | {'route_id': str(uuid4())}
        self.assertEqual(self.ack(bad, 'started').status_code, 409)
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')
        self.poll(); self.confirm(self.start)
        task = self.request(); wire = self.poll().json(); self.ack(wire, 'started')
        movement = wire['movement'] | {'left_encoder_ticks': wire['movement']['left_encoder_ticks'] + 1}
        self.assertEqual(self.ack(wire, 'completed', movement=movement).status_code, 409)

    def test_actual_motion_invalidates_start_and_no_cache_is_allowed(self):
        self.assertIn('no-store', self.poll(False).headers['cache-control'])
        task = self.request(dispatch=False)
        self.assertEqual(task['status'], 'waiting_for_atlas')
        self.client.post(f"/guidance-requests/{task['id']}/cancel")
        self.poll(); self.confirm(self.start)
        task = self.request()
        self.assertEqual(self.client.post('/robot/robot1/command', json={'command': 'stop'}).status_code, 200)
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'failed')

    def test_postgres_task_ddl_resolves_cross_metadata_foreign_keys(self):
        from sqlalchemy.schema import CreateTable
        from sqlalchemy.dialects import postgresql
        sql = str(CreateTable(GuidanceTask.__table__).compile(dialect=postgresql.dialect()))
        self.assertIn('route_id UUID', sql)
        self.assertIn('REFERENCES robots (id)', sql)
        task = self.request()
        response = self.client.delete('/routes/' + self.route_id)
        self.assertEqual(response.status_code, 409)
        self.assertIsNotNone(self.client.get('/guidance-requests/' + task['id']).json()['id'])

    def test_ready_atlas_dispatches_immediately_without_code(self):
        task = self.request(dispatch=False)
        self.assertEqual(task['status'], 'waiting_for_atlas')
        wire = self.poll().json()
        self.assertEqual(wire['command'], 'replay_segment')
        self.assertTrue(wire['execute'])
        self.assertTrue(wire['session_id'])
        public = self.client.get('/guidance-requests/' + task['id']).json()
        self.assertEqual(public['session_id'], wire['session_id'])
        self.assertFalse(any('otp' in key for key in public))
        with database.SessionLocal() as db:
            row = db.get(GuidanceTask, task['id'])
            self.assertEqual(row.session_id, wire['session_id'])
        repeated = self.poll().json()
        self.assertEqual(repeated['command_id'], wire['command_id'])
        self.assertEqual(repeated['session_id'], wire['session_id'])

    def test_direct_start_preserves_task_ownership_and_removed_endpoint(self):
        owner = self.user
        first = self.request(dispatch=False)
        self.user = SimpleNamespace(user_id=str(uuid4()), email='other@example.com')
        self.assertEqual(self.client.get(f"/guidance-requests/{first['id']}").status_code, 404)
        self.assertEqual(self.client.post(f"/guidance-requests/{first['id']}/cancel").status_code, 404)
        self.user = owner
        self.assertEqual(self.client.post(f"/guidance-requests/{first['id']}/verify", json={}).status_code, 404)

    def test_cancel_waiting_task_prevents_dispatch(self):
        task = self.request(dispatch=False)
        self.assertEqual(self.client.post(f"/guidance-requests/{task['id']}/cancel").status_code, 200)
        self.assertEqual(self.poll().json()['command'], 'stop')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'cancelled')

    def test_concurrent_device_polls_create_exactly_one_session(self):
        from concurrent.futures import ThreadPoolExecutor
        self.request(dispatch=False)
        with ThreadPoolExecutor(max_workers=2) as pool:
            responses = list(pool.map(lambda _: self.poll(), range(2)))
        self.assertTrue(all(r.status_code == 200 for r in responses))
        self.assertEqual(len({r.json()['session_id'] for r in responses}), 1)
        self.assertEqual(len({r.json()['command_id'] for r in responses}), 1)
        with database.SessionLocal() as db:
            self.assertEqual(db.query(GuidanceTask).filter(GuidanceTask.session_id.is_not(None)).count(), 1)

    def test_fifo_waits_through_return_then_dispatches_next_without_code(self):
        first = self.request()
        owner = self.user
        self.user = SimpleNamespace(user_id=str(uuid4()), email='next@example.com')
        second = self.request(dispatch=False)
        self.user = SimpleNamespace(user_id=str(uuid4()), email='third@example.com')
        third = self.request(dispatch=False)
        self.assertIsNone(second['session_id'])
        self.user = owner
        self.execute(); self.execute(); self.arrival(first, self.end, self.route_id)
        self.assertEqual(self.poll().json()['phase'], 'return')
        with database.SessionLocal() as db:
            self.assertEqual(db.get(GuidanceTask, second['id']).status, 'waiting_for_atlas')
            self.assertIsNone(db.get(GuidanceTask, second['id']).session_id)
            self.assertEqual(db.get(database.RobotModel, 'robot1').guidance_state, 'returning_home')
        self.execute(); self.execute()
        self.assertEqual(self.poll().json()['command'], 'stop')
        self.assertEqual(self.arrival(first, self.start, self.return_id).status_code, 200)
        wire = self.poll().json()
        self.assertEqual((wire['command'], wire['task_id']), ('replay_segment', second['id']))
        with database.SessionLocal() as db:
            self.assertIsNone(db.get(GuidanceTask, third['id']).session_id)

    def test_failed_return_never_claims_home_or_dispatches_next_task(self):
        task = self.request(); self.execute(); self.execute(); self.arrival(task, self.end, self.route_id)
        self.user = SimpleNamespace(user_id=str(uuid4()), email='next@example.com')
        waiting = self.request(dispatch=False)
        wire = self.poll().json(); self.ack(wire, 'failed', error='wheel stall')
        self.assertEqual(self.poll().json()['command'], 'stop')
        with database.SessionLocal() as db:
            self.assertEqual(db.get(database.RobotModel, 'robot1').guidance_state, 'fault')
            self.assertEqual(db.get(GuidanceTask, waiting['id']).status, 'waiting_for_atlas')
            self.assertIsNone(db.get(GuidanceTask, waiting['id']).session_id)

    def test_moving_atlas_cannot_claim_waiting_task(self):
        task = self.request(dispatch=False)
        self.assertEqual(self.poll(False).json()['command'], 'stop')
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'waiting_for_atlas')

    def test_waiting_timeout_and_missing_return_route(self):
        from datetime import timedelta
        runtime.state('robot1')['checkpoint_location_verified'] = False
        task = self.request(dispatch=False)
        with database.SessionLocal() as db:
            db.get(GuidanceTask, task['id']).created_at -= timedelta(seconds=901); db.commit()
        self.assertEqual(self.client.get('/guidance-requests/' + task['id']).json()['status'], 'expired')
        with database.SessionLocal() as db:
            db.get(database.RouteModel, self.return_id).status = 'disabled'; db.commit()
        self.assertEqual(self.client.post('/guidance-requests', json={'book_id': self.book}).status_code, 409)

    def test_encoder_only_arrival_and_wrong_session_are_rejected(self):
        task = self.request(); self.execute(); self.execute()
        self.poll()
        payload = {'task_id': task['id'], 'session_id': task['session_id'], 'route_id': self.route_id,
                   'checkpoint_id': self.end, 'stopped': True, 'confirmation_method': 'operator_attestation'}
        endpoint = '/robot/robot1/guidance/arrival'
        self.assertEqual(self.client.post(endpoint, json=payload, headers={'x-atlas-key': 'x' * 40}).status_code, 409)
        self.confirm(self.end, task['id'])
        self.assertEqual(self.client.post(endpoint, json=payload | {'session_id': str(uuid4())}, headers={'x-atlas-key': 'x' * 40}).status_code, 409)
        self.assertEqual(self.client.post(endpoint, json=payload, headers={'x-atlas-key': 'x' * 40}).status_code, 200)
