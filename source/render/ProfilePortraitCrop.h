/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROFILEPORTRAITCROP_H
#define PROFILEPORTRAITCROP_H

#include <CEGUI/Rect.h>
#include <algorithm>

//! Top square of a profile portrait, without changing its texture or orientation.
inline CEGUI::Rectf getProfilePortraitArea(float width, float height)
{
    const float side = std::min(width, height);
    return CEGUI::Rectf(0.0f, 0.0f, side, side);
}

#endif
