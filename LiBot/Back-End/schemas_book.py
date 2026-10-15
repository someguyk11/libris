from pydantic import BaseModel
from typing import Optional
from datetime import datetime
import uuid

class BookRegistration(BaseModel):
    isbn: str
    title: str
    author: str
    publish_date: Optional[str] = "Date Unknown"
    shelf_location: Optional[str] = None  # kept for API compatibility; not stored in DB
    shelf_id: Optional[uuid.UUID] = None  # UUID for assigning to a logical shelf

class BookResponse(BaseModel):
    isbn: str
    title: str
    author: str
    publish_date: Optional[str]
    shelf_location: Optional[str] = None
    shelf_id: Optional[str] = None
    # extra fields from DB
    publisher: Optional[str] = None
    edition: Optional[str] = None
    language: Optional[str] = None
    category: Optional[str] = None
    description: Optional[str] = None
    cover_image_url: Optional[str] = None
    # structured shelf information
    shelf: Optional[dict] = None

    class Config:
        orm_mode = True

class BookShelfUpdate(BaseModel):
    shelf_id: uuid.UUID

class BookLocationHistoryResponse(BaseModel):
    id: str
    book_id: str
    from_shelf_id: Optional[str] = None
    to_shelf_id: str
    changed_at: datetime
    reason: Optional[str] = None
    
    # Optional shelf information for readability
    from_shelf_name: Optional[str] = None
    from_shelf_code: Optional[str] = None
    to_shelf_name: Optional[str] = None
    to_shelf_code: Optional[str] = None

    class Config:
        orm_mode = True