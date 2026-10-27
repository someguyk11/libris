#include "TelemetryManager.h"
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_system.h>

bool TelemetryManager::begin() {
    if (begun_) return true;
    commands_ = xQueueCreate(1, sizeof(BackendClient::Command));
    results_ = xQueueCreate(AtlasConfig::ResultQueueCapacity, sizeof(BackendClient::Result));
    acks_ = xQueueCreate(AtlasConfig::ResultQueueCapacity, sizeof(Ack));
    samples_ = xQueueCreate(1, sizeof(BackendClient::SensorSample));
    if (!commands_ || !results_ || !acks_ || !samples_ ||
        xTaskCreatePinnedToCore(taskEntry, "atlas-network", AtlasConfig::NetworkWorkerStackBytes,
            this, AtlasConfig::NetworkWorkerPriority, nullptr, AtlasConfig::NetworkWorkerCore) != pdPASS) {
        if (commands_) vQueueDelete(commands_);
        if (results_) vQueueDelete(results_);
        if (acks_) vQueueDelete(acks_);
        if (samples_) vQueueDelete(samples_);
        commands_ = results_ = acks_ = samples_ = nullptr;
        deliveryFault_.store(true);
        return false;
    }
    begun_ = true;
    return true;
}

void TelemetryManager::taskEntry(void* context) {
    static_cast<TelemetryManager*>(context)->worker(); // never returns.
}

