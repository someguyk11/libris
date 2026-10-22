"""Directed checkpoint validation, not localization or a navigation algorithm."""
from fastapi import HTTPException
from database import CheckpointModel
from atlas_schemas import RouteCreate

def checkpoint(db, identifier, active=False):
    value = db.get(CheckpointModel, str(identifier))
    if not value:
        raise HTTPException(404, 'Checkpoint not found.')
    if active and not value.is_active:
        raise HTTPException(409, 'Checkpoint is inactive.')
    return value

def route_data(route):
    try:
        return RouteCreate.model_validate({key: getattr(route, key) for key in ('name', 'start_checkpoint_id', 'destination_checkpoint_id', 'status', 'recording_version', 'route_segments')})
    except ValueError:
        raise HTTPException(409, 'Stored route data is invalid; replace it with a valid V1 recording before activation.')

def validate_route(db, body):
    active = body.status == 'active'
    start = checkpoint(db, body.start_checkpoint_id, active)
    destination = checkpoint(db, body.destination_checkpoint_id, active)
    confirmed = []
    for segment in body.route_segments:
        if segment.checkpoint_id is not None:
            confirmed.append(checkpoint(db, segment.checkpoint_id, active))
    if active:
        if not body.route_segments or not confirmed:
            raise HTTPException(409, 'Active routes require complete encoder segments and confirmed checkpoints.')
        if confirmed[0].id != start.id or confirmed[-1].id != destination.id:
            raise HTTPException(409, 'First and final confirmed checkpoints must match the route endpoints.')
        for previous, current in zip(confirmed, confirmed[1:]):
            if previous.id != current.id and current.id not in previous.connected_checkpoint_ids:
                raise HTTPException(409, f'Missing directed connection: {previous.name} → {current.name}.')

def references_checkpoint(route, identifier):
    return identifier in (route.start_checkpoint_id, route.destination_checkpoint_id) or any(segment.get('checkpoint_id') == identifier for segment in route.route_segments)
