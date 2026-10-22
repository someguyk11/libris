from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from sqlalchemy.exc import IntegrityError
from database import get_db, ShelfModel, ShelfCreate, ShelfResponse, BookModel

router = APIRouter(prefix='/shelves', tags=['Shelves Subsystem'])

def find_shelf(db, shelf_id):
    shelf = db.query(ShelfModel).filter(ShelfModel.shelf_id == shelf_id).first()
    if not shelf:
        raise HTTPException(404, 'Shelf not found.')
    return shelf

def persist(db, shelf):
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(409, 'Shelf name already exists or the shelf is still assigned to books.')
    db.refresh(shelf)
    return shelf

@router.post('/', response_model=ShelfResponse, status_code=201)
def create_shelf(shelf: ShelfCreate, db: Session = Depends(get_db)):
    result = ShelfModel(**shelf.model_dump())
    db.add(result)
    return persist(db, result)

@router.get('/', response_model=list[ShelfResponse])
def get_all_shelves(db: Session = Depends(get_db)):
    return db.query(ShelfModel).all()

@router.get('/{shelf_id}', response_model=ShelfResponse)
def get_single_shelf(shelf_id: int, db: Session = Depends(get_db)):
    return find_shelf(db, shelf_id)

@router.put('/{shelf_id}', response_model=ShelfResponse)
def update_shelf(shelf_id: int, updated_data: ShelfCreate, db: Session = Depends(get_db)):
    shelf = find_shelf(db, shelf_id)
    for key, value in updated_data.model_dump().items():
        setattr(shelf, key, value)
    return persist(db, shelf)

@router.delete('/{shelf_id}')
def delete_shelf(shelf_id: int, db: Session = Depends(get_db)):
    shelf = find_shelf(db, shelf_id)
    count = db.query(BookModel).filter(BookModel.shelf_id == shelf_id).count()
    if count:
        raise HTTPException(400, f'Deletion rejected. {count} books are assigned to this shelf.')
    db.delete(shelf)
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(409, 'Shelf is still assigned to books.')
    return {'status': 'success', 'message': f'Shelf {shelf_id} deleted.'}