void TelemetryManager::worker() {
    network_.begin();
    BackendClient::Result pending;
    bool hasPending = false, baseline = true;
    uint8_t attempts = 0;
    uint32_t retryAt = 0, pollAt = 0, telemetryAt = 0;
    uint32_t backoff = AtlasConfig::BackendRetryInitialMs;
    uint32_t linkGeneration = network_.generation();
    uint32_t epoch = generation_.fetch_add(1) + 1;
    BackendClient::Command recovery;
    uint32_t recoveryAt = 0, recoveryRetryAt = 0;
    for (;;) {
        uint32_t now = millis();
        network_.update(now);
        if (!network_.connected() || linkGeneration != network_.generation()) {
            if (!baseline || healthy_.load()) epoch = generation_.fetch_add(1) + 1;
            healthy_.store(false);
            baseline = true;
            linkGeneration = network_.generation();
        }
        if (recoveryHold_.load()) {
            // No flash, disconnect or restart before the motor-owning main loop stops navigation.
            if (now - recoveryAt > 10000) recoveryHold_.store(false);
            else if (recoveryApproved_.load() && network_.connected() && static_cast<int32_t>(now - recoveryRetryAt) >= 0) {
                if (backend_.acknowledgeRecovery(recovery)) {
                    using R = BackendClient::Command::Recovery;
                    if (recovery.recovery == R::Restart) esp_restart();
                    if (recovery.recovery == R::Reconnect) network_.reconnect();
                    if (recovery.recovery == R::AutoReconnect) network_.setAutoReconnect(recovery.recoveryEnabled);
                    backend_.recoveryApplied(recovery); // report application only on a subsequent successful poll.
                    recoveryHold_.store(false);
                    baseline = true;
                    epoch = generation_.fetch_add(1) + 1;
                    pollAt = millis() + AtlasConfig::CommandPollIntervalMs;
                } else {
                    const int code = backend_.lastHttpStatus();
                    if (code >= 400 && code < 500 && code != 408 && code != 429) recoveryHold_.store(false);
                    recoveryRetryAt = millis() + 1000;
                }
            }
        } else if (network_.connected() && backend_.configured() && BackendClient::utcNowUs()) {
            // Poll first. Slow TLS/POSTs can cause a local lease stop, never extend a lease.
            if (static_cast<int32_t>(now - pollAt) >= 0) {
                BackendClient::Command c;
                const uint32_t link = network_.generation();
                bool ok = false;
                if (AtlasConfig::UseSupabaseCommands && supabase_.configured()) {
                    // Supabase-backed command source (simulation): poll the
                    // robot_commands table instead of the render.com endpoint.
                    SupabaseClient::CommandRow row;
                    ok = supabase_.poll(row);
                    if (ok) c = rowToCommand(row);
                } else {
                    ok = backend_.poll(c);
                }
                // Manual executable stop/recovery wins over guidance. Status is not stop.
                if (ok && !c.execute && !c.recoveryId[0] && !c.factoryReset && !c.reconfigure) {
                    BackendClient::Command guidance;
                    ok = backend_.pollGuidance(guidance, stoppedSnapshot_.load());
                    if (ok && (guidance.guidance || !c.id[0])) {
                        guidance.autoReconnect = c.autoReconnect;
                        c = guidance;
                    }
                }
                now = millis();
                ok = ok && network_.connected() && network_.generation() == link &&
                    now - c.fetchedAtMs <= AtlasConfig::MaximumCommandResponseMs;
                if (ok) {
                    c.baseline = baseline; c.generation = epoch; c.linkGeneration = link;
                    baseline = false;
                    healthy_.store(true);
                    network_.setAutoReconnect(c.autoReconnect);
                    if (c.recoveryId[0]) {
                        recovery = c;
                        recoveryAt = now; recoveryRetryAt = now;
                        recoveryApproved_.store(false);
                        recoveryHold_.store(true);
                        healthy_.store(false);
                    }
                    xQueueOverwrite(commands_, &c);
                    backoff = AtlasConfig::BackendRetryInitialMs;
                    pollAt = now + (AtlasConfig::UseSupabaseCommands ? AtlasConfig::SupabasePollIntervalMs : AtlasConfig::CommandPollIntervalMs);
                    if (c.factoryReset || c.reconfigure) {
                        healthy_.store(false);
                        epoch = generation_.fetch_add(1) + 1;
                        baseline = true;
                        if (c.factoryReset) network_.factoryReset();
                        else network_.reconfigure(c.ssid, c.password);
                    }
                } else {
                    healthy_.store(false);
                    epoch = generation_.fetch_add(1) + 1;
                    baseline = true;
                    pollAt = now + backoff;
                    backoff = min(backoff * 2, AtlasConfig::BackendRetryMaximumMs);
                }
            }
            if (!hasPending && xQueueReceive(results_, &pending, 0) == pdTRUE) {
                hasPending = true; attempts = 0; retryAt = millis();
            }
            if (hasPending && static_cast<int32_t>(millis() - retryAt) >= 0) {
                bool ready = true;
                if (pending.status == BackendClient::ResultStatus::Paused) {
                    // Publish a fresh main-loop stopped snapshot before pause ACK.
                    BackendClient::Command polled;
                    ready = stoppedSnapshot_.load() && backend_.pollGuidance(polled, true);
                    if (ready) { polled.generation = epoch; polled.linkGeneration = linkGeneration; xQueueOverwrite(commands_, &polled); }
                }
                const bool ok = ready && backend_.sendResult(pending);
                const int code = backend_.lastHttpStatus();
                // Retry identical captured payload only; never retry permanent 4xx responses.
                const bool permanent = code >= 400 && code < 500 && code != 408 && code != 429;
                if (ok || permanent || ++attempts >= AtlasConfig::ResultRetryLimit) {
                    Ack ack{}; strlcpy(ack.id, pending.id, sizeof(ack.id));
                    strlcpy(ack.controlId, pending.controlId, sizeof(ack.controlId));
                    ack.status = pending.status; ack.ok = ok;
                    if (!ok || xQueueSend(acks_, &ack, 0) != pdTRUE) deliveryFault_.store(true);
                    hasPending = false;
                } else retryAt = millis() + AtlasConfig::ResultRetryMs;
            } else if (!hasPending && !activeMovement_.load() && healthy_.load()) {
                // Optional traffic never competes with an executing command's poll/result.
                BackendClient::SensorSample sample;
                if (xQueueReceive(samples_, &sample, 0) == pdTRUE) backend_.sendSensors(sample);
                else if (static_cast<int32_t>(millis() - telemetryAt) >= 0) {
                    backend_.sendTelemetry(WiFi.SSID().c_str(), WiFi.RSSI(), esp_timer_get_time() / 1000000ULL);
                    telemetryAt = millis() + AtlasConfig::TelemetryIntervalMs;
                }
            }
        }
        vTaskDelay(max(static_cast<TickType_t>(1), pdMS_TO_TICKS(AtlasConfig::NetworkWorkerTickMs)));
    }
}

