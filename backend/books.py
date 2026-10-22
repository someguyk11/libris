"""LiBot registration semantics: an existing ISBN is reassigned, not edited."""
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from database import get_db, BookModel, ShelfModel, BookRegistration, BookResponse

router = APIRouter(tags=['Books Operations'])

@router.post('/book/register', response_model=BookResponse)
async def register_book(book: BookRegistration, db: Session = Depends(get_db)):
    if not db.query(ShelfModel).filter(ShelfModel.shelf_id == book.shelf_id).first():
        raise HTTPException(404, 'Choose a registered shelf.')
    existing = db.query(BookModel).filter(BookModel.isbn == book.isbn).first()
    if existing:
        existing.shelf_id = book.shelf_id
        result = existing
    else:
        result = BookModel(**book.model_dump())
        db.add(result)
    db.commit()
    db.refresh(result)
    return result

@router.get('/book/search/{query}', response_model=list[BookResponse])
async def search_books(query: str, db: Session = Depends(get_db)):
    if query.strip() == '%':
        return db.query(BookModel).all()
    pattern = f'%{query.lower().strip()}%'
    return db.query(BookModel).filter(BookModel.title.ilike(pattern) | BookModel.author.ilike(pattern) | BookModel.isbn.ilike(pattern)).all()
