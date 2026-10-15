import os
from datetime import datetime
from typing import Optional
from dotenv import load_dotenv
from sqlalchemy import create_engine, Column, Integer, String, Boolean, DateTime, Text, REAL, ForeignKey, UniqueConstraint, text, Date, BigInteger, Index
from sqlalchemy.sql import func
from sqlalchemy.dialects.postgresql import UUID as PG_UUID, JSONB
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship, sessionmaker
from pydantic import BaseModel, ConfigDict

load_dotenv()
DATABASE_URL = os.getenv("DATABASE_URL")

engine = create_engine(DATABASE_URL)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

# ============= Core Models =============
class Book(Base):
    __tablename__ = "books"

    book_id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    isbn = Column(String(20), nullable=False, unique=True)
    title = Column(Text, nullable=False)
    author = Column(Text, nullable=False)
    publisher = Column(String(200), nullable=True)
    edition = Column(String(50), nullable=True)
    language = Column(String(50), nullable=True, server_default=text("'English'"))
    category = Column(String(100), nullable=True)
    description = Column(Text, nullable=True)
    cover_image_url = Column(Text, nullable=True)
    created_at = Column(DateTime(timezone=True), nullable=True, server_default=text('CURRENT_TIMESTAMP'))
    shelf_id = Column(PG_UUID(as_uuid=True), ForeignKey('shelves.id'), nullable=True)
    # relationships
    copies = relationship("BookCopy", back_populates="book")
    shelf = relationship("Shelf", back_populates="books")
    location_history = relationship("BookLocationHistory", back_populates="book", order_by="desc(BookLocationHistory.changed_at)")

class BookCopy(Base):
    __tablename__ = "book_copies"

    copy_id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    book_id = Column(PG_UUID(as_uuid=True), ForeignKey('books.book_id'), nullable=False)
    shelf_id = Column(PG_UUID(as_uuid=True), ForeignKey('physical_shelf.shelf_id'), nullable=True)
    barcode = Column(String(100), nullable=False, unique=True)
    status = Column(String(50), nullable=True, server_default=text("'Available'"))
    condition = Column(String(50), nullable=True, server_default=text("'Good'"))
    acquired_date = Column(Date, nullable=True, server_default=text('CURRENT_DATE'))
    # relationships
    book = relationship("Book", back_populates="copies")
    shelf = relationship("PhysicalShelf", back_populates="copies")
    reservations = relationship("Reservation", back_populates="copy")
    borrows = relationship("BorrowRecord", back_populates="copy")

class PhysicalShelf(Base):
    __tablename__ = "physical_shelf"

    shelf_id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    zone = Column(String(50), nullable=False)
    aisle = Column(String(50), nullable=False)
    row = Column(Integer, nullable=False)
    column = Column(Integer, nullable=False)
    navigation_node_id = Column(PG_UUID(as_uuid=True), ForeignKey('navigation_nodes.node_id'), nullable=True)
    description = Column(Text, nullable=True)
    # relationships
    navigation_node = relationship("NavigationNode", back_populates="shelves")
    copies = relationship("BookCopy", back_populates="shelf")

class NavigationNode(Base):
    __tablename__ = "navigation_nodes"

    node_id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    node_name = Column(String(100), nullable=False, unique=True)
    x_coordinate = Column(REAL, nullable=False)
    y_coordinate = Column(REAL, nullable=False)
    node_type = Column(String(50), nullable=True, server_default=text("'waypoint'"))
    confidence = Column(REAL, nullable=True, server_default=text('1.0'))
    created_at = Column(DateTime(timezone=True), nullable=True, server_default=text('CURRENT_TIMESTAMP'))
    # relationships
    shelves = relationship("PhysicalShelf", back_populates="navigation_node")
    start_edges = relationship("NavigationEdge", foreign_keys="NavigationEdge.start_node", back_populates="start_node_rel")
    end_edges = relationship("NavigationEdge", foreign_keys="NavigationEdge.end_node", back_populates="end_node_rel")

class NavigationEdge(Base):
    __tablename__ = "navigation_edges"

    edge_id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    start_node = Column(PG_UUID(as_uuid=True), ForeignKey('navigation_nodes.node_id'), nullable=True)
    end_node = Column(PG_UUID(as_uuid=True), ForeignKey('navigation_nodes.node_id'), nullable=True)
    distance = Column(REAL, nullable=False)
    preferred_direction = Column(String(20), nullable=True, server_default=text("'bidirectional'"))
    max_speed = Column(REAL, nullable=True, server_default=text('1.0'))
    # relationships
    start_node_rel = relationship("NavigationNode", foreign_keys=[start_node], back_populates="start_edges")
    end_node_rel = relationship("NavigationNode", foreign_keys=[end_node], back_populates="end_edges")
    __table_args__ = (UniqueConstraint('start_node', 'end_node', name='unique_edge_pair'),)

class RobotJob(Base):
    __tablename__ = "robot_jobs"

    job_id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    destination_node_id = Column(PG_UUID(as_uuid=True), ForeignKey('navigation_nodes.node_id'), nullable=False)
    robot_id = Column(String(50), nullable=True, server_default=text("'libot_v1'"))
    priority = Column(Integer, nullable=True, server_default=text('1'))
    status = Column(String(50), nullable=True, server_default=text("'Waiting'"))
    created_time = Column(DateTime(timezone=True), nullable=True, server_default=text('CURRENT_TIMESTAMP'))
    updated_time = Column(DateTime(timezone=True), nullable=True, server_default=text('CURRENT_TIMESTAMP'))
    # relationships
    destination_node = relationship("NavigationNode")