BackendClient::Sensors TelemetryManager::capture(uint32_t nowMs) const {
    BackendClient::Sensors s;
    s.capturedAtUs = BackendClient::utcNowUs();
    for (uint8_t i = 0; i < AtlasConfig::SensorCount; ++i) {
        const auto reading = sensors_.reading(static_cast<AtlasConfig::SensorId>(i), nowMs);
        if (reading.valid && !reading.faulted) { s.cm[i] = reading.distanceCm; s.validMask |= 1U << i; }
    }
    return s;
}

BackendClient::Command TelemetryManager::rowToCommand(const SupabaseClient::CommandRow& row) {
    BackendClient::Command c;
    c.execute = true;
    c.baseline = false;
    c.fetchedAtMs = row.fetchedAtMs;
    strlcpy(c.id, row.id, sizeof(c.id));
    if (!strcmp(row.kind, "forward")) c.kind = BackendClient::CommandKind::Forward;
    else if (!strcmp(row.kind, "backward")) c.kind = BackendClient::CommandKind::Backward;
    else if (!strcmp(row.kind, "left")) c.kind = BackendClient::CommandKind::Left;
    else if (!strcmp(row.kind, "right")) c.kind = BackendClient::CommandKind::Right;
    else if (!strcmp(row.kind, "stop")) c.kind = BackendClient::CommandKind::Stop;
    else c.kind = BackendClient::CommandKind::Stop; // unknown kind -> safe stop
    // Replay rows carry left/right encoder ticks; forward/backward rows carry
    // a distance (cm) which overrides BackendStepCm when set.
    if (!strcmp(row.kind, "replay")) {
        c.leftTicks = row.ticksLeft;
        c.rightTicks = row.ticksRight;
        c.kind = BackendClient::CommandKind::Forward; // replayTicks uses direction from kind
    }
    return c;
}

BackendClient::Result TelemetryManager::result(const BackendClient::Command& c,
    BackendClient::ResultStatus status, uint32_t nowMs) const {
    BackendClient::Result r;
    strlcpy(r.id, c.id, sizeof(r.id)); strlcpy(r.recordingId, c.recordingId, sizeof(r.recordingId));
    r.sequence = c.sequence; r.status = status; r.capturedAtUs = BackendClient::utcNowUs();
    r.sensors = capture(nowMs);
    r.guidance = c.guidance;
    strlcpy(r.taskId, c.taskId, sizeof(r.taskId));
    strlcpy(r.sessionId, c.sessionId, sizeof(r.sessionId));
    strlcpy(r.routeId, c.routeId, sizeof(r.routeId));
    return r;
}

void TelemetryManager::measure(BackendClient::Result& r, BackendClient::CommandKind kind,
    uint32_t leftStart, uint32_t rightStart) const {
    uint32_t left, right; motion_.encoderCounts(left, right);
    r.measured = true; r.leftTicks = left - leftStart; r.rightTicks = right - rightStart;
    using D = BackendClient::Direction; using K = BackendClient::CommandKind;
    r.leftDirection = r.leftTicks ? ((kind == K::Backward || kind == K::Left) ? D::Backward : D::Forward) : D::Stop;
    r.rightDirection = r.rightTicks ? ((kind == K::Backward || kind == K::Right) ? D::Backward : D::Forward) : D::Stop;
}

