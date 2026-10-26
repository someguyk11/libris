"""Atlas V1 wire contract. Missing measured values are never defaulted to zero."""
from datetime import datetime, timezone
from enum import Enum
from typing import Literal, Optional
from uuid import UUID
from pydantic import AliasChoices, BaseModel, ConfigDict, Field, computed_field, model_validator

class StrictModel(BaseModel):
    model_config = ConfigDict(extra='forbid', str_strip_whitespace=True, allow_inf_nan=False)

class RobotCommandType(str, Enum):
    forward = 'forward'
    backward = 'backward'
    left = 'left'
    right = 'right'
    left_turn = 'left_turn'
    right_turn = 'right_turn'
    stop = 'stop'

class RobotCreate(StrictModel):
    # robot_id remains a supported registration alias for the existing website.
    id: str = Field(validation_alias=AliasChoices('id', 'robot_id'), pattern=r'^[A-Za-z0-9-]{1,80}$')
    name: str = Field(min_length=1, max_length=100)
    firmware_version: Optional[str] = Field(default=None, max_length=100)

class RobotUpdate(StrictModel):
    name: str = Field(min_length=1, max_length=100)

class RobotResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: str
    name: str
    residing_checkpoint_id: Optional[UUID]
    last_sequence_number: Optional[int]
    firmware_version: Optional[str]
    created_at: datetime
    updated_at: datetime

    @computed_field
    @property
    def robot_id(self) -> str:
        return self.id

class CheckpointWrite(StrictModel):
    name: str = Field(min_length=1, max_length=100)
    location_id: Optional[UUID] = None
    connected_checkpoint_ids: list[UUID] = Field(default_factory=list, max_length=500)
    is_active: bool = True

    @model_validator(mode='after')
    def unique_connections(self):
        if len(set(self.connected_checkpoint_ids)) != len(self.connected_checkpoint_ids):
            raise ValueError('Checkpoint connections must not be duplicated.')
        return self

class CheckpointResponse(CheckpointWrite):
    model_config = ConfigDict(from_attributes=True)
    id: UUID
    created_at: datetime
    updated_at: datetime

class Movement(StrictModel):
    left_encoder_ticks: int = Field(strict=True, ge=0)
    right_encoder_ticks: int = Field(strict=True, ge=0)
    left_direction: Literal['forward', 'backward', 'stop']
    right_direction: Literal['forward', 'backward', 'stop']

    @model_validator(mode='after')
    def stationary_wheels(self):
        for side in ('left', 'right'):
            if getattr(self, side + '_direction') == 'stop' and getattr(self, side + '_encoder_ticks') != 0:
                raise ValueError('A stopped wheel cannot have nonzero encoder ticks.')
        return self

class SensorReadings(StrictModel):
    front_cm: Optional[float] = Field(default=None, ge=0)
    back_cm: Optional[float] = Field(default=None, ge=0)
    left_cm: Optional[float] = Field(default=None, ge=0)
    right_cm: Optional[float] = Field(default=None, ge=0)

class RouteSegment(StrictModel):
    sequence: int = Field(strict=True, ge=1)
    checkpoint_id: Optional[UUID]
    movement: Movement
    checkpoint_sensor_readings: Optional[SensorReadings] = None
    # Legacy routes remain readable, but cannot drive protocol 3 until re-recorded.
    motion_kind: Literal['forward', 'backward', 'left', 'right', 'stop'] | None = None
    turn_degrees: int | None = Field(default=None, strict=True)

    @model_validator(mode='after')
    def checkpoint_snapshot_only(self):
        if self.checkpoint_sensor_readings is not None and self.checkpoint_id is None:
            raise ValueError('Permanent sensor snapshots require a confirmed checkpoint.')
        if self.motion_kind in ('left', 'right'):
            if self.turn_degrees != 90:
                raise ValueError('Atlas V1 supports exactly 90 degree turns.')
        elif self.turn_degrees is not None:
            raise ValueError('Turn angle requires a turn kind.')
        if self.motion_kind is not None:
            validate_measured_motion(self.motion_kind, self.movement)
        return self

