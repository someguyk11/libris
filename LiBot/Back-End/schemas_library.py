from pydantic import BaseModel
from typing import Optional
from datetime import datetime
import uuid

class LibraryBase(BaseModel):
    name: str
    description: Optional[str] = None

class LibraryCreate(LibraryBase):
    pass

class LibraryResponse(LibraryBase):
    id: uuid.UUID
    created_at: datetime
    updated_at: datetime

    class Config:
        orm_mode = True

class SectionBase(BaseModel):
    library_id: uuid.UUID
    name: str
    code: str
    description: Optional[str] = None

class SectionCreate(SectionBase):
    pass

class SectionResponse(SectionBase):
    id: uuid.UUID
    created_at: datetime
    updated_at: datetime

    class Config:
        orm_mode = True

class ShelfBase(BaseModel):
    section_id: uuid.UUID
    name: str
    code: str
    description: Optional[str] = None

class ShelfCreate(ShelfBase):
    pass

class ShelfResponse(ShelfBase):
    id: uuid.UUID
    created_at: datetime
    updated_at: datetime

    class Config:
        orm_mode = True