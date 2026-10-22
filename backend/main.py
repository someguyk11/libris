from contextlib import asynccontextmanager, suppress
import asyncio
from fastapi import FastAPI, Request, Depends
from fastapi.exceptions import RequestValidationError
from fastapi.exception_handlers import request_validation_exception_handler
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from database import engine, SessionLocal
from migrate import upgrade
import books
import shelves
import robot
import routes
import checkpoints
import recordings
import auth
from auth_config import get_config, allowed_origins
from auth_migrate import upgrade_auth
from auth_security import AuthSafetyMiddleware
import guidance
from guidance_migrate import upgrade_guidance
from guidance_access import operational_guard, validate_config

@asynccontextmanager
async def lifespan(app):
    get_config()  # Fail startup on unsafe/incomplete enabled auth configuration.
    upgrade(engine)
    upgrade_auth(engine)
    upgrade_guidance(engine)
    validate_config()
    with SessionLocal() as db:
        robot.reset_runtime(db)
        guidance.recover(db)
    worker = asyncio.create_task(guidance.watchdog())
    try:
        yield
    finally:
        worker.cancel()
        with suppress(asyncio.CancelledError):
            await worker

app = FastAPI(title='Libris / LiBot Backend', lifespan=lifespan)
app.add_middleware(AuthSafetyMiddleware, origins=allowed_origins())
app.add_middleware(CORSMiddleware,
    allow_origins=allowed_origins(),
    allow_credentials=True, allow_methods=['GET', 'POST', 'PUT', 'PATCH', 'DELETE'], allow_headers=['Content-Type', 'X-CSRF-Token'])
app.include_router(auth.router)
app.include_router(guidance.router)
for operational_router in (books.router, shelves.router, robot.router, routes.router, checkpoints.router, recordings.router):
    app.include_router(operational_router, dependencies=[Depends(operational_guard)])

@app.exception_handler(RequestValidationError)
async def safe_auth_validation(request: Request, error: RequestValidationError):
    if request.url.path == '/auth' or request.url.path.startswith('/auth/') or '/guidance' in request.url.path:
        # Default Pydantic errors include rejected input (passwords/OTP codes).
        return JSONResponse(status_code=422, content={'detail': [
            {'loc': list(item['loc']), 'type': item['type'], 'msg': 'Invalid authentication input.'}
            for item in error.errors()
        ]})
    return await request_validation_exception_handler(request, error)

@app.get('/ping')
async def ping():
    return {'status': 'alive'}