def validate_measured_motion(kind, movement):
    """Unsigned single-signal counts cannot establish direction; bind to command."""
    directions = {'forward': ('forward', 'forward'), 'backward': ('backward', 'backward'),
                  'left': ('backward', 'forward'), 'right': ('forward', 'backward'),
                  'stop': ('stop', 'stop')}
    expected = directions[kind.replace('_turn', '')]
    actual = (movement.left_direction, movement.right_direction)
    if kind == 'stop':
        if movement.left_encoder_ticks or movement.right_encoder_ticks or actual != expected:
            raise ValueError('Stop completion requires zero measured motion.')
    elif actual != expected or not movement.left_encoder_ticks or not movement.right_encoder_ticks:
        raise ValueError('Measured wheel directions/counts do not match the command.')

class RouteCreate(StrictModel):
    name: str = Field(min_length=1, max_length=100)
    start_checkpoint_id: UUID
    destination_checkpoint_id: UUID
    status: Literal['draft', 'active', 'disabled'] = 'draft'
    recording_version: Literal[1] = 1
    route_segments: list[RouteSegment] = Field(default_factory=list, max_length=5000)

    @model_validator(mode='after')
    def ordered_sequences(self):
        sequences = [segment.sequence for segment in self.route_segments]
        if any(a >= b for a, b in zip(sequences, sequences[1:])):
            raise ValueError('Sequences must be unique and strictly increasing in their original order.')
        return self

class RouteRename(StrictModel):
    name: Optional[str] = Field(default=None, min_length=1, max_length=100)
    status: Optional[Literal['draft', 'active', 'disabled']] = None

class RouteResponse(RouteCreate):
    model_config = ConfigDict(from_attributes=True)
    id: UUID
    robot_id: str
    last_verified_at: Optional[datetime]
    created_at: datetime
    updated_at: datetime

class CommandSubmission(StrictModel):
    command: RobotCommandType
    request_id: Optional[UUID] = None
    recording_id: Optional[UUID] = None

class ExecutionResult(StrictModel):
    command_id: UUID
    recording_id: Optional[UUID] = None
    sequence: int = Field(strict=True, ge=1)
    status: Literal['started', 'completed', 'failed']
    captured_at: datetime
    movement: Optional[Movement] = None
    sensor_readings: Optional[SensorReadings] = None
    error: Optional[str] = Field(default=None, max_length=500)

    @model_validator(mode='after')
    def measured_completion(self):
        if self.captured_at.tzinfo is None:
            raise ValueError('captured_at must include a timezone.')
        self.captured_at = self.captured_at.astimezone(timezone.utc)
        if self.status == 'completed' and self.movement is None:
            raise ValueError('Completed commands require measured left/right encoder differences and directions.')
        return self

class SensorSample(StrictModel):
    command_id: UUID
    recording_id: UUID
    sequence: int = Field(strict=True, ge=1)
    captured_at: datetime
    sensor_readings: SensorReadings

    @model_validator(mode='after')
    def timestamp(self):
        if self.captured_at.tzinfo is None:
            raise ValueError('captured_at must include a timezone.')
        self.captured_at = self.captured_at.astimezone(timezone.utc)
        return self

class RecordingStart(StrictModel):
    start_checkpoint_id: UUID

class CheckpointConfirmation(StrictModel):
    sequence: int = Field(strict=True, ge=1)
    checkpoint_id: UUID
    confirmation_basis: Literal['operator_confirmed']
    sensor_captured_at: Optional[datetime] = None

    @model_validator(mode='after')
    def timestamp(self):
        if self.sensor_captured_at is not None and self.sensor_captured_at.tzinfo is None:
            raise ValueError('sensor_captured_at must include a timezone.')
        return self

class RecordingSave(StrictModel):
    name: str = Field(min_length=1, max_length=100)
    destination_checkpoint_id: UUID
    status: Literal['draft', 'active'] = 'draft'
