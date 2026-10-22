"""One durable guidance task, not a general job queue or permanent command table."""
from datetime import datetime, timezone
from uuid import uuid4
from sqlalchemy import Column, Text, Integer, DateTime, ForeignKey, CheckConstraint, Index, Uuid, text
from sqlalchemy.orm import declarative_base

GuidanceBase = declarative_base()
ACTIVE = ('waiting_for_atlas', 'awaiting_otp', 'queued', 'awaiting_robot', 'running', 'returning_home')
CLAIMED = ACTIVE[1:]

class GuidanceTask(GuidanceBase):
    __tablename__ = 'guidance_tasks'
    __table_args__ = (
        CheckConstraint("status IN ('waiting_for_atlas','awaiting_otp','queued','awaiting_robot','running','returning_home','expired','completed','failed','cancelled')", name='ck_guidance_status'),
        CheckConstraint('progress_index >= 0', name='ck_guidance_progress'),
        CheckConstraint('otp_attempts BETWEEN 0 AND 5', name='ck_guidance_otp_attempts'),
        Index('uq_guidance_active_robot', 'robot_id', unique=True, sqlite_where=text("status IN ('awaiting_otp','queued','awaiting_robot','running','returning_home')"), postgresql_where=text("status IN ('awaiting_otp','queued','awaiting_robot','running','returning_home')")),
        Index('uq_guidance_active_user', 'user_id', unique=True, sqlite_where=text("status IN ('waiting_for_atlas','awaiting_otp','queued','awaiting_robot','running','returning_home')"), postgresql_where=text("status IN ('waiting_for_atlas','awaiting_otp','queued','awaiting_robot','running','returning_home')")),
    )
    id = Column(Text, primary_key=True, default=lambda: str(uuid4()))
    user_id = Column(Text, nullable=False, index=True)
    book_id = Column(Integer, nullable=False)
    shelf_id = Column(Integer, nullable=False)
    robot_id = Column(Text, nullable=False, index=True)
    route_id = Column(Uuid(as_uuid=False), nullable=False)
    # Content fingerprint freezes selection without duplicating recorded routes.
    route_fingerprint = Column(Text, nullable=False)
    return_route_id = Column(Uuid(as_uuid=False), nullable=True)
    return_route_fingerprint = Column(Text, nullable=True)
    atlas_otp_hash = Column(Text, nullable=True)
    otp_expires_at = Column(DateTime(timezone=True), nullable=True)
    otp_attempts = Column(Integer, nullable=False, default=0, server_default='0')
    verified_at = Column(DateTime(timezone=True), nullable=True)
    session_id = Column(Text, nullable=True, unique=True)
    status = Column(Text, nullable=False, default='waiting_for_atlas')
    progress_index = Column(Integer, nullable=False, default=0)
    failure_reason = Column(Text, nullable=True)
    created_at = Column(DateTime(timezone=True), nullable=False, default=lambda: datetime.now(timezone.utc))
    updated_at = Column(DateTime(timezone=True), nullable=False, default=lambda: datetime.now(timezone.utc))

def attach_foreign_keys():
    # Resolve against existing metadata rather than creating duplicate tables.
    from database import BookModel, ShelfModel, RobotModel, RouteModel
    for column, target in [('book_id', BookModel.id), ('shelf_id', ShelfModel.shelf_id), ('robot_id', RobotModel.id), ('route_id', RouteModel.id), ('return_route_id', RouteModel.id)]:
        GuidanceTask.__table__.c[column].append_foreign_key(ForeignKey(target, ondelete='RESTRICT'))

attach_foreign_keys()
