"""Focused Atlas V1 integration tests; only isolated in-memory SQLite is written."""
import os
import unittest
from unittest.mock import patch
from uuid import uuid4
from copy import deepcopy
os.environ['DATABASE_URL'] = 'sqlite://'
from sqlalchemy import create_engine, event, inspect, text
from sqlalchemy.pool import StaticPool
from sqlalchemy.exc import SQLAlchemyError
import database
database.engine = create_engine('sqlite://', connect_args={'check_same_thread': False}, poolclass=StaticPool)
@event.listens_for(database.engine, 'connect')
def foreign_keys(connection, _):
    connection.execute('PRAGMA foreign_keys=ON')
database.SessionLocal.configure(bind=database.engine)
from fastapi.testclient import TestClient
from main import app
import atlas_runtime as runtime
from migrate import upgrade, VERSION

class BackendTests(unittest.TestCase):
    def setUp(self):
        # These catalogue/recording functional tests isolate operational auth;
        # dedicated authorization tests exercise the real guard without overrides.
        from guidance_access import operational_guard
        app.dependency_overrides[operational_guard] = lambda: None
        self.addCleanup(lambda: app.dependency_overrides.pop(operational_guard, None))
        database.Base.metadata.create_all(database.engine)
        with database.engine.begin() as connection:
            for table in ('routes', 'robots', 'checkpoints', 'books', 'shelves'):
                connection.execute(text(f'DELETE FROM {table}'))
        runtime.clear()
        self.client = TestClient(app)
        self.client.headers['origin'] = 'http://localhost:5173'

    def robot(self, identifier='robot1'):
        response = self.client.post('/robots/', json={'id': identifier, 'name': 'Atlas One'})
        self.assertEqual(response.status_code, 201, response.text)
        return response.json()

    def cp(self, name='C02', connections=None):
        response = self.client.post('/checkpoints/', json={'name': name, 'connected_checkpoint_ids': connections or []})
        self.assertEqual(response.status_code, 201, response.text)
        return response.json()['id']

    def connect(self, identifier, connections, active=True):
        return self.client.put(f'/checkpoints/{identifier}', json={'name': self.client.get(f'/checkpoints/{identifier}').json()['name'], 'connected_checkpoint_ids': connections, 'is_active': active})

    def movement(self, ticks=20, direction='forward'):
        return {'left_encoder_ticks': ticks, 'right_encoder_ticks': ticks, 'left_direction': direction, 'right_direction': direction}

    def segment(self, sequence, identifier=None, readings=None):
        result = {'sequence': sequence, 'checkpoint_id': identifier, 'movement': self.movement()}
        if readings is not None:
            result['checkpoint_sensor_readings'] = readings
        return result

    def route(self, start, destination, segments=None, status='draft', name='Route One'):
        return {'name': name, 'start_checkpoint_id': start, 'destination_checkpoint_id': destination, 'status': status, 'route_segments': segments if segments is not None else [self.segment(1, start), self.segment(2, destination)]}

    def start(self, start=None):
        self.robot()
        start = start or self.cp()
        self.client.get('/robot/robot1/command')
        response = self.client.post('/robot/robot1/recordings', json={'start_checkpoint_id': start})
        self.assertEqual(response.status_code, 201, response.text)
        return response.json()['id'], start

    def submit(self, recording_id, movement='forward', request_id=None):
        data = {'command': movement, 'recording_id': recording_id}
        if request_id:
            data['request_id'] = request_id
        response = self.client.post('/robot/robot1/command', json=data)
        self.assertEqual(response.status_code, 200, response.text)
        return response.json()['data']

    def result(self, command, movement=None, status='completed', sensors=None):
        data = {'command_id': command['command_id'], 'recording_id': command.get('recording_id'),
                'sequence': command.get('recording_sequence') or command['sequence'], 'status': status,
                'captured_at': '2026-10-18T10:00:00Z'}
        if status == 'completed':
            data['movement'] = movement or (self.movement(0, 'stop') if command['command'] == 'stop' else self.movement())
        if sensors is not None:
            data['sensor_readings'] = sensors
        return data

    def execute(self, recording_id, movement='forward', sensors=None):
        submitted = self.submit(recording_id, movement)
        delivered = self.client.get('/robot/robot1/command').json()['data']
        self.assertEqual(submitted['command_id'], delivered['command_id'])
        body = self.result(delivered, sensors=sensors)
        response = self.client.post('/robot/robot1/command/result', json=body)
        self.assertEqual(response.status_code, 200, response.text)
        return delivered, body

    def confirm(self, recording_id, sequence, checkpoint_id, sensor_time=None):
        body = {'sequence': sequence, 'checkpoint_id': checkpoint_id, 'confirmation_basis': 'operator_confirmed'}
        if sensor_time:
            body['sensor_captured_at'] = sensor_time
        return self.client.post(f'/recordings/{recording_id}/checkpoints', json=body)

    def completed_recording(self):
        recording, start = self.start()
        destination = self.cp('C03')
        self.connect(start, [destination])
        self.execute(recording, 'stop')
        self.assertEqual(self.confirm(recording, 1, start).status_code, 200)
        self.execute(recording)
        self.assertEqual(self.confirm(recording, 2, destination).status_code, 200)
        self.execute(recording, 'stop')
        return recording, start, destination

    def test_robot1_text_identity_nullable_checkpoint_and_registration_alias(self):
        robot = self.robot()
        self.assertEqual(robot['id'], 'robot1')
        self.assertEqual(robot['robot_id'], 'robot1')
        self.assertIsNone(robot['residing_checkpoint_id'])
        self.assertIsNone(robot['last_sequence_number'])
        self.assertEqual(self.client.post('/robots/', json={'id': 'robot1', 'name': 'Duplicate'}).status_code, 409)
        response = self.client.post('/robots/', json={'robot_id': 'AT-OLD', 'name': 'Legacy website'})
        self.assertEqual(response.status_code, 201)
        self.assertEqual(response.json()['id'], 'AT-OLD')
        self.assertEqual(self.client.patch('/robots/robot1', json={'name': 'Renamed'}).json()['name'], 'Renamed')
        self.assertEqual(self.client.patch('/robots/robot1', json={'name': 'No', 'id': 'different'}).status_code, 422)

    def test_legacy_telemetry_and_network_protocol_do_not_add_database_columns(self):
        body = {'wifi_ssid': 'Library', 'signal_strength': -50, 'uptime_seconds': 10, 'firmware_version': 'encoder-v1'}
        self.assertEqual(self.client.post('/robot/robot1/telemetry', json=body).status_code, 200)
        self.assertEqual(self.client.get('/robots/').json()[0]['firmware_version'], 'encoder-v1')
        status = self.client.get('/robot/robot1/status').json()['data']
        self.assertEqual(status['wifi_ssid'], 'Library')
        self.assertFalse(status['drive_ready'])
        self.client.post('/robot/robot1/reconfigure', json={'ssid': 'New', 'password': 'do-not-store'})
        self.assertNotIn('do-not-store', self.client.get('/robot/robot1/status').text)
        self.assertEqual(self.client.get('/robot/robot1/command').json()['data']['new_network']['password'], 'do-not-store')
        self.assertIsNone(self.client.get('/robot/robot1/command').json()['data']['new_network'])
        self.assertNotIn('wifi_ssid', database.RobotModel.__table__.columns)

    def test_no_browser_poll_claims_hardware_location(self):
        self.robot()
        status = self.client.get('/robot/robot1/status').json()['data']
        self.assertEqual(status['last_seen'], 0)
        self.assertFalse(status['checkpoint_location_verified'])
        self.assertEqual(self.client.post('/robot/robot1/command', json={'command': 'forward'}).status_code, 409)
        self.assertEqual(self.client.post('/robot/robot1/command', json={'command': 'stop'}).status_code, 200)

    def test_directed_checkpoint_connections_and_inactive_targets(self):
        a, b = self.cp(), self.cp('C03')
        self.assertEqual(self.connect(a, [b]).status_code, 200)
        self.assertEqual(self.client.get(f'/checkpoints/{b}').json()['connected_checkpoint_ids'], [])
        self.assertEqual(self.connect(b, [str(uuid4())]).status_code, 404)
        self.assertEqual(self.connect(a, [b, b]).status_code, 422)
        self.client.delete(f'/checkpoints/{b}')
        self.assertEqual(self.client.get(f'/checkpoints/{a}').json()['connected_checkpoint_ids'], [])
        self.assertEqual(self.connect(a, [b]).status_code, 409)

    def test_json_route_repeated_checkpoint_visits_and_original_order(self):
        self.robot()
        a, b, c = self.cp(), self.cp('C03'), self.cp('C05')
        self.connect(a, [b]); self.connect(b, [c]); self.connect(c, [b])
        body = self.route(a, b, [self.segment(1, a), self.segment(2, b), self.segment(3, c), self.segment(4, b)], status='active')
        response = self.client.post('/robots/robot1/routes', json=body)
        self.assertEqual(response.status_code, 201, response.text)
        self.assertEqual([segment['checkpoint_id'] for segment in response.json()['route_segments']], [a, b, c, b])
        self.assertIsNone(response.json()['last_verified_at'])
        route_id = response.json()['id']
        self.assertEqual(self.client.get(f'/routes/{route_id}').json()['route_segments'], response.json()['route_segments'])
        self.assertEqual(self.client.patch(f'/routes/{route_id}', json={'name': 'Renamed'}).json()['name'], 'Renamed')
        self.assertEqual(self.client.delete(f'/routes/{route_id}').status_code, 200)

    def test_activation_requires_explicit_reverse_connection(self):
        self.robot()
        a, b = self.cp(), self.cp('C03'); self.connect(a, [b])
        body = self.route(b, a, status='active')
        self.assertEqual(self.client.post('/robots/robot1/routes', json=body).status_code, 409)
        self.connect(b, [a])
        self.assertEqual(self.client.post('/robots/robot1/routes', json=body).status_code, 201)

    def test_invalid_sequence_or_unmeasured_segments_are_rejected(self):
        self.robot(); a = self.cp()
        for segments in ([self.segment(1, a), self.segment(1, a)], [self.segment(2, a), self.segment(1, a)]):
            self.assertEqual(self.client.post('/robots/robot1/routes', json=self.route(a, a, segments)).status_code, 422)
        segment = self.segment(1, a); del segment['movement']['left_encoder_ticks']
        self.assertEqual(self.client.post('/robots/robot1/routes', json=self.route(a, a, [segment], 'active')).status_code, 422)
        self.assertEqual(self.client.post('/robots/robot1/routes', json=self.route(a, a, [], 'active')).status_code, 409)

    def test_sensor_nulls_omissions_and_checkpoint_only_persistence(self):
        self.robot(); a = self.cp()
        response = self.client.post('/robots/robot1/routes', json=self.route(a, a, [self.segment(1, a, {'front_cm': None})]))
        self.assertEqual(response.status_code, 201)
        self.assertIsNone(response.json()['route_segments'][0]['checkpoint_sensor_readings']['front_cm'])
        self.assertEqual(self.client.post('/robots/robot1/routes', json=self.route(a, a, [self.segment(1, None, {'front_cm': 10})])).status_code, 422)
        self.assertEqual(self.client.post('/robots/robot1/routes', json=self.route(a, a, [self.segment(1, a, {'front_cm': -1})])).status_code, 422)

    def test_deactivation_disables_active_routes_even_for_intermediate_checkpoint(self):
        self.robot(); a, b, c = self.cp(), self.cp('C03'), self.cp('C05')
        self.connect(a, [b]); self.connect(b, [c])
        response = self.client.post('/robots/robot1/routes', json=self.route(a, c, [self.segment(1, a), self.segment(2, b), self.segment(3, c)], 'active'))
        self.assertEqual(response.status_code, 201)
        self.client.delete(f'/checkpoints/{b}')
        self.assertEqual(self.client.get(f"/routes/{response.json()['id']}").json()['status'], 'disabled')
        self.assertEqual(self.client.get(f'/checkpoints/{a}').json()['connected_checkpoint_ids'], [])

    def test_removing_a_directed_edge_disables_affected_routes(self):
        self.robot(); a, b = self.cp(), self.cp('C03'); self.connect(a, [b])
        response = self.client.post('/robots/robot1/routes', json=self.route(a, b, status='active'))
        self.connect(a, [])
        self.assertEqual(self.client.get(f"/routes/{response.json()['id']}").json()['status'], 'disabled')

    def test_one_active_recording_per_robot_and_no_per_movement_route_write(self):
        recording, start = self.start()
        self.assertEqual(self.client.post('/robot/robot1/recordings', json={'start_checkpoint_id': start}).status_code, 409)
        self.execute(recording)
        self.assertEqual(self.client.get('/robots/robot1/routes').json(), [])
        self.assertEqual(len(runtime.active_recordings), 1)
        self.assertNotIn('recordings', inspect(database.engine).get_table_names())

    def test_duplicate_poll_delivery_and_started_ack_do_not_create_new_execution(self):
        recording, _ = self.start()
        request_id = str(uuid4())
        submitted = self.submit(recording, request_id=request_id)
        first = self.client.get('/robot/robot1/command').json()['data']
        second = self.client.get('/robot/robot1/command').json()['data']
        self.assertEqual(first['command_id'], second['command_id'])
        self.assertEqual(first['sequence'], second['sequence'])
        self.assertEqual(len(runtime.pending_commands), 1)
        self.assertEqual(self.submit(recording, request_id=request_id)['command_id'], submitted['command_id'])
        started = self.result(first, status='started')
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=started).status_code, 200)
        self.assertFalse(self.client.get('/robot/robot1/command').json()['data']['execute'])
        completed = self.result(first)
        for _ in range(2):
            self.assertEqual(self.client.post('/robot/robot1/command/result', json=completed).status_code, 200)
        changed = deepcopy(completed); changed['movement']['left_encoder_ticks'] += 1
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=changed).status_code, 409)

    def test_encoder_results_must_match_robot_recording_and_sequence(self):
        recording, _ = self.start()
        command = self.submit(recording)
        result = self.result(command)
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=result).status_code, 409)
        self.client.get('/robot/robot1/command')
        result['sequence'] += 1
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=result).status_code, 409)
        result['sequence'] -= 1
        self.assertEqual(self.client.post('/robot/other/command/result', json=result).status_code, 404)
        result['recording_id'] = str(uuid4())
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=result).status_code, 409)

    def test_command_completion_does_not_automatically_confirm_checkpoint(self):
        recording, start = self.start()
        command, _ = self.execute(recording, sensors={'front_cm': 42})
        self.assertIsNone(self.client.get('/robots/').json()[0]['residing_checkpoint_id'])
        self.assertEqual(self.confirm(recording, 1, start).status_code, 200)
        self.assertEqual(self.client.get('/robots/').json()[0]['residing_checkpoint_id'], start)
        self.submit(recording, 'backward')
        self.assertFalse(self.client.get('/robot/robot1/status').json()['data']['checkpoint_location_verified'])

    def test_sensors_use_capture_identity_not_http_arrival_order(self):
        recording, start, destination = self.completed_recording()
        command = runtime.pending_commands[runtime.active_recordings[recording]['command_ids'][1]]
        # Completed result already arrived. Earlier-captured samples may arrive later.
        base = {'command_id': command['id'], 'recording_id': recording, 'sequence': 2}
        for stamp, cm in [('2026-10-18T09:59:59Z', 12), ('2026-10-18T09:59:58Z', 33)]:
            body = {**base, 'captured_at': stamp, 'sensor_readings': {'front_cm': cm}}
            self.assertEqual(self.client.post('/robot/robot1/sensors', json=body).status_code, 200)
        # A prior confirmation with no snapshot is immutable, so select a fresh observation on final stop.
        self.assertEqual(self.confirm(recording, 3, destination, '2026-10-18T10:00:01Z').status_code, 200)
        last = runtime.pending_commands[runtime.active_recordings[recording]['command_ids'][2]]
        body = {'command_id': last['id'], 'recording_id': recording, 'sequence': 3, 'captured_at': '2026-10-18T10:00:01Z', 'sensor_readings': {'front_cm': None, 'left_cm': 18}}
        self.assertEqual(self.client.post('/robot/robot1/sensors', json=body).status_code, 200)
        response = self.client.post(f'/recordings/{recording}/save', json={'name': 'Recorded', 'destination_checkpoint_id': destination, 'status': 'active'})
        self.assertEqual(response.status_code, 200, response.text)
        segments = response.json()['route_segments']
        self.assertIsNone(segments[1].get('checkpoint_sensor_readings'))
        self.assertEqual(segments[2]['checkpoint_sensor_readings']['left_cm'], 18)
        with database.SessionLocal() as db:
            persisted = db.get(database.RouteModel, recording).route_segments
            self.assertFalse(any(segment.get('checkpoint_sensor_readings', {}).get('front_cm') in (12, 33) for segment in persisted if segment.get('checkpoint_sensor_readings')))
            self.assertNotIn('captured_at', str(persisted))
            self.assertNotIn('command_id', str(persisted))

    def test_finalize_waits_for_all_results_and_stops_accepting_new_movements(self):
        recording, start = self.start(); destination = self.cp('C03'); self.connect(start, [destination])
        self.execute(recording, 'stop'); self.confirm(recording, 1, start)
        command = self.submit(recording)
        self.client.get('/robot/robot1/command')
        body = {'name': 'Recorded', 'destination_checkpoint_id': destination, 'status': 'active'}
        waiting = self.client.post(f'/recordings/{recording}/save', json=body)
        self.assertEqual(waiting.status_code, 202)
        self.assertEqual(self.client.get('/robots/robot1/routes').json(), [])
        self.assertEqual(self.client.post('/robot/robot1/command', json={'command': 'left', 'recording_id': recording}).status_code, 409)
        stop = self.client.get('/robot/robot1/command').json()['data']
        self.assertEqual(stop['command'], 'stop')
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=self.result(command)).status_code, 200)
        self.confirm(recording, 2, destination)
        self.assertEqual(self.client.post(f'/recordings/{recording}/save', json=body).status_code, 202)
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=self.result(stop)).status_code, 200)
        self.assertEqual(self.client.post(f'/recordings/{recording}/save', json=body).status_code, 200)
        self.assertNotIn(recording, runtime.active_recordings)

    def test_failed_database_save_retains_buffer_and_retry_is_idempotent(self):
        recording, start, destination = self.completed_recording()
        body = {'name': 'Recorded', 'destination_checkpoint_id': destination, 'status': 'active'}
        with patch('sqlalchemy.orm.Session.commit', side_effect=SQLAlchemyError('simulated connection loss')):
            response = self.client.post(f'/recordings/{recording}/save', json=body)
        self.assertEqual(response.status_code, 503, response.text)
        self.assertIn(recording, runtime.active_recordings)
        self.assertEqual(len(runtime.active_recordings[recording]['command_ids']), 3)
        response = self.client.post(f'/recordings/{recording}/save', json=body)
        self.assertEqual(response.status_code, 200, response.text)
        self.assertEqual(response.json()['id'], recording)
        self.assertNotIn(recording, runtime.active_recordings)
        self.assertFalse(any(command['recording_id'] == recording for command in runtime.pending_commands.values()))
        self.assertEqual(self.client.post(f'/recordings/{recording}/save', json=body).json()['id'], recording)
        self.assertEqual(len(self.client.get('/robots/robot1/routes').json()), 1)

    def test_missing_encoder_results_and_failed_executions_cannot_be_activated(self):
        recording, start = self.start()
        command = self.submit(recording); self.client.get('/robot/robot1/command')
        body = self.result(command); del body['movement']
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=body).status_code, 422)
        failed = self.result(command, status='failed'); failed['error'] = 'Encoder disconnected'
        self.assertEqual(self.client.post('/robot/robot1/command/result', json=failed).status_code, 200)
        response = self.client.post(f'/recordings/{recording}/save', json={'name': 'Incomplete', 'destination_checkpoint_id': start, 'status': 'active'})
        self.assertEqual(response.status_code, 409)
        self.assertIn(recording, runtime.active_recordings)

    def test_retry_clears_buffer_when_commit_succeeded_but_refresh_failed(self):
        recording, start, destination = self.completed_recording()
        body = {'name': 'Committed', 'destination_checkpoint_id': destination}
        with patch('sqlalchemy.orm.Session.refresh', side_effect=SQLAlchemyError('response failed after commit')):
            response = self.client.post(f'/recordings/{recording}/save', json=body)
        self.assertEqual(response.status_code, 503)
        self.assertIn(recording, runtime.active_recordings)
        response = self.client.post(f'/recordings/{recording}/save', json=body)
        self.assertEqual(response.status_code, 200)
        self.assertNotIn(recording, runtime.active_recordings)
        self.assertEqual(len(self.client.get('/robots/robot1/routes').json()), 1)

    def test_watchdog_queues_one_stop_without_inventing_encoder_results(self):
        with patch('atlas_runtime.time.time', return_value=100):
            recording, _ = self.start()
            submitted = self.submit(recording)
            first = self.client.get('/robot/robot1/command').json()['data']
        with patch('atlas_runtime.time.time', return_value=102):
            stop = self.client.get('/robot/robot1/command').json()['data']
            repeated = self.client.get('/robot/robot1/command').json()['data']
        self.assertEqual(stop['command'], 'stop')
        self.assertNotEqual(stop['command_id'], first['command_id'])
        self.assertEqual(stop['command_id'], repeated['command_id'])
        self.assertIsNone(runtime.pending_commands[submitted['command_id']]['result'])

    def test_activation_rejects_wrong_endpoints_and_missing_checkpoint_references(self):
        self.robot(); a, b = self.cp(), self.cp('C03'); self.connect(a, [b])
        self.assertEqual(self.client.post('/robots/robot1/routes', json=self.route(a, b, [self.segment(1, b)], 'active')).status_code, 409)
        self.assertEqual(self.client.post('/robots/robot1/routes', json=self.route(a, b, [self.segment(1, a), self.segment(2, str(uuid4())), self.segment(3, b)])).status_code, 404)
        self.assertEqual(self.client.post('/robots/robot1/routes', json={**self.route(a, b), 'estimated_duration': 10}).status_code, 422)

    def test_unsupplied_checkpoint_snapshot_is_not_guessed_during_save(self):
        recording, start, destination = self.completed_recording()
        self.confirm(recording, 3, destination, '2026-10-18T11:00:00Z')
        response = self.client.post(f'/recordings/{recording}/save', json={'name': 'Waiting', 'destination_checkpoint_id': destination})
        self.assertEqual(response.status_code, 409)
        self.assertIn(recording, runtime.active_recordings)
        self.assertEqual(self.client.get('/robots/robot1/routes').json(), [])

    def test_completed_route_does_not_persist_estimated_duration_or_speed(self):
        recording, start, destination = self.completed_recording()
        result = self.client.post(f'/recordings/{recording}/save', json={'name': 'Recorded', 'destination_checkpoint_id': destination}).json()
        self.assertEqual(len(result['route_segments']), 3)
        self.assertNotIn('duration', str(result))
        self.assertNotIn('speed', str(result))
        self.assertNotIn('route_steps', inspect(database.engine).get_table_names())

    def test_restart_keeps_historical_checkpoint_but_not_location_proof_or_buffer(self):
        recording, start = self.start(); self.execute(recording, 'stop'); self.confirm(recording, 1, start)
        runtime.clear()
        status = self.client.get('/robot/robot1/status').json()['data']
        self.assertEqual(status['residing_checkpoint_id'], start)
        self.assertFalse(status['checkpoint_location_verified'])
        self.assertFalse(status['drive_ready'])
        self.assertEqual(self.client.get(f'/recordings/{recording}').status_code, 404)

    def test_libris_book_and_shelf_operations_are_preserved(self):
        response = self.client.post('/shelves/', json={'shelf_name': 'Fiction', 'section': 'North', 'floor': 2})
        self.assertEqual(response.status_code, 201)
        shelf = response.json()['shelf_id']
        self.assertEqual(self.client.post('/book/register', json={'isbn': '9780593135204', 'title': 'Project Hail Mary', 'author': 'Andy Weir', 'shelf_id': shelf}).status_code, 200)
        self.assertEqual(len(self.client.get('/book/search/%25').json()), 1)
        self.assertEqual(self.client.delete(f'/shelves/{shelf}').status_code, 400)

    def test_postgresql_types_compile_to_finalized_schema(self):
        from sqlalchemy.schema import CreateTable
        from sqlalchemy.dialects import postgresql
        robot_sql = str(CreateTable(database.RobotModel.__table__).compile(dialect=postgresql.dialect()))
        checkpoint_sql = str(CreateTable(database.CheckpointModel.__table__).compile(dialect=postgresql.dialect()))
        route_sql = str(CreateTable(database.RouteModel.__table__).compile(dialect=postgresql.dialect()))
        self.assertIn('id TEXT', robot_sql)
        self.assertIn('UUID[]', checkpoint_sql)
        self.assertIn('JSONB', route_sql)
        self.assertIn('TIMESTAMP WITH TIME ZONE', route_sql)

    def test_migration_restructures_only_empty_legacy_atlas_tables(self):
        legacy = create_engine('sqlite://')
        try:
            with legacy.begin() as connection:
                connection.execute(text('CREATE TABLE robots (id TEXT PRIMARY KEY, robot_id TEXT, name TEXT)'))
                connection.execute(text('CREATE TABLE routes (id TEXT PRIMARY KEY, shelf_id INTEGER)'))
                connection.execute(text('CREATE TABLE route_steps (id INTEGER PRIMARY KEY, duration_ms INTEGER)'))
                connection.execute(text('CREATE TABLE unrelated_libris (id INTEGER PRIMARY KEY, value TEXT)'))
                connection.execute(text("INSERT INTO unrelated_libris VALUES (1, 'preserve')"))
            upgrade(legacy); upgrade(legacy)
            tables = inspect(legacy).get_table_names()
            self.assertNotIn('route_steps', tables)
            self.assertIn('checkpoints', tables)
            with legacy.connect() as connection:
                self.assertEqual(connection.execute(text('SELECT value FROM unrelated_libris')).scalar(), 'preserve')
                self.assertEqual(connection.execute(text('SELECT version FROM schema_migrations')).scalar(), VERSION)
        finally:
            legacy.dispose()

    def test_migration_refuses_to_invent_encoder_data_for_nonempty_legacy_routes(self):
        legacy = create_engine('sqlite://')
        try:
            with legacy.begin() as connection:
                connection.execute(text('CREATE TABLE robots (id TEXT PRIMARY KEY, robot_id TEXT)'))
                connection.execute(text("INSERT INTO robots VALUES ('old-uuid', 'robot1')"))
            with self.assertRaisesRegex(RuntimeError, 'contain records'):
                upgrade(legacy)
            with legacy.connect() as connection:
                self.assertEqual(connection.execute(text('SELECT robot_id FROM robots')).scalar(), 'robot1')
        finally:
            legacy.dispose()

if __name__ == '__main__':
    unittest.main()
