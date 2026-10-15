from fastapi import APIRouter
from pydantic import BaseModel
from enum import Enum
from typing import Dict, Optional
import time

router = APIRouter()


class RobotCommandType(str, Enum):
    forward = "forward"
    backward = "backward"
    left = "left"
    right = "right"
    left_turn = "left_turn"
    right_turn = "right_turn"
    stop = "stop"


class RobotCommand(BaseModel):
    command: RobotCommandType


class RobotTelemetry(BaseModel):
    wifi_ssid: str
    signal_strength: int
    uptime_seconds: int


class NetworkProvisionRequest(BaseModel):
    ssid: str
    password: str


ROBOT_REGISTRY: Dict[str, dict] = {}


def get_or_create_robot(robot_id: str) -> dict:
    if robot_id not in ROBOT_REGISTRY:
        ROBOT_REGISTRY[robot_id] = {
            "robot_id": robot_id,
            "command": "stop",
            "sequence": 0,
            "wifi_ssid": "Unknown Network",
            "signal_strength": 0,
            "uptime_seconds": 0,
            "last_seen": 0.0,
            "pending_reconfig": None,      # Stores temporary network dict if requested
            "factory_reset_triggered": False  # Signal to communicate memory format down
        }

    robot = ROBOT_REGISTRY[robot_id]
    if robot["sequence"] == 0:
        robot["status"] = "Needs drive setup"
    elif time.time() - robot["last_seen"] > 15.0:
        robot["status"] = "Offline"
    else:
        robot["status"] = "Online"

    return robot


@router.post("/robot/{robot_id}/command")
async def send_command(robot_id: str, body: RobotCommand):
    robot = get_or_create_robot(robot_id)
    robot["command"] = body.command
    robot["sequence"] += 1
    return {"status": 200, "data": {"command": robot["command"], "sequence": robot["sequence"]}}


@router.post("/robot/{robot_id}/telemetry")
async def update_telemetry(robot_id: str, telemetry: RobotTelemetry):
    robot = get_or_create_robot(robot_id)
    robot["wifi_ssid"] = telemetry.wifi_ssid
    robot["signal_strength"] = telemetry.signal_strength
    robot["uptime_seconds"] = telemetry.uptime_seconds
    robot["last_seen"] = time.time()
    return {"status": 200, "status_state": robot["status"]}


# New network staging target update endpoint
@router.post("/robot/{robot_id}/reconfigure")
async def register_network_provision(robot_id: str, body: NetworkProvisionRequest):
    robot = get_or_create_robot(robot_id)
    robot["pending_reconfig"] = {
        "ssid": body.ssid,
        "password": body.password
    }
    return {"status": 200, "message": "Network target staging initialized successfully."}


@router.get("/robot/{robot_id}/status")
async def get_robot_status(robot_id: str):
    return {"status": 200, "data": get_or_create_robot(robot_id)}


# Enhanced command endpoint to dispatch config instructions to ESP32
@router.get("/robot/{robot_id}/command")
async def get_command(robot_id: str):
    robot = get_or_create_robot(robot_id)
    robot["last_seen"] = time.time()

    response = {
        "status": 200,
        "data": {
            "command": robot["command"],
            "sequence": robot["sequence"],
            "factory_reset": robot["factory_reset_triggered"],
            "new_network": robot["pending_reconfig"]
        }
    }

    # Reset downstream configuration flags once packaged for delivery
    if robot["factory_reset_triggered"]:
        robot["factory_reset_triggered"] = False
    if robot["pending_reconfig"] is not None:
        robot["pending_reconfig"] = None

    return response


# Modified endpoint to signal explicit flash/preferences wipe
@router.post("/robot/{robot_id}/reset")
async def reset_robot(robot_id: str):
    ROBOT_REGISTRY[robot_id] = {
        "robot_id": robot_id,
        "command": "stop",
        "sequence": 0,
        "wifi_ssid": "Disconnected",
        "signal_strength": 0,
        "uptime_seconds": 0,
        "last_seen": 0.0,
        "pending_reconfig": None,
        # Tell ESP32 to clear preferences flash storage area
        "factory_reset_triggered": True
    }
    return {"status": 200, "message": "System wipe instruction sequence locked for dispatch."}