/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/AppearanceCompose.h"

#include "game/CreatureAppearance.h"
#include "render/PortraitTint.h"

#include <algorithm>

namespace AppearanceCompose
{
const char* const SLOT_HELMET = "helmet";
const char* const SLOT_SCAR = "scar";
const uint32_t CACHE_KEEP_NEWEST = 4;

namespace
{
//! Dark background of the portraits, the same colour the model portraits are rendered on
const float BACKGROUND_RED = 0.025f;
const float BACKGROUND_GREEN = 0.018f;
const float BACKGROUND_BLUE = 0.015f;

bool startsWith(const std::string& text, const std::string& prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}

//! Normal "over" blending of a non premultiplied source pixel with the given alpha on the destination
void blendPixel(uint8_t* dst, const uint8_t* src, uint32_t srcAlpha)
{
    if(srcAlpha == 0)
        return;

    uint32_t dstAlpha = dst[3];
    uint32_t outAlpha = (srcAlpha * 255 + dstAlpha * (255 - srcAlpha) + 127) / 255;
    if(outAlpha == 0)
    {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = 0;
        return;
    }

    uint32_t denominator = outAlpha * 255;
    for(uint32_t c = 0; c < 3; ++c)
    {
        uint32_t numerator = static_cast<uint32_t>(src[c]) * srcAlpha * 255 +
            static_cast<uint32_t>(dst[c]) * dstAlpha * (255 - srcAlpha);
        dst[c] = static_cast<uint8_t>((numerator + denominator / 2) / denominator);
    }
    dst[3] = static_cast<uint8_t>(outAlpha);
}

//! Alpha of the helmet at a pixel of the canvas, 0 outside of the helmet
uint32_t getHelmetAlpha(const Part& helmet, uint32_t canvasX, uint32_t canvasY)
{
    if((canvasX < helmet.mX) || (canvasY < helmet.mY))
        return 0;

    uint32_t x = canvasX - helmet.mX;
    uint32_t y = canvasY - helmet.mY;
    if((x >= helmet.mImage.mWidth) || (y >= helmet.mImage.mHeight))
        return 0;

    return helmet.mImage.mPixels[(static_cast<size_t>(y) * helmet.mImage.mWidth + x) * 4 + 3];
}
}

bool isHelmetDamageClipped(const std::string& catalogId)
{
    return startsWith(catalogId, "Knight.") || startsWith(catalogId, "Cultist.");
}

void tintPart(Part& part, const std::string& catalogId, const std::string& optionName, const PortraitTint* tint,
    const std::string& creatureName)
{
    if((tint == nullptr) || !part.mImage.isValid())
        return;

    std::vector<std::string> keys;
    if(tint->hasMesh(part.mSlot))
        keys.push_back(part.mSlot);
    std::string optionKey = part.mSlot + ":" + optionName;
    if(tint->hasMesh(optionKey))
        keys.push_back(optionKey);
    std::string catalogKey = catalogId + ":" + optionKey;
    if(tint->hasMesh(catalogKey))
        keys.push_back(catalogKey);
    if(keys.empty())
        return;

    size_t pixels = static_cast<size_t>(part.mImage.mWidth) * part.mImage.mHeight;
    std::vector<float> rgb(pixels * 3);
    std::vector<float> coverage(pixels);
    for(size_t i = 0; i < pixels; ++i)
    {
        const uint8_t* pixel = &part.mImage.mPixels[i * 4];
        rgb[i * 3] = pixel[0] / 255.0f;
        rgb[i * 3 + 1] = pixel[1] / 255.0f;
        rgb[i * 3 + 2] = pixel[2] / 255.0f;
        coverage[i] = (pixel[3] > 0) ? 1.0f : 0.0f;
    }

    for(size_t k = 0; k < keys.size(); ++k)
        tint->apply(keys[k], creatureName, rgb, part.mImage.mWidth, part.mImage.mHeight, &coverage);

    for(size_t i = 0; i < pixels; ++i)
    {
        if(coverage[i] == 0.0f)
            continue;

        uint8_t* pixel = &part.mImage.mPixels[i * 4];
        pixel[0] = static_cast<uint8_t>(rgb[i * 3] * 255.0f + 0.5f);
        pixel[1] = static_cast<uint8_t>(rgb[i * 3 + 1] * 255.0f + 0.5f);
        pixel[2] = static_cast<uint8_t>(rgb[i * 3 + 2] * 255.0f + 0.5f);
    }
}

RgbaImage compose(const RgbaImage& base, const std::vector<Part>& parts, bool clipDamageToHelmet)
{
    if(!base.isValid())
        return RgbaImage();

    RgbaImage result = base;

    const Part* helmet = nullptr;
    if(clipDamageToHelmet)
    {
        for(std::vector<Part>::const_iterator it = parts.begin(); it != parts.end(); ++it)
        {
            if((it->mSlot == SLOT_HELMET) && it->mImage.isValid())
                helmet = &(*it);
        }
    }

    for(std::vector<Part>::const_iterator it = parts.begin(); it != parts.end(); ++it)
    {
        const Part& part = *it;
        if(!part.mImage.isValid())
            continue;

        bool clipToHelmet = (helmet != nullptr) && (part.mSlot == SLOT_SCAR);
        for(uint32_t y = 0; y < part.mImage.mHeight; ++y)
        {
            uint32_t canvasY = part.mY + y;
            if(canvasY >= result.mHeight)
                break;

            for(uint32_t x = 0; x < part.mImage.mWidth; ++x)
            {
                uint32_t canvasX = part.mX + x;
                if(canvasX >= result.mWidth)
                    break;

                const uint8_t* src = &part.mImage.mPixels[(static_cast<size_t>(y) * part.mImage.mWidth + x) * 4];
                uint32_t alpha = src[3];
                if(clipToHelmet)
                    alpha = (alpha * getHelmetAlpha(*helmet, canvasX, canvasY) + 127) / 255;

                uint8_t* dst = &result.mPixels[(static_cast<size_t>(canvasY) * result.mWidth + canvasX) * 4];
                blendPixel(dst, src, alpha);
            }
        }
    }
    return result;
}

std::vector<uint8_t> flattenAndTint(const RgbaImage& image, uint32_t downscale, const PortraitTint* tint,
    const std::string& tintKey, const std::string& creatureName, uint32_t& width, uint32_t& height)
{
    width = 0;
    height = 0;
    if(!image.isValid() || (downscale == 0))
        return std::vector<uint8_t>();

    uint32_t outWidth = image.mWidth / downscale;
    uint32_t outHeight = image.mHeight / downscale;
    if((outWidth == 0) || (outHeight == 0))
        return std::vector<uint8_t>();

    const float backgroundRed = BACKGROUND_RED * 255.0f;
    const float backgroundGreen = BACKGROUND_GREEN * 255.0f;
    const float backgroundBlue = BACKGROUND_BLUE * 255.0f;
    const float blockSize = static_cast<float>(downscale * downscale) * 255.0f * 255.0f;

    std::vector<float> rgb(static_cast<size_t>(outWidth) * outHeight * 3);
    for(uint32_t y = 0; y < outHeight; ++y)
    {
        for(uint32_t x = 0; x < outWidth; ++x)
        {
            float sum[3] = {0.0f, 0.0f, 0.0f};
            for(uint32_t dy = 0; dy < downscale; ++dy)
            {
                for(uint32_t dx = 0; dx < downscale; ++dx)
                {
                    const uint8_t* pixel = &image.mPixels[(static_cast<size_t>(y * downscale + dy) * image.mWidth +
                        x * downscale + dx) * 4];
                    float alpha = static_cast<float>(pixel[3]);
                    float inverse = 255.0f - alpha;
                    sum[0] += pixel[0] * alpha + backgroundRed * inverse;
                    sum[1] += pixel[1] * alpha + backgroundGreen * inverse;
                    sum[2] += pixel[2] * alpha + backgroundBlue * inverse;
                }
            }
            size_t index = (static_cast<size_t>(y) * outWidth + x) * 3;
            rgb[index] = sum[0] / blockSize;
            rgb[index + 1] = sum[1] / blockSize;
            rgb[index + 2] = sum[2] / blockSize;
        }
    }

    if(tint != nullptr)
        tint->apply(tintKey, creatureName, rgb, outWidth, outHeight);

    std::vector<uint8_t> data(static_cast<size_t>(outWidth) * outHeight * 4);
    for(size_t i = 0; i < static_cast<size_t>(outWidth) * outHeight; ++i)
    {
        data[i * 4] = static_cast<uint8_t>(rgb[i * 3] * 255.0f + 0.5f);
        data[i * 4 + 1] = static_cast<uint8_t>(rgb[i * 3 + 1] * 255.0f + 0.5f);
        data[i * 4 + 2] = static_cast<uint8_t>(rgb[i * 3 + 2] * 255.0f + 0.5f);
        data[i * 4 + 3] = 255;
    }

    width = outWidth;
    height = outHeight;
    return data;
}

uint32_t hashPixels(const std::vector<uint8_t>& pixels)
{
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < pixels.size(); ++i)
    {
        hash ^= pixels[i];
        hash *= 16777619u;
    }
    return hash;
}

std::string makePictureKey(const CreatureAppearance& appearance, const std::string& creatureName)
{
    return CreatureAppearanceLogic::toToken(appearance) + "|" + creatureName;
}

bool cacheTouch(std::vector<CacheEntry>& entries, const std::string& key)
{
    for(size_t i = 0; i < entries.size(); ++i)
    {
        if(entries[i].mKey != key)
            continue;

        CacheEntry entry = entries[i];
        entries.erase(entries.begin() + i);
        entries.push_back(entry);
        return true;
    }
    return false;
}

void cacheAdd(std::vector<CacheEntry>& entries, const std::string& key, uint64_t bytes)
{
    for(size_t i = 0; i < entries.size(); ++i)
    {
        if(entries[i].mKey == key)
        {
            entries.erase(entries.begin() + i);
            break;
        }
    }

    CacheEntry entry;
    entry.mKey = key;
    entry.mBytes = bytes;
    entries.push_back(entry);
}

std::vector<std::string> cacheEvict(std::vector<CacheEntry>& entries, uint32_t maxPictures, uint64_t maxBytes,
    uint32_t keepNewest)
{
    std::vector<std::string> removed;
    uint64_t total = 0;
    for(size_t i = 0; i < entries.size(); ++i)
        total += entries[i].mBytes;

    while((entries.size() > keepNewest) && ((entries.size() > maxPictures) || (total > maxBytes)))
    {
        removed.push_back(entries.front().mKey);
        total -= entries.front().mBytes;
        entries.erase(entries.begin());
    }
    return removed;
}
}
