#include "TelemetryManager.h"
#include <freertos/task.h>
#include <esp_timer.h>

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
    for (;;) {
        uint32_t now = millis();
        network_.update(now);
        if (!network_.connected() || linkGeneration != network_.generation()) {
            if (!baseline || healthy_.load()) epoch = generation_.fetch_add(1) + 1;
            healthy_.store(false);
            baseline = true;
            linkGeneration = network_.generation();
        }
        if (network_.connected() && backend_.configured() && BackendClient::utcNowUs()) {
            // Poll first. Slow TLS/POSTs can cause a local lease stop, never extend a lease.
            if (static_cast<int32_t>(now - pollAt) >= 0) {
                BackendClient::Command c;
                const uint32_t link = network_.generation();
                bool ok = backend_.poll(c);
                now = millis();
                ok = ok && network_.connected() && network_.generation() == link &&
                    now - c.fetchedAtMs <= AtlasConfig::MaximumCommandResponseMs;
                if (ok) {
                    c.baseline = baseline; c.generation = epoch; c.linkGeneration = link;
                    baseline = false;
                    healthy_.store(true);
                    xQueueOverwrite(commands_, &c);
                    backoff = AtlasConfig::BackendRetryInitialMs;
                    pollAt = now + AtlasConfig::CommandPollIntervalMs;
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
                const bool ok = backend_.sendResult(pending);
                const int code = backend_.lastHttpStatus();
                // Retry identical captured payload only; never retry permanent 4xx responses.
                const bool permanent = code >= 400 && code < 500 && code != 408 && code != 429;
                if (ok || permanent || ++attempts >= AtlasConfig::ResultRetryLimit) {
                    Ack ack{}; strlcpy(ack.id, pending.id, sizeof(ack.id));
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

BackendClient::Result TelemetryManager::result(const BackendClient::Command& c,
    BackendClient::ResultStatus status, uint32_t nowMs) const {
    BackendClient::Result r;
    strlcpy(r.id, c.id, sizeof(r.id)); strlcpy(r.recordingId, c.recordingId, sizeof(r.recordingId));
    r.sequence = c.sequence; r.status = status; r.capturedAtUs = BackendClient::utcNowUs();
    r.sensors = capture(nowMs);
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
    const bool ran = phase_ == Phase::Running;
    if (!completed && ran) navigation_.cancel(); // stops; never resets a safety latch.
    auto r = result(active_, completed ? BackendClient::ResultStatus::Completed : BackendClient::ResultStatus::Failed, nowMs);
    if (ran) measure(r, active_.kind, leftStart_, rightStart_);
    if (error) strlcpy(r.error, error, sizeof(r.error));
    enqueue(r);
    phase_ = Phase::Idle;
    activeMovement_.store(false);
}

void TelemetryManager::command(const BackendClient::Command& c, uint32_t nowMs) {
    using K = BackendClient::CommandKind; using S = BackendClient::ResultStatus;
    if (c.factoryReset || c.reconfigure) {
        if (phase_ != Phase::Idle) finish(false, "Network configuration changed", nowMs);
        navigation_.cancel();
        return;
    }
    // The overwrite mailbox may replace the worker's first baseline response.
    // Independently reject the first command observed in every connection epoch.
    const bool baseline = c.baseline || observedGeneration_ != c.generation;
    if (baseline) {
        observedGeneration_ = c.generation;
        highSequence_ = c.transportSequence;
    }
    if (phase_ != Phase::Idle && !strcmp(c.id, active_.id) && c.sequence == active_.sequence &&
        c.kind == active_.kind && c.generation == active_.generation && c.linkGeneration == active_.linkGeneration) {
        lastPollMs_ = c.fetchedAtMs;
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
    if (c.kind == K::Stop) {
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
    if (baseline || c.transportSequence <= previousSequence) error = "Stale/baseline command; submit a new command";
    else if (!backendReady() || !network_.connected() || network_.configurationPending() || c.generation != generation_.load() || c.linkGeneration != network_.generation() ||
        nowMs - c.fetchedAtMs > AtlasConfig::CommandLeaseMs) error = "Command connection/lease expired";
    else if (navigation_.movementPending() || !safety_.canMove() ||
        (navigation_.state() != Navigation::State::Idle && navigation_.state() != Navigation::State::Completed))
        error = "Navigation busy or safety interlock active";
    else if ((c.kind == K::Forward || c.kind == K::Backward) &&
        (!AtlasConfig::positive(AtlasConfig::BackendStepCm) || AtlasConfig::BackendStepCm <= AtlasConfig::DistanceToleranceCm ||
         AtlasConfig::BackendStepCm > AtlasConfig::MaximumMoveCm)) error = "BackendStepCm is unconfigured or invalid";
    if (error) { auto r = result(c, S::Failed, nowMs); strlcpy(r.error, error, sizeof(r.error)); enqueue(r); return; }
    active_ = c; lastPollMs_ = c.fetchedAtMs; phase_ = Phase::AwaitingStart;
    activeMovement_.store(true);
    if (!enqueue(result(c, S::Started, nowMs))) finish(false, "Result queue unavailable", nowMs);
}

void TelemetryManager::update(uint32_t nowMs) {
    if (!begun_) return;
    if (network_.configurationPending()) {
        if (phase_ != Phase::Idle) finish(false, "Wi-Fi profile change requires stationary robot", nowMs);
        navigation_.cancel();
        network_.approveConfiguration();
        return; // no dispatch until the worker finishes its flash write.
    }
    if (phase_ != Phase::Idle && !fresh(nowMs)) finish(false, "Connection lost, stale poll or result delivery fault", nowMs);
    BackendClient::Command c;
    if (xQueueReceive(commands_, &c, 0) == pdTRUE) command(c, nowMs);
    Ack ack{};
    // Bounded drain: at most one queue's capacity, no blocking waits.
    for (uint8_t i = 0; i < AtlasConfig::ResultQueueCapacity && xQueueReceive(acks_, &ack, 0) == pdTRUE; ++i) {
        if (phase_ != Phase::AwaitingStart || strcmp(ack.id, active_.id) || ack.status != BackendClient::ResultStatus::Started) continue;
        if (!ack.ok || !fresh(nowMs)) { finish(false, "Start acknowledgement failed or expired", nowMs); continue; }
        motion_.encoderCounts(leftStart_, rightStart_);
        using K = BackendClient::CommandKind;
        const bool ok = active_.kind == K::Forward ? navigation_.driveDistance(AtlasConfig::BackendStepCm) :
            active_.kind == K::Backward ? navigation_.driveDistance(-AtlasConfig::BackendStepCm) :
            navigation_.turn90(active_.kind == K::Right);
        if (ok) phase_ = Phase::Running;
        else finish(false, "Navigation rejected movement (state/calibration/safety)", nowMs);
    }
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
