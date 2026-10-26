"""Existing catalogue registration with explicit safe scanning/bulk entry points."""
from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy import func, text
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session
from database import get_db, BookModel, ShelfModel, BookRegistration, BookResponse, BulkBookRegistration, ExistingBookReference
from guidance_access import staff
from isbn import isbn13, isbn10
from book_metadata import metadata_service, MetadataError

router = APIRouter(tags=['Books Operations'])


def lock_editions(db, values):
    if db.get_bind().dialect.name == 'postgresql':
        # Stable ordering avoids deadlocks between overlapping batches. Unique ISBN
        # remains the final constraint; locking also covers legacy ISBN-10 aliases.
        for value in sorted(set(values)):
            db.execute(text('SELECT pg_advisory_xact_lock(hashtext(:isbn))'), {'isbn': value})


def find_edition(db, canonical):
    aliases = [canonical, isbn10(canonical)]
    compact = func.upper(func.replace(func.replace(BookModel.isbn, '-', ''), ' ', ''))
    matches = db.query(BookModel).filter(compact.in_([v for v in aliases if v])).all()
    if len(matches) > 1:
        raise HTTPException(409, 'Multiple existing records identify this edition. Resolve the catalogue duplicates before registering it.')
    return matches[0] if matches else None


def require_shelf(db, shelf_id):
    if not db.query(ShelfModel).filter(ShelfModel.shelf_id == shelf_id).first():
        raise HTTPException(404, 'Choose a registered shelf.')


@router.get('/book/metadata/{value}', dependencies=[Depends(staff)])
async def lookup_metadata(value: str, db: Session = Depends(get_db)):
    try:
        canonical = isbn13(value)
    except ValueError as error:
        raise HTTPException(422, str(error)) from None
    existing = find_edition(db, canonical)
    # Local information wins. Do not unnecessarily contact a provider for a known ISBN.
    if existing:
        return {'isbn': canonical, 'found': True, 'metadata': {}, 'existing': BookResponse.model_validate(existing)}
    try:
        return {**await metadata_service.lookup(canonical), 'existing': None}
    except MetadataError as error:
        raise HTTPException(error.status, error.message, headers={'Retry-After': '60'} if error.status == 429 else None) from None


@router.post('/book/register', response_model=BookResponse)
async def register_book(book: BookRegistration, db: Session = Depends(get_db)):
    require_shelf(db, book.shelf_id)
    lock_editions(db, [book.isbn])
    existing = find_edition(db, book.isbn)
    if existing:
        if book.require_new:
            raise HTTPException(409, 'This edition is already registered. Review its existing shelf instead of creating another copy.')
        if book.expected_existing_id is not None and existing.id != book.expected_existing_id:
            raise HTTPException(409, 'The existing book changed. Refresh and review it again.')
        existing.shelf_id = book.shelf_id
        result = existing
    else:
        if book.expected_existing_id is not None:
            raise HTTPException(409, 'The existing book no longer exists. Refresh before continuing.')
        result = BookModel(**book.model_dump(exclude={'require_new', 'expected_existing_id'}))
        db.add(result)
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(409, 'The ISBN or shelf changed while saving. Refresh the catalogue before retrying.') from None
    db.refresh(result)
    return result


@router.post('/book/register-bulk')
def register_bulk(batch: BulkBookRegistration, db: Session = Depends(get_db)):
    require_shelf(db, batch.shelf_id)
    lock_editions(db, [book.isbn for book in batch.books])
    outcomes = []
    seen = set()
    try:
        for book in batch.books:
            if book.isbn in seen:
                outcomes.append({'isbn': book.isbn, 'status': 'duplicate', 'book': None})
                continue
            seen.add(book.isbn)
            existing = find_edition(db, book.isbn)
            if isinstance(book, ExistingBookReference) and (not existing or existing.id != book.existing_id):
                raise HTTPException(409, 'Nothing in this batch was saved. An existing edition changed; look it up again before retrying.')
            if existing:
                # Bulk retries are safe. Never move or overwrite an existing title.
                outcomes.append({'isbn': book.isbn, 'status': 'existing', 'book': existing})
                continue
            result = BookModel(**book.model_dump(), shelf_id=batch.shelf_id)
            db.add(result)
            db.flush()
            outcomes.append({'isbn': book.isbn, 'status': 'created', 'book': result})
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(409, 'Nothing in this batch was saved. An ISBN or shelf changed; refresh and retry the batch.') from None
    except Exception:
        db.rollback()
        raise
    return {'shelf_id': batch.shelf_id, 'results': [
        {**item, 'book': BookResponse.model_validate(item['book']) if item['book'] else None} for item in outcomes
    ]}

@router.get('/book/search/{query}', response_model=list[BookResponse])
def search_books(query: str, limit: int = Query(50, ge=1, le=100), offset: int = Query(0, ge=0, le=100000), db: Session = Depends(get_db)):
    query = query.strip()
    if not query or len(query) > 200:
        raise HTTPException(422, 'Search must contain between 1 and 200 characters.')
    books = db.query(BookModel)
    if query != '%':
        pattern = '%' + query.replace('\\', '\\\\').replace('%', '\\%').replace('_', '\\_') + '%'
        books = books.filter(BookModel.title.ilike(pattern, escape='\\') | BookModel.author.ilike(pattern, escape='\\') | BookModel.isbn.ilike(pattern, escape='\\'))
    return books.order_by(func.lower(BookModel.title), BookModel.id).offset(offset).limit(limit).all()


@router.get('/book/{book_id}', response_model=BookResponse)
def get_book(book_id: int, db: Session = Depends(get_db)):
    book = db.get(BookModel, book_id)
    if not book:
        raise HTTPException(404, 'Book not found.')
    return book
