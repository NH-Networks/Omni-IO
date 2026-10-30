/*
 * Modifications Copyright 2026 CloudAXS.
 * Original upstream portions remain licensed under Apache-2.0.
 */
#include <blind_position.h>
#include <esp_timer.h>
#include <Arduino.h>
#include <algorithm>

namespace IOHC {
    BlindPosition::BlindPosition(uint32_t travelTimeSec)
            : state(State::Idle), openTravelTime(travelTimeSec), closeTravelTime(travelTimeSec),
              lastUpdateUs(0), position(0.0f) {}

    BlindPosition::BlindPosition(uint32_t openTravelTimeSec, uint32_t closeTravelTimeSec)
            : state(State::Idle), openTravelTime(openTravelTimeSec), closeTravelTime(closeTravelTimeSec),
              lastUpdateUs(0), position(0.0f) {}

    void BlindPosition::setTravelTime(uint32_t sec) {
        openTravelTime = sec;
        closeTravelTime = sec;
    }

    void BlindPosition::setTravelTimes(uint32_t openSec, uint32_t closeSec) {
        openTravelTime = openSec;
        closeTravelTime = closeSec;
    }

    void BlindPosition::setOpenTravelTime(uint32_t sec) {
        openTravelTime = sec;
    }

    void BlindPosition::setCloseTravelTime(uint32_t sec) {
        closeTravelTime = sec;
    }

    uint32_t BlindPosition::getTravelTime() const {
        return std::max(openTravelTime, closeTravelTime);
    }

    uint32_t BlindPosition::getOpenTravelTime() const {
        return openTravelTime;
    }

    uint32_t BlindPosition::getCloseTravelTime() const {
        return closeTravelTime;
    }

    void BlindPosition::startOpening() {
        update();
        Serial.printf("[BlindPosition] start opening (pos=%.1f%%, openTime=%us)\n", position, openTravelTime);
        state = State::Opening;
        lastUpdateUs = esp_timer_get_time();
    }

    void BlindPosition::startClosing() {
        update();
        Serial.printf("[BlindPosition] start closing (pos=%.1f%%, closeTime=%us)\n", position, closeTravelTime);
        state = State::Closing;
        lastUpdateUs = esp_timer_get_time();
    }

    void BlindPosition::stop() {
        update();
        Serial.printf("[BlindPosition] stop (pos=%.1f%%)\n", position);
        state = State::Idle;
    }

    void BlindPosition::update() {
        uint32_t currentTravelTime = (state == State::Opening) ? openTravelTime : closeTravelTime;
        if (state == State::Idle || currentTravelTime == 0) {
            lastUpdateUs = esp_timer_get_time();
            return;
        }

        uint64_t now = esp_timer_get_time();
        uint64_t elapsed = now - lastUpdateUs;
        float delta = static_cast<float>(elapsed) * 100.0f /
                      (static_cast<float>(currentTravelTime) * 1000000.0f);

        if (state == State::Opening) {
            position += delta;
            if (position >= 99.5f) {  // margin to avoid floating point rounding issue
                position = 100.0f;
                state = State::Idle;
            }
        } else if (state == State::Closing) {
            position -= delta;
            if (position <= 0.5f) {  // margin for rounding
                position = 0.0f;
                state = State::Idle;
            }
        }

        // Clamp position to [0, 100]
        position = std::clamp(position, 0.0f, 100.0f);

        lastUpdateUs = now;
        Serial.printf("[BlindPosition] update (state=%d pos=%.1f%%)\n", static_cast<int>(state), position);
    }

    float BlindPosition::getPosition() const { return position; }

    bool BlindPosition::isMoving() const { return state != State::Idle; }

    void BlindPosition::setPosition(float pos) {
        position = std::clamp(pos, 0.0f, 100.0f);
        lastUpdateUs = esp_timer_get_time();
    }
}
