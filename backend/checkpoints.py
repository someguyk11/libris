from uuid import UUID
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from database import get_db, CheckpointModel, RouteModel
from atlas_schemas import CheckpointWrite, CheckpointResponse
from atlas_validation import checkpoint, validate_route, route_data
from atlas_runtime import lock

router = APIRouter(prefix='/checkpoints', tags=['Atlas checkpoints'])

def invalidate_routes(db, removed=None):
    db.flush()
    for route in db.query(RouteModel).filter(RouteModel.status == 'active').all():
        try:
            validate_route(db, route_data(route))
        except HTTPException:
            route.status = 'disabled'
    if removed:
        for other in db.query(CheckpointModel).all():
            other.connected_checkpoint_ids = [value for value in other.connected_checkpoint_ids if value != removed]

@router.get('/', response_model=list[CheckpointResponse])
def list_checkpoints(db: Session = Depends(get_db)):
    return db.query(CheckpointModel).order_by(CheckpointModel.name).all()

@router.post('/', response_model=CheckpointResponse, status_code=201)
def create_checkpoint(body: CheckpointWrite, db: Session = Depends(get_db)):
    with lock:
        for identifier in body.connected_checkpoint_ids:
            checkpoint(db, identifier, active=True)
        value = CheckpointModel(**body.model_dump(mode='json'))
        db.add(value); db.commit(); db.refresh(value)
        return value

@router.get('/{checkpoint_id}', response_model=CheckpointResponse)
def get_checkpoint(checkpoint_id: UUID, db: Session = Depends(get_db)):
    return checkpoint(db, checkpoint_id)

@router.put('/{checkpoint_id}', response_model=CheckpointResponse)
def update_checkpoint(checkpoint_id: UUID, body: CheckpointWrite, db: Session = Depends(get_db)):
    with lock:
        value = checkpoint(db, checkpoint_id)
        for identifier in body.connected_checkpoint_ids:
            checkpoint(db, identifier, active=True)
        for key, field in body.model_dump(mode='json').items():
            setattr(value, key, field)
        invalidate_routes(db, value.id if not value.is_active else None)
        db.commit(); db.refresh(value)
        return value

@router.delete('/{checkpoint_id}')
def delete_checkpoint(checkpoint_id: UUID, db: Session = Depends(get_db)):
    with lock:
        value = checkpoint(db, checkpoint_id)
        value.is_active = False
        invalidate_routes(db, value.id)
        # Soft deletion preserves historical confirmed references and robot metadata.
        db.commit()
        return {'status': 'success', 'message': 'Checkpoint deactivated; inbound connections removed and affected active routes disabled. Historical references retained.'}
