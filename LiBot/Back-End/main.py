from fastapi import FastAPI, Depends, HTTPException, status, APIRouter
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy.orm import Session
from sqlalchemy import func
import httpx

# Database components
from database import engine, Base, get_db, BookModel, Library, Section, Shelf, BookLocationHistory
from schemas_book import BookRegistration, BookResponse, BookShelfUpdate, BookLocationHistoryResponse
from schemas_library import LibraryCreate, LibraryResponse, SectionCreate, SectionResponse, ShelfCreate, ShelfResponse

from robot import router as robot_router

# Build tables in Supabase automatically if missing (safe if tables already exist)
Base.metadata.create_all(bind=engine)

app = FastAPI(title="LiBot OS Backend")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(robot_router)


@app.get("/ping")
async def ping():
    return {"status": "alive"}


# ─── 1. REGISTER NEW BOOK (POST /book/register) ──────────────────────────────
@app.post("/book/register")
async def register_book(book: BookRegistration, db: Session = Depends(get_db)):
    # Normalize input
    isbn_clean = book.isbn.strip()
    title_clean = book.title.strip()
    author_clean = book.author.strip()
    # shelf_location and publish_date are accepted for API compatibility but not stored
    shelf_location_clean = book.shelf_location.strip() if book.shelf_location else None
    publish_date_clean = book.publish_date.strip() if book.publish_date else "Date Unknown"

    # Determine shelf_id from request
    shelf_id_to_set = None
    if book.shelf_id is not None:
        # book.shelf_id is already a UUID object (validated by Pydantic)
        shelf_id_to_set = book.shelf_id
    elif shelf_location_clean:
        # Try to find a shelf by name or code
        shelf = db.query(Shelf).filter(
            (Shelf.name == shelf_location_clean) | (Shelf.code == shelf_location_clean)
        ).first()
        if shelf is None:
            raise HTTPException(
                status_code=400,
                detail=f"Shelf location '{shelf_location_clean}' does not match any existing shelf."
            )
        shelf_id_to_set = shelf.id  # This is a UUID object
    # If neither is provided, shelf_id_to_set remains None
    # Validate that the shelf exists if we have a shelf_id_to_set
    if shelf_id_to_set is not None:
        shelf_obj = db.query(Shelf).filter(Shelf.id == shelf_id_to_set).first()
        if shelf_obj is None:
            raise HTTPException(
                status_code=400,
                detail=f"Shelf with id '{shelf_id_to_set}' does not exist."
            )

    # Check if book is already registered in PostgreSQL
    existing_book = db.query(BookModel).filter(BookModel.isbn == isbn_clean).first()
    if existing_book:
        # Since we cannot store shelf_location/publish_date in the books table,
        # we treat this as a no-op (location is managed via book_copies & shelves).
        # Return success with echoed input.
        # Prepare shelf_location string and shelf object from the assigned shelf
        response_shelf_location = None
        response_shelf = None
        if existing_book.shelf_id:
            shelf_obj = db.query(Shelf).filter(Shelf.id == existing_book.shelf_id).first()
            if shelf_obj:
                response_shelf_location = shelf_obj.name  # or code? We'll use name for consistency
                response_shelf = {
                    "id": str(shelf_obj.id),
                    "name": shelf_obj.name,
                    "code": shelf_obj.code,
                    "section": {
                        "id": str(shelf_obj.section.id),
                        "name": shelf_obj.section.name,
                        "code": shelf_obj.section.code,
                    }
                }

        return {
            "status": 200,
            "message": "Book already registered; location is managed via book_copies & shelves.",
            "data": {
                "isbn": existing_book.isbn,
                "title": existing_book.title,
                "author": existing_book.author,
                "publish_date": publish_date_clean,  # echo back
                "shelf_location": shelf_location_clean,  # echo back (for API compatibility)
                "shelf_id": str(existing_book.shelf_id) if existing_book.shelf_id else None,
                # extra fields from DB
                "publisher": existing_book.publisher,
                "edition": existing_book.edition,
                "language": existing_book.language,
                "category": existing_book.category,
                "description": existing_book.description,
                "cover_image_url": existing_book.cover_image_url,
                "shelf": response_shelf,
            }
        }

    # Save a brand new entry to PostgreSQL (only fields that exist in the table)
    new_book = BookModel(
        isbn=isbn_clean,
        title=title_clean,
        author=author_clean,
        shelf_id=shelf_id_to_set,
        # other columns get defaults or remain nullable
    )
    db.add(new_book)
    db.commit()
    db.refresh(new_book)

    # Prepare shelf_location string for response (based on assigned shelf)
    response_shelf_location = None
    response_shelf = None
    if new_book.shelf_id:
        shelf_obj = db.query(Shelf).filter(Shelf.id == new_book.shelf_id).first()
        if shelf_obj:
            response_shelf_location = shelf_obj.name  # or code? We'll use name for consistency
            response_shelf = {
                "id": str(shelf_obj.id),
                "name": shelf_obj.name,
                "code": shelf_obj.code,
                "section": {
                    "id": str(shelf_obj.section.id),
                    "name": shelf_obj.section.name,
                    "code": shelf_obj.section.code,
                }
            }

    # Return data matching expected API shape
    return {
        "status": 200,
        "message": "Book logged successfully into PostgreSQL database.",
        "data": {
            "isbn": new_book.isbn,
            "title": new_book.title,
            "author": new_book.author,
            "publish_date": publish_date_clean,  # echo back
            "shelf_location": response_shelf_location,  # echo back (now based on assigned shelf)
            "shelf_id": str(new_book.shelf_id) if new_book.shelf_id else None,
            # extra fields from DB (null for new record)
            "publisher": new_book.publisher,
            "edition": new_book.edition,
            "language": new_book.language,
            "category": new_book.category,
            "description": new_book.description,
            "cover_image_url": new_book.cover_image_url,
            "shelf": response_shelf,
        }
    }


# ─── 2. EXTRACT BOOK BY ISBN (GET /book/{isbn}) ──────────────────────────────
@app.get("/book/{isbn}")
async def get_book(isbn: str, db: Session = Depends(get_db)):
    # Look up in PostgreSQL first
    local_book = db.query(BookModel).filter(BookModel.isbn == isbn).first()
    if local_book:
        # Prepare shelf_location string and shelf object from the assigned shelf
        response_shelf_location = None
        response_shelf = None
        if local_book.shelf_id:
            shelf_obj = db.query(Shelf).filter(Shelf.id == local_book.shelf_id).first()
            if shelf_obj:
                response_shelf_location = shelf_obj.name  # or code? We'll use name for consistency
                response_shelf = {
                    "id": str(shelf_obj.id),
                    "name": shelf_obj.name,
                    "code": shelf_obj.code,
                    "section": {
                        "id": str(shelf_obj.section.id),
                        "name": shelf_obj.section.name,
                        "code": shelf_obj.section.code,
                    }
                }

        return {
            "status": 200,
            "data": {
                "isbn": local_book.isbn,
                "title": local_book.title,
                "author": local_book.author,
                "publish_date": None,  # column does not exist
                "shelf_location": response_shelf_location,  # now based on assigned shelf
                "shelf_id": str(local_book.shelf_id) if local_book.shelf_id else None,
                # extra fields from DB
                "publisher": local_book.publisher,
                "edition": local_book.edition,
                "language": local_book.language,
                "category": local_book.category,
                "description": local_book.description,
                "cover_image_url": local_book.cover_image_url,
                "shelf": response_shelf,
            }
        }

    # Fallback: Query Open Library if missing from database
    url = f"https://openlibrary.org/isbn/{isbn}.json"
    async with httpx.AsyncClient(timeout=5.0) as client:
        try:
            response = await client.get(url, follow_redirects=True)
            if response.status_code == 404:
                return {"status": 404, "message": "Book not found anywhere."}
            elif response.status_code != 200:
                return {"status": response.status_code, "message": "Failed to fetch from Open Library."}

            open_lib_data = response.json()
            formatted_book = {
                "isbn": isbn,
                "title": open_lib_data.get("title", "Untitled Document"),
                "author": open_lib_data.get("by_statement", "Unknown Author"),
                "publish_date": open_lib_data.get("publish_date", "Date Unknown"),
                "shelf_location": "Unassigned Matrix",
                "shelf_id": None,
                # extra fields not known from Open Library
                "publisher": None,
                "edition": None,
                "language": None,
                "category": None,
                "description": None,
                "cover_image_url": None,
                "shelf": None,
            }
            return {"status": 200, "data": formatted_book}
        except httpx.RequestError as e:
            return {"status": 500, "message": f"Connection error: {str(e)}"}