class Reservation(Base):
    __tablename__ = "reservations"

    reservation_id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    copy_id = Column(PG_UUID(as_uuid=True), ForeignKey('book_copies.copy_id'), nullable=False)
    user_id = Column(PG_UUID(as_uuid=True), nullable=False)  # assuming UUID
    created_time = Column(DateTime(timezone=True), nullable=True, server_default=text('CURRENT_TIMESTAMP'))
    expiry_time = Column(DateTime(timezone=True), nullable=False)
    status = Column(String(50), nullable=True, server_default=text("'Pending'"))
    # relationships
    copy = relationship("BookCopy", back_populates="reservations")

class BorrowRecord(Base):
    __tablename__ = "borrow_records"

    borrow_id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    copy_id = Column(PG_UUID(as_uuid=True), ForeignKey('book_copies.copy_id'), nullable=False)
    user_id = Column(PG_UUID(as_uuid=True), nullable=False)
    borrow_date = Column(DateTime(timezone=True), nullable=True, server_default=text('CURRENT_TIMESTAMP'))
    due_date = Column(DateTime(timezone=True), nullable=False)
    returned_date = Column(DateTime(timezone=True), nullable=True)
    status = Column(String(50), nullable=True, server_default=text("'Active'"))
    # relationships
    copy = relationship("BookCopy", back_populates="borrows")

class SystemLog(Base):
    __tablename__ = "system_logs"

    log_id = Column(BigInteger, primary_key=True, server_default=text("nextval('system_logs_log_id_seq'::regclass)"))
    event_type = Column(String(100), nullable=False)
    message = Column(Text, nullable=False)
    meta_data = Column(JSONB, nullable=True)
    created_at = Column(DateTime(timezone=True), nullable=True, server_default=text('CURRENT_TIMESTAMP'))

class Library(Base):
    __tablename__ = "libraries"

    id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    name = Column(String(200), nullable=False)
    description = Column(Text, nullable=True)
    from sqlalchemy.sql import func
    created_at = Column(DateTime(timezone=True), nullable=False, default=func.now())
    updated_at = Column(DateTime(timezone=True), nullable=False, default=func.now(), onupdate=func.now())

    # Relationships
    sections = relationship("Section", back_populates="library")


class Section(Base):
    __tablename__ = "sections"

    id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    library_id = Column(PG_UUID(as_uuid=True), ForeignKey('libraries.id'), nullable=False)
    name = Column(String(200), nullable=False)
    code = Column(String(50), nullable=False)
    description = Column(Text, nullable=True)
    from sqlalchemy.sql import func
    created_at = Column(DateTime(timezone=True), nullable=False, default=func.now())
    updated_at = Column(DateTime(timezone=True), nullable=False, default=func.now(), onupdate=func.now())

    # Relationships
    library = relationship("Library", back_populates="sections")
    shelves = relationship("Shelf", back_populates="section")

    # Constraints
    __table_args__ = (UniqueConstraint('library_id', 'code', name='uq_section_code_per_library'),)


class Shelf(Base):
    __tablename__ = "shelves"

    id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    section_id = Column(PG_UUID(as_uuid=True), ForeignKey('sections.id'), nullable=False)
    name = Column(String(200), nullable=False)
    code = Column(String(50), nullable=False)
    description = Column(Text, nullable=True)
    from sqlalchemy.sql import func
    created_at = Column(DateTime(timezone=True), nullable=False, default=func.now())
    updated_at = Column(DateTime(timezone=True), nullable=False, default=func.now(), onupdate=func.now())

    # Relationships
    section = relationship("Section", back_populates="shelves")
    books = relationship("Book", back_populates="shelf")
    from_location_history = relationship("BookLocationHistory", foreign_keys="[BookLocationHistory.from_shelf_id]", back_populates="from_shelf")
    to_location_history = relationship("BookLocationHistory", foreign_keys="[BookLocationHistory.to_shelf_id]", back_populates="to_shelf")

    # Constraints
    __table_args__ = (UniqueConstraint('section_id', 'code', name='uq_shelf_code_per_section'),)


# Pydantic schemas have been moved to schemas_book.py
# Alias for backward compatibility with existing imports
BookModel = Book


class BookLocationHistory(Base):
    __tablename__ = "book_location_history"

    id = Column(PG_UUID(as_uuid=True), primary_key=True, server_default=text('uuid_generate_v4()'))
    book_id = Column(PG_UUID(as_uuid=True), ForeignKey('books.book_id'), nullable=False)
    from_shelf_id = Column(PG_UUID(as_uuid=True), ForeignKey('shelves.id'), nullable=True)
    to_shelf_id = Column(PG_UUID(as_uuid=True), ForeignKey('shelves.id'), nullable=False)
    changed_at = Column(DateTime(timezone=True), nullable=False, server_default=text('CURRENT_TIMESTAMP'))
    reason = Column(String(200), nullable=True)

    # Relationships
    book = relationship("Book", back_populates="location_history")
    from_shelf = relationship("Shelf", foreign_keys=[from_shelf_id], back_populates="from_location_history")
    to_shelf = relationship("Shelf", foreign_keys=[to_shelf_id], back_populates="to_location_history")

    # Indexes for better query performance
    __table_args__ = (
        Index('ix_book_location_history_book_id', 'book_id'),
        Index('ix_book_location_history_from_shelf_id', 'from_shelf_id'),
        Index('ix_book_location_history_to_shelf_id', 'to_shelf_id'),
        Index('ix_book_location_history_changed_at', 'changed_at'),
    )