/*
 * Modifications Copyright 2026 CloudAXS.
 * Original upstream portions remain licensed under Apache-2.0.
 */
#ifndef IOHC_BLIND_POSITION_H
#define IOHC_BLIND_POSITION_H

#include <stdint.h>
#include <algorithm>

namespace IOHC {
    class BlindPosition {
    public:
        explicit BlindPosition(uint32_t travelTimeSec = 0);
        explicit BlindPosition(uint32_t openTravelTimeSec, uint32_t closeTravelTimeSec);

        void setTravelTime(uint32_t sec);
        void setTravelTimes(uint32_t openSec, uint32_t closeSec);
        void setOpenTravelTime(uint32_t sec);
        void setCloseTravelTime(uint32_t sec);

        uint32_t getTravelTime() const;
        uint32_t getOpenTravelTime() const;
        uint32_t getCloseTravelTime() const;

        void startOpening();
        void startClosing();
        void stop();
        void update();

        float getPosition() const;
        bool isMoving() const;
        void setPosition(float pos);

    private:
        enum class State { Idle, Opening, Closing };
        State state;
        uint32_t openTravelTime;  // seconds to fully open (0 -> 100%)
        uint32_t closeTravelTime; // seconds to fully close (100 -> 0%)
        uint64_t lastUpdateUs;
        float position; // 0..100
    };
}

#endif // IOHC_BLIND_POSITION_H
