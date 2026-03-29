#ifndef _MOD_ARCHIPELAWOW_PLAYER_POSITION_H_
#define _MOD_ARCHIPELAWOW_PLAYER_POSITION_H_

#include "Define.h"
#include "GameTime.h"

#include <chrono>

namespace ModArchipelaWoW
{
    struct PlayerPosition
    {
        PlayerPosition()
            : PlayerPosition(0, 0.0f, 0.0f, 0.0f, 0.0f)
        {
        }

        PlayerPosition(uint32 mapId, float x, float y, float z, float orientation) :
            mapId(mapId),
            x(x),
            y(y),
            z(z),
            orientation(orientation),
            time(GameTime::GetGameTime())
        {
        }

        uint32 mapId;
        float x;
        float y;
        float z;
        float orientation;
        std::chrono::seconds time;
    };
}

#endif