# ─── 3. REPOSITORY TEXT INDEX SEARCH ──────────────────────────────────────────
@app.get("/book/search/{query}")
async def search_books(query: str, db: Session = Depends(get_db)):
    query_clean = f"%{query.lower().strip()}%"
    results = db.query(BookModel).filter(
        (BookModel.title.ilike(query_clean)) |
        (BookModel.author.ilike(query_clean)) |
        (BookModel.isbn.ilike(query_clean))
    ).all()

    # Format list response to keep frontend components uniform
    books_list = []
    for b in results:
        # Prepare shelf_location string and shelf object from the assigned shelf
        shelf_location_str = None
        shelf_obj_dict = None
        if b.shelf_id:
            shelf_obj = db.query(Shelf).filter(Shelf.id == b.shelf_id).first()
            if shelf_obj:
                shelf_location_str = shelf_obj.name  # or code? We'll use name for consistency
                shelf_obj_dict = {
                    "id": str(shelf_obj.id),
                    "name": shelf_obj.name,
                    "code": shelf_obj.code,
                    "section": {
                        "id": str(shelf_obj.section.id),
                        "name": shelf_obj.section.name,
                        "code": shelf_obj.section.code,
                    }
                }
        books_list.append({
            "isbn": b.isbn,
            "title": b.title,
            "author": b.author,
            "publish_date": None,  # column does not exist
            "shelf_location": shelf_location_str,  # now based on assigned shelf
            "shelf_id": str(b.shelf_id) if b.shelf_id else None,
            # extra fields from DB
            "publisher": b.publisher,
            "edition": b.edition,
            "language": b.language,
            "category": b.category,
            "description": b.description,
            "cover_image_url": b.cover_image_url,
            "shelf": shelf_obj_dict,
        })
    return books_list

# Routers for library hierarchy
library_router = APIRouter(prefix="/libraries", tags=["libraries"])
section_router = APIRouter(prefix="/sections", tags=["sections"])
shelf_router = APIRouter(prefix="/shelves", tags=["shelves"])

# Libraries
@library_router.post("/", response_model=LibraryResponse, status_code=status.HTTP_201_CREATED)
def create_library(library: LibraryCreate, db: Session = Depends(get_db)):
    db_lib = Library(**library.dict())
    db.add(db_lib)
    db.commit()
    db.refresh(db_lib)
    return db_lib

@library_router.get("/{library_id}", response_model=LibraryResponse)
def get_library(library_id: str, db: Session = Depends(get_db)):
    db_lib = db.query(Library).filter(Library.id == library_id).first()
    if not db_lib:
        raise HTTPException(status_code=404, detail="Library not found")
    return db_lib

@library_router.get("/", response_model=list[LibraryResponse])
def list_libraries(db: Session = Depends(get_db)):
    return db.query(Library).all()

# Sections
@section_router.post("/", response_model=SectionResponse, status_code=status.HTTP_201_CREATED)
def create_section(section: SectionCreate, db: Session = Depends(get_db)):
    # Verify library exists
    lib = db.query(Library).filter(Library.id == section.library_id).first()
    if not lib:
        raise HTTPException(status_code=400, detail="Library not found")
    db_sec = Section(**section.dict())
    db.add(db_sec)
    db.commit()
    db.refresh(db_sec)
    return db_sec

@section_router.get("/{section_id}", response_model=SectionResponse)
def get_section(section_id: str, db: Session = Depends(get_db)):
    db_sec = db.query(Section).filter(Section.id == section_id).first()
    if not db_sec:
        raise HTTPException(status_code=404, detail="Section not found")
    return db_sec

@section_router.get("/", response_model=list[SectionResponse])
def list_sections(db: Session = Depends(get_db)):
    return db.query(Section).all()

@section_router.get("/by-library/{library_id}", response_model=list[SectionResponse])
def list_sections_by_library(library_id: str, db: Session = Depends(get_db)):
    return db.query(Section).filter(Section.library_id == library_id).all()

# Shelves
@shelf_router.post("/", response_model=ShelfResponse, status_code=status.HTTP_201_CREATED)
def create_shelf(shelf: ShelfCreate, db: Session = Depends(get_db)):
    # Verify section exists
    sec = db.query(Section).filter(Section.id == shelf.section_id).first()
    if not sec:
        raise HTTPException(status_code=400, detail="Section not found")
    db_sh = Shelf(**shelf.dict())
    db.add(db_sh)
    db.commit()
    db.refresh(db_sh)
    return db_sh

@shelf_router.get("/{shelf_id}", response_model=ShelfResponse)
def get_shelf(shelf_id: str, db: Session = Depends(get_db)):
    db_sh = db.query(Shelf).filter(Shelf.id == shelf_id).first()
    if not db_sh:
        raise HTTPException(status_code=404, detail="Shelf not found")
    return db_sh

