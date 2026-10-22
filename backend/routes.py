"""Atlas V1 encoder JSON routes. Activation validates data; it never starts replay."""
from uuid import UUID
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from sqlalchemy.exc import IntegrityError
from database import get_db, RobotModel, RouteModel
from atlas_schemas import RouteCreate, RouteRename, RouteResponse
from atlas_validation import validate_route, route_data
from atlas_runtime import lock

router = APIRouter(tags=['Atlas routes'])

def find_route(db, route_id):
    route = db.get(RouteModel, str(route_id))
    if not route:
        raise HTTPException(404, 'Route not found.')
    return route

def save(db, route):
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(409, 'Route name already exists for this Atlas, or a referenced record changed.')
    db.refresh(route)
    return route

@router.get('/robots/{robot_id}/routes', response_model=list[RouteResponse])
def list_routes(robot_id: str, db: Session = Depends(get_db)):
    if not db.get(RobotModel, robot_id):
        raise HTTPException(404, 'Atlas not found.')
    return db.query(RouteModel).filter(RouteModel.robot_id == robot_id).order_by(RouteModel.created_at.desc()).all()

@router.post('/robots/{robot_id}/routes', response_model=RouteResponse, status_code=201)
def create_route(robot_id: str, body: RouteCreate, db: Session = Depends(get_db)):
    with lock:
        if not db.get(RobotModel, robot_id):
            raise HTTPException(404, 'Register this Atlas before saving a route.')
        validate_route(db, body)
        route = RouteModel(robot_id=robot_id, **body.model_dump(mode='json'))
        db.add(route)
        return save(db, route)

@router.get('/routes/{route_id}', response_model=RouteResponse)
def get_route(route_id: UUID, db: Session = Depends(get_db)):
    return find_route(db, route_id)

@router.put('/routes/{route_id}', response_model=RouteResponse)
def update_route(route_id: UUID, body: RouteCreate, db: Session = Depends(get_db)):
    with lock:
        route = find_route(db, route_id)
        validate_route(db, body)
        for key, value in body.model_dump(mode='json').items():
            setattr(route, key, value)
        # Editing/activation is not physical verification.
        route.last_verified_at = None
        return save(db, route)

@router.patch('/routes/{route_id}', response_model=RouteResponse)
def rename_route(route_id: UUID, body: RouteRename, db: Session = Depends(get_db)):
    with lock:
        route = find_route(db, route_id)
        for key, value in body.model_dump(exclude_none=True).items():
            setattr(route, key, value)
        validate_route(db, route_data(route))
        return save(db, route)

@router.delete('/routes/{route_id}')
def delete_route(route_id: UUID, db: Session = Depends(get_db)):
    with lock:
        db.delete(find_route(db, route_id))
        try:
            db.commit()
        except IntegrityError:
            db.rollback()
            raise HTTPException(409, 'Route is referenced by guidance history and cannot be deleted. Disable it instead.') from None
        return {'status': 'success', 'message': 'Route deleted.'}