bool TelemetryManager::enqueue(const BackendClient::Result& r) {
    if (r.capturedAtUs && xQueueSend(results_, &r, 0) == pdTRUE) return true;
    deliveryFault_.store(true); // bounded backpressure is a stop condition, not silent loss.
    return false;
}

bool TelemetryManager::seen(const char* id) const {
    for (const auto& old : recent_) if (!strcmp(old, id)) return true;
    return false;
}
void TelemetryManager::remember(const char* id) {
    strlcpy(recent_[recentIndex_], id, sizeof(recent_[0]));
    recentIndex_ = (recentIndex_ + 1) % AtlasConfig::RecentCommandCapacity;
}

bool TelemetryManager::fresh(uint32_t nowMs) const {
    return backendReady() && network_.connected() && !network_.configurationPending() && active_.generation == generation_.load() &&
        active_.linkGeneration == network_.generation() && nowMs - lastPollMs_ <= AtlasConfig::CommandLeaseMs;
}

void TelemetryManager::finish(bool completed, const char* error, uint32_t nowMs) {
    const bool ran = phase_ == Phase::Running || phase_ == Phase::Paused ||
        phase_ == Phase::AwaitingPauseAck || phase_ == Phase::AwaitingResumeAck || phase_ == Phase::Resuming;
    if (!completed && ran) navigation_.cancel(); // stops; never resets a safety latch.
    auto r = result(active_, completed ? BackendClient::ResultStatus::Completed : BackendClient::ResultStatus::Failed, nowMs);
    if (ran) measure(r, active_.kind, leftStart_, rightStart_);
    if (error) strlcpy(r.error, error, sizeof(r.error));
    enqueue(r);
    // Mark Supabase command row status (simulation backend).
    if (AtlasConfig::UseSupabaseCommands && supabase_.configured() && active_.id[0]) {
        supabase_.markStatus(active_.id, completed ? "executed" : "failed");
    }
    phase_ = Phase::Idle;
    controlId_[0] = 0;
    activeMovement_.store(false);
}

void TelemetryManager::command(const BackendClient::Command& c, uint32_t nowMs) {
    using K = BackendClient::CommandKind; using S = BackendClient::ResultStatus;
    if (c.recoveryId[0]) {
        if (phase_ != Phase::Idle) finish(false, "Recovery requested; no automatic resume", nowMs);
        navigation_.cancel();
        recoveryApproved_.store(true);
        return;
    }
    if (c.factoryReset || c.reconfigure) {
        if (phase_ != Phase::Idle) finish(false, "Network configuration changed", nowMs);
        navigation_.cancel();
        return;
    }
    if (phase_ != Phase::Idle && c.guidance != active_.guidance && !c.execute) return;
    // The overwrite mailbox may replace the worker's first baseline response.
    // Independently reject the first command observed in every connection epoch.
    const bool baseline = c.baseline || observedGeneration_ != c.generation;
    if (baseline) {
        observedGeneration_ = c.generation;
        highSequence_ = c.transportSequence;
    }
    if (phase_ != Phase::Idle && !strcmp(c.id, active_.id) && c.sequence == active_.sequence &&
        c.kind == active_.kind && c.generation == active_.generation && c.linkGeneration == active_.linkGeneration) {
        if (c.guidance && (strcmp(c.taskId, active_.taskId) || strcmp(c.sessionId, active_.sessionId) ||
            strcmp(c.routeId, active_.routeId) || c.leftTicks != active_.leftTicks || c.rightTicks != active_.rightTicks)) {
            finish(false, "Guidance identity/target changed", nowMs); return;
        }
        lastPollMs_ = c.fetchedAtMs;
        using C = BackendClient::Command::Control;
        if (c.guidance && c.control != C::None && strcmp(c.controlId, controlId_)) {
            if (c.control == C::Pause && phase_ == Phase::Running) {
                if (!navigation_.pause()) { finish(false, "Pause rejected", nowMs); return; }
                strlcpy(controlId_, c.controlId, sizeof(controlId_));
                auto r = result(active_, S::Paused, nowMs); strlcpy(r.controlId, controlId_, sizeof(r.controlId));
                phase_ = Phase::AwaitingPauseAck; enqueue(r);
            } else if (c.control == C::Resume && phase_ == Phase::Paused && safety_.canMove()) {
                strlcpy(controlId_, c.controlId, sizeof(controlId_));
                auto r = result(active_, S::Resumed, nowMs); strlcpy(r.controlId, controlId_, sizeof(r.controlId));
                phase_ = Phase::AwaitingResumeAck; enqueue(r);
            } else { finish(false, "Invalid pause/resume transition", nowMs); return; }
        } else if (c.guidance && c.serverPaused && phase_ == Phase::Running) {
            finish(false, "Backend/firmware pause contradiction", nowMs);
        }
        return; // duplicate delivery/started poll refreshes lease, never repeats execution.
    }
    if (phase_ != Phase::Idle) finish(false, "Command stopped or superseded", nowMs);
    if (!c.id[0] || !c.execute || seen(c.id)) {
        // Idle/status polls and handled commands do not own local recording/replay.
        // Any active backend movement was stopped above; executable Stop is below.
        return;
    }
    remember(c.id);
    const uint32_t previousSequence = highSequence_;
    highSequence_ = max(highSequence_, c.transportSequence);
    if (c.kind == K::Stop && !c.guidance) {
        uint32_t left, right; motion_.encoderCounts(left, right);
        const auto kind = motion_.kind();
        navigation_.cancel();
        auto r = result(c, S::Completed, nowMs);
        K prior = kind == MotionController::Kind::Backward ? K::Backward :
            kind == MotionController::Kind::Counterclockwise90 ? K::Left :
            kind == MotionController::Kind::Clockwise90 ? K::Right : K::Forward;
        measure(r, prior, left, right);
        enqueue(r);
        return;
    }
    const char* error = nullptr;
    if (baseline || (!c.guidance && c.transportSequence <= previousSequence)) error = "Stale/baseline command; submit a new command";
    else if (!backendReady() || !network_.connected() || network_.configurationPending() || c.generation != generation_.load() || c.linkGeneration != network_.generation() ||
        nowMs - c.fetchedAtMs > AtlasConfig::CommandLeaseMs) error = "Command connection/lease expired";
    else if (navigation_.movementPending() || !safety_.canMove() ||
        (navigation_.state() != Navigation::State::Idle && navigation_.state() != Navigation::State::Completed))
        error = "Navigation busy or safety interlock active";
    else if (!c.guidance && (c.kind == K::Forward || c.kind == K::Backward) &&
        (!AtlasConfig::positive(AtlasConfig::BackendStepCm) || AtlasConfig::BackendStepCm <= AtlasConfig::DistanceToleranceCm ||
         AtlasConfig::BackendStepCm > AtlasConfig::MaximumMoveCm)) error = "BackendStepCm is unconfigured or invalid";
    if (error) { auto r = result(c, S::Failed, nowMs); strlcpy(r.error, error, sizeof(r.error)); enqueue(r); return; }
    active_ = c; lastPollMs_ = c.fetchedAtMs; phase_ = Phase::AwaitingStart;
    activeMovement_.store(true);
    if (!enqueue(result(c, S::Started, nowMs))) finish(false, "Result queue unavailable", nowMs);
}

