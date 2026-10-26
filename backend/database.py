"""LiBot book/shelf models and API schemas, shared with Libris."""
from pathlib import Path
from datetime import datetime, timezone
from typing import Optional
from uuid import uuid4
from dotenv import load_dotenv
from sqlalchemy import create_engine, Column, Integer, String, Text, Boolean, DateTime, ForeignKey, CheckConstraint, UniqueConstraint, JSON, Uuid, event, text
from sqlalchemy.dialects.postgresql import ARRAY, JSONB
from sqlalchemy.orm import declarative_base, sessionmaker, relationship
from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator
from isbn import isbn13, isbn10, normalize_isbn
from urllib.parse import urlparse
from database_config import database_url
from sqlalchemy.engine import make_url

load_dotenv(Path(__file__).with_name('.env'))
DATABASE_URL = database_url()
IS_SQLITE = make_url(DATABASE_URL).get_backend_name() == 'sqlite'
engine = create_engine(DATABASE_URL, connect_args={'check_same_thread': False} if IS_SQLITE else {})
if IS_SQLITE:
    @event.listens_for(engine, 'connect')
    def enable_foreign_keys(connection, _):
        connection.execute('PRAGMA foreign_keys=ON')
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()

def utc_now():
    # Preserve the existing timezone-naive database columns, storing UTC values.
    return datetime.now(timezone.utc).replace(tzinfo=None)

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

class ShelfModel(Base):
    __tablename__ = 'shelves'
    shelf_id = Column(Integer, primary_key=True, index=True, autoincrement=True)
    shelf_name = Column(String, unique=True, index=True, nullable=False)
    section = Column(String, nullable=True)
    floor = Column(Integer, default=1, nullable=False)
    path_id = Column(Integer, default=0, nullable=False)
    description = Column(String, nullable=True)
    destination_checkpoint_id = Column(Uuid(as_uuid=False), ForeignKey('checkpoints.id', ondelete='SET NULL'), nullable=True)
    books = relationship('BookModel', back_populates='shelf')

class BookModel(Base):
    __tablename__ = 'books'
    id = Column(Integer, primary_key=True, index=True, autoincrement=True)
    isbn = Column(String, unique=True, index=True, nullable=False)
    title = Column(String, nullable=False)
    author = Column(String, nullable=False)
    publish_date = Column(String, nullable=True)
    bibliography = Column(JSON, nullable=True)
    available = Column(Boolean, default=True)
    created_at = Column(DateTime, default=utc_now)
    updated_at = Column(DateTime, default=utc_now, onupdate=utc_now)
    shelf_id = Column(Integer, ForeignKey('shelves.shelf_id'), nullable=False)
    shelf = relationship('ShelfModel', back_populates='books')

class RobotModel(Base):
    __tablename__ = 'robots'
    __table_args__ = (CheckConstraint('last_sequence_number IS NULL OR last_sequence_number >= 0', name='ck_robot_confirmed_sequence'),)
    id = Column(Text, primary_key=True)
    home_checkpoint_id = Column(Uuid(as_uuid=False), ForeignKey('checkpoints.id', ondelete='RESTRICT'), nullable=True)
    guidance_state = Column(Text, nullable=False, default='unknown', server_default='unknown')
    name = Column(Text, nullable=False)
    residing_checkpoint_id = Column(Uuid(as_uuid=False), ForeignKey('checkpoints.id', name='fk_robot_checkpoint', use_alter=True, ondelete='SET NULL'), nullable=True, index=True)
    last_sequence_number = Column(Integer, nullable=True)
    firmware_version = Column(Text, nullable=True)
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), nullable=False)
    updated_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), onupdate=lambda: datetime.now(timezone.utc), nullable=False)
    routes = relationship('RouteModel', back_populates='robot', passive_deletes=True)

class CheckpointModel(Base):
    __tablename__ = 'checkpoints'
    id = Column(Uuid(as_uuid=False), primary_key=True, default=lambda: str(uuid4()))
    name = Column(Text, nullable=False)
    # No location table exists in this Libris schema. Do not invent a duplicate one.
    location_id = Column(Uuid(as_uuid=False), nullable=True)
    connected_checkpoint_ids = Column(JSON().with_variant(ARRAY(Uuid(as_uuid=False)), 'postgresql'), default=list, nullable=False)
    is_active = Column(Boolean, default=True, server_default=text('true'), nullable=False)
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), nullable=False)
    updated_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), onupdate=lambda: datetime.now(timezone.utc), nullable=False)

class RouteModel(Base):
    __tablename__ = 'routes'
    __table_args__ = (
        UniqueConstraint('robot_id', 'name', name='uq_routes_robot_name'),
        CheckConstraint("status IN ('draft','active','disabled')", name='ck_routes_status'),
        CheckConstraint('recording_version = 1', name='ck_routes_recording_version'),
    )
    id = Column(Uuid(as_uuid=False), primary_key=True, default=lambda: str(uuid4()))
    robot_id = Column(Text, ForeignKey('robots.id', ondelete='RESTRICT'), nullable=False, index=True)
    name = Column(Text, nullable=False)
    start_checkpoint_id = Column(Uuid(as_uuid=False), ForeignKey('checkpoints.id', ondelete='RESTRICT'), nullable=False, index=True)
    destination_checkpoint_id = Column(Uuid(as_uuid=False), ForeignKey('checkpoints.id', ondelete='RESTRICT'), nullable=False, index=True)
    status = Column(Text, default='draft', nullable=False)
    recording_version = Column(Integer, default=1, nullable=False)
    route_segments = Column(JSON().with_variant(JSONB(), 'postgresql'), default=list, nullable=False)
    last_verified_at = Column(DateTime(timezone=True), nullable=True)
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), nullable=False)
    updated_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), onupdate=lambda: datetime.now(timezone.utc), nullable=False)
    robot = relationship('RobotModel', back_populates='routes')

class ShelfCreate(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)
    shelf_name: str = Field(min_length=1, max_length=100)
    section: Optional[str] = Field(default=None, max_length=100)
    floor: int = 1
    path_id: int = Field(default=0, ge=0)
    description: Optional[str] = Field(default=None, max_length=500)

class ShelfResponse(ShelfCreate):
    model_config = ConfigDict(from_attributes=True)
    shelf_id: int
    destination_checkpoint_id: Optional[str] = None

class BookBibliography(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True, extra='forbid')
    isbn10: Optional[str] = None
    publisher: Optional[str] = Field(default=None, max_length=500)
    description: Optional[str] = Field(default=None, max_length=20000)
    edition: Optional[str] = Field(default=None, max_length=500)
    cover_url: Optional[str] = Field(default=None, max_length=2000)

    @field_validator('cover_url')
    @classmethod
    def safe_cover(cls, value):
        if not value:
            return None
        parsed = urlparse(value)
        if parsed.scheme != 'https' or not parsed.hostname or parsed.username or parsed.password:
            raise ValueError('Cover image must be a valid HTTPS URL without credentials.')
        return value


class BookDetails(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)
    isbn: str
    title: str = Field(min_length=1, max_length=200)
    author: str = Field(min_length=1, max_length=100)
    publish_date: Optional[str] = Field(default=None, max_length=100)
    bibliography: Optional[BookBibliography] = None

    @field_validator('isbn')
    @classmethod
    def valid_isbn(cls, value):
        return isbn13(value)

    @model_validator(mode='after')
    def original_isbn(self):
        equivalent = isbn10(self.isbn)
        if self.bibliography and self.bibliography.isbn10:
            if len(normalize_isbn(self.bibliography.isbn10)) != 10 or isbn13(self.bibliography.isbn10) != self.isbn:
                raise ValueError('ISBN-10 must identify the same edition as ISBN-13.')
        if equivalent:
            self.bibliography = (self.bibliography or BookBibliography()).model_copy(update={'isbn10': equivalent})
        return self


class BookRegistration(BookDetails):
    shelf_id: int
    require_new: bool = False
    expected_existing_id: Optional[int] = None


class ExistingBookReference(BaseModel):
    model_config = ConfigDict(extra='forbid')
    isbn: str
    existing_id: int = Field(ge=1)

    @field_validator('isbn')
    @classmethod
    def valid_isbn(cls, value):
        return isbn13(value)


class BulkBookRegistration(BaseModel):
    shelf_id: int
    books: list[BookDetails | ExistingBookReference] = Field(min_length=1, max_length=50)

class BookResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    isbn: str
    title: str
    author: str
    publish_date: Optional[str]
    bibliography: Optional[BookBibliography] = None
    available: bool
    shelf_id: int
    shelf: Optional[ShelfResponse] = None