@shelf_router.get("/", response_model=list[ShelfResponse])
def list_shelves(db: Session = Depends(get_db)):
    return db.query(Shelf).all()

@shelf_router.get("/{section_id}/test")
def section_test(section_id: str, db: Session = Depends(get_db)):
    return {"section_id": section_id}
# Book router
book_router = APIRouter(prefix="/book", tags=["books"])

@book_router.get("/{isbn}/location-history")
def get_book_location_history(isbn: str, db: Session = Depends(get_db)):
    # Find the book by ISBN
    book = db.query(BookModel).filter(BookModel.isbn == isbn).first()
    if not book:
        raise HTTPException(status_code=404, detail="Book not found")
    
    # Query all location history records for this book, ordered by newest first
    history_records = db.query(BookLocationHistory).filter(BookLocationHistory.book_id == book.book_id).order_by(BookLocationHistory.changed_at.desc()).all()
    
    # Convert to response format
    history_list = []
    for record in history_records:
        # Get shelf information for readability
        from_shelf_name = None
        from_shelf_code = None
        to_shelf_name = None
        to_shelf_code = None
        
        if record.from_shelf:
            from_shelf_name = record.from_shelf.name
            from_shelf_code = record.from_shelf.code
            
        if record.to_shelf:
            to_shelf_name = record.to_shelf.name
            to_shelf_code = record.to_shelf.code
        
        history_list.append({
            "id": str(record.id),
            "book_id": str(record.book_id),
            "from_shelf_id": str(record.from_shelf_id) if record.from_shelf_id else None,
            "to_shelf_id": str(record.to_shelf_id),
            "changed_at": record.changed_at,
            "reason": record.reason,
            "from_shelf_name": from_shelf_name,
            "from_shelf_code": from_shelf_code,
            "to_shelf_name": to_shelf_name,
            "to_shelf_code": to_shelf_code
        })
    
    return history_list


@book_router.get("/debugput/{isbn}")
def debug_put_endpoint(isbn: str, db: Session = Depends(get_db)):
    # Find the book by ISBN
    book = db.query(BookModel).filter(BookModel.isbn == isbn).first()
    if not book:
        raise HTTPException(status_code=404, detail="Book not found")
    # Validate that the shelf exists
    shelf_id = "d0661078-d82d-462f-88fb-3a2e2cdb9c34"
    shelf = db.query(Shelf).filter(Shelf.id == shelf_id).first()
    if not shelf:
        raise HTTPException(status_code=404, detail="Shelf not found")
    # Update the book's shelf_id
    book.shelf_id = shelf_id
    db.commit()
    db.refresh(book)
    return {
        "status": 200,
        "message": "Debug PUT successful",
        "data": {
            "isbn": book.isbn,
            "shelf_id": str(book.shelf_id) if book.shelf_id else None
        }
    }


@book_router.get("/debug-root")
def debug_root():
    return {"message": "debug root works"}


@book_router.get("/{bookId}/test")
def debug_test(bookId: str, db: Session = Depends(get_db)):
    return {"bookId": bookId}


# @book_router.get("/{isbn}/history")
# def debug_history(isbn: str, db: Session = Depends(get_db)):
#     return {"isbn": isbn}