void TelemetryManager::update(uint32_t nowMs) {
    if (!begun_) return;
    stoppedSnapshot_.store(!motion_.moving());
    if (network_.configurationPending()) {
        if (phase_ != Phase::Idle) finish(false, "Wi-Fi profile change requires stationary robot", nowMs);
        navigation_.cancel();
        network_.approveConfiguration();
        return; // no dispatch until the worker finishes its flash write.
    }
    if (phase_ != Phase::Idle && !fresh(nowMs)) finish(false, "Connection lost, stale poll or result delivery fault", nowMs);
    BackendClient::Command c;
    if (xQueueReceive(commands_, &c, 0) == pdTRUE) command(c, nowMs);
    if (recoveryHold_.load()) return; // keep stopped until worker acknowledges/applies or abandons recovery.
    Ack ack{};
    // Bounded drain: at most one queue's capacity, no blocking waits.
    for (uint8_t i = 0; i < AtlasConfig::ResultQueueCapacity && xQueueReceive(acks_, &ack, 0) == pdTRUE; ++i) {
        if (strcmp(ack.id, active_.id)) continue;
        if ((phase_ == Phase::AwaitingPauseAck && ack.status == BackendClient::ResultStatus::Paused) ||
            (phase_ == Phase::AwaitingResumeAck && ack.status == BackendClient::ResultStatus::Resumed)) {
            if (strcmp(ack.controlId, controlId_)) continue;
            if (!ack.ok || !fresh(nowMs) || !safety_.canMove()) {
                finish(false, "Control acknowledgement failed or safety interlock", nowMs); continue;
            }
            if (phase_ == Phase::AwaitingPauseAck) phase_ = Phase::Paused;
            else if (navigation_.resume()) phase_ = Phase::Resuming;
            else finish(false, "Resume rejected; remains stopped", nowMs);
            continue;
        }
        if (phase_ != Phase::AwaitingStart || strcmp(ack.id, active_.id) || ack.status != BackendClient::ResultStatus::Started) continue;
        if (!ack.ok || !fresh(nowMs)) { finish(false, "Start acknowledgement failed or expired", nowMs); continue; }
        motion_.encoderCounts(leftStart_, rightStart_);
        using K = BackendClient::CommandKind;
        if (active_.guidance && active_.kind == K::Stop) {
            navigation_.cancel();
            phase_ = Phase::Running;
            finish(true, "", nowMs);
            continue;
        }
        using M = MotionController::Kind;
        const M kind = active_.kind == K::Forward ? M::Forward : active_.kind == K::Backward ? M::Backward :
            active_.kind == K::Left ? M::Counterclockwise90 : M::Clockwise90;
        const bool ok = active_.guidance ? navigation_.replayTicks(active_.leftTicks, active_.rightTicks, kind) :
            active_.kind == K::Forward ? navigation_.driveDistance(AtlasConfig::BackendStepCm) :
            active_.kind == K::Backward ? navigation_.driveDistance(-AtlasConfig::BackendStepCm) :
            navigation_.turn90(active_.kind == K::Right);
        if (ok) phase_ = Phase::Running;
        else finish(false, "Navigation rejected movement (state/calibration/safety)", nowMs);
    }
    if (phase_ != Phase::Idle && phase_ != Phase::AwaitingStart &&
        (!safety_.canMove() || navigation_.state() == Navigation::State::Fault))
        finish(false, "Safety interruption; no automatic resume", nowMs);
    if (phase_ == Phase::Resuming && navigation_.state() != Navigation::State::Paused) phase_ = Phase::Running;
    if (phase_ == Phase::Running) {
        if (!safety_.canMove() || navigation_.state() == Navigation::State::Fault || navigation_.state() == Navigation::State::Paused)
            finish(false, "Safety/navigation interruption; no automatic resume", nowMs);
        else if (!navigation_.movementPending())
            finish(navigation_.state() == Navigation::State::Completed, "", nowMs);
        else if (active_.recordingId[0] && nowMs - lastSampleMs_ >= AtlasConfig::RecordingSensorIntervalMs) {
            BackendClient::SensorSample sample;
            strlcpy(sample.id, active_.id, sizeof(sample.id)); strlcpy(sample.recordingId, active_.recordingId, sizeof(sample.recordingId));
            sample.sequence = active_.sequence; sample.sensors = capture(nowMs);
            if (sample.sensors.validMask && sample.sensors.capturedAtUs) xQueueOverwrite(samples_, &sample);
            lastSampleMs_ = nowMs;
        }
    }
}