@book_router.put("/{isbn}")
def update_book_shelf(isbn: str, book_update: BookShelfUpdate, db: Session = Depends(get_db)):
    # Find the book by ISBN
    book = db.query(BookModel).filter(BookModel.isbn == isbn).first()
    if not book:
        raise HTTPException(status_code=404, detail="Book not found")
    # Validate that the shelf exists
    shelf = db.query(Shelf).filter(Shelf.id == book_update.shelf_id).first()
    if not shelf:
        raise HTTPException(status_code=404, detail="Shelf not found")
    
    # Handle no-op moves (book already on the requested shelf)
    if book.shelf_id == book_update.shelf_id:
        # Return the book without creating a history record
        response_shelf_location = None
        response_shelf = None
        if book.shelf_id:
            shelf_obj = db.query(Shelf).filter(Shelf.id == book.shelf_id).first()
            if shelf_obj:
                response_shelf_location = shelf_obj.name
                response_shelf = {
                    "id": str(shelf_obj.id),
                    "name": shelf_obj.name,
                    "code": shelf_obj.code,
                    "section": {
                        "id": str(shelf_obj.section.id),
                        "name": shelf_obj.section.name,
                        "code": shelf_obj.section.code,
                    }
                }

        return {
            "status": 200,
            "message": "Book is already on the specified shelf.",
            "data": {
                "isbn": book.isbn,
                "title": book.title,
                "author": book.author,
                "publish_date": None,  # column does not exist
                "shelf_location": response_shelf_location,
                "shelf_id": str(book.shelf_id) if book.shelf_id else None,
                # extra fields from DB
                "publisher": book.publisher,
                "edition": book.edition,
                "language": book.language,
                "category": book.category,
                "description": book.description,
                "cover_image_url": book.cover_image_url,
                "shelf": response_shelf,
            }
        }
    
    # Update the book's shelf_id and create history record
    old_shelf_id = book.shelf_id
    book.shelf_id = book_update.shelf_id
    
    # Create history record for the shelf change
    history_record = BookLocationHistory(
        book_id=book.book_id,
        from_shelf_id=old_shelf_id,
        to_shelf_id=book_update.shelf_id,
        changed_at=func.now(),
        reason=None  # Could be set based on context if needed
    )
    db.add(history_record)
    db.commit()
    db.refresh(book)
    # Prepare shelf_location string and shelf object from the assigned shelf
    response_shelf_location = None
    response_shelf = None
    if book.shelf_id:
        shelf_obj = db.query(Shelf).filter(Shelf.id == book.shelf_id).first()
        if shelf_obj:
            response_shelf_location = shelf_obj.name  # or code? We'll use name for consistency
            response_shelf = {
                "id": str(shelf_obj.id),
                "name": shelf_obj.name,
                "code": shelf_obj.code,
                "section": {
                    "id": str(shelf_obj.section.id),
                    "name": shelf_obj.section.name,
                    "code": shelf_obj.section.code,
                }
            }

    return {
        "status": 200,
        "message": "Book shelf updated successfully.",
        "data": {
            "isbn": book.isbn,
            "title": book.title,
            "author": book.author,
            "publish_date": None,  # column does not exist
            "shelf_location": response_shelf_location,
            "shelf_id": str(book.shelf_id) if book.shelf_id else None,
            # extra fields from DB
            "publisher": book.publisher,
            "edition": book.edition,
            "language": book.language,
            "category": book.category,
            "description": book.description,
            "cover_image_url": book.cover_image_url,
            "shelf": response_shelf,
        }
    }

@book_router.get("/shelves/{shelf_id}/books")
def get_books_on_shelf(shelf_id: str, db: Session = Depends(get_db)):
    # Validate that the shelf exists
    shelf = db.query(Shelf).filter(Shelf.id == shelf_id).first()
    if not shelf:
        raise HTTPException(status_code=404, detail="Shelf not found")
    # Query books on this shelf
    books = db.query(BookModel).filter(BookModel.shelf_id == shelf_id).all()
    books_list = []
    for b in books:
        # Prepare shelf_location string and shelf object from the assigned shelf
        shelf_location_str = None
        shelf_obj_dict = None
        if b.shelf_id:
            shelf_obj = db.query(Shelf).filter(Shelf.id == b.shelf_id).first()
            if shelf_obj:
                shelf_location_str = shelf_obj.name  # or code? We'll use name for consistency
                shelf_obj_dict = {
                    "id": str(shelf_obj.id),
                    "name": shelf_obj.name,
                    "code": shelf_obj.code,
                    "section": {
                        "id": str(shelf_obj.section.id),
                        "name": shelf_obj.section.name,
                        "code": shelf_obj.section.code,
                    }
                }
        books_list.append({
            "isbn": b.isbn,
            "title": b.title,
            "author": b.author,
            "publish_date": None,  # column does not exist
            "shelf_location": shelf_location_str,
            "shelf_id": str(b.shelf_id) if b.shelf_id else None,
            # extra fields from DB
            "publisher": b.publisher,
            "edition": b.edition,
            "language": b.language,
            "category": b.category,
            "description": b.description,
            "cover_image_url": b.cover_image_url,
            "shelf": shelf_obj_dict,
        })
    return books_list
 
 
@book_router.get("/test/fixed/{param}")
def test_double_fixed(param: str, db: Session = Depends(get_db)):
    return {"param": param}





@book_router.get("/test-endpoint")
def test_endpoint():
    return {"message": "test endpoint works"}




# Include routers
app.include_router(library_router)
app.include_router(section_router)
app.include_router(shelf_router)
app.include_router(book_router)