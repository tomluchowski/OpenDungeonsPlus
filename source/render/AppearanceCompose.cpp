/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/AppearanceCompose.h"

#include "game/CreatureAppearance.h"
#include "render/PortraitTint.h"

#include <algorithm>
#include <cmath>

namespace AppearanceCompose
{
const uint32_t CACHE_KEEP_NEWEST = 4;

namespace
{
//! Dark background of the portraits, the same colour the model portraits are rendered on
const float BACKGROUND_RED = 0.025f;
const float BACKGROUND_GREEN = 0.018f;
const float BACKGROUND_BLUE = 0.015f;

//! Exclude only the measured source-space contour, with an optional narrow outside feather.
uint32_t polygonCoverage(const Part& part, uint32_t sourceX, uint32_t sourceY)
{
    const std::vector<uint32_t>& points = part.mSourcePolygon;
    if(points.empty())
        return 255;
    double x = sourceX + 0.5;
    double y = sourceY + 0.5;
    bool inside = false;
    double distanceSquared = 1.0e30;
    for(size_t i = 0, j = points.size() - 2; i < points.size(); j = i, i += 2)
    {
        double ax = points[j], ay = points[j + 1];
        double bx = points[i], by = points[i + 1];
        if(((ay > y) != (by > y)) && (x < (bx - ax) * (y - ay) / (by - ay) + ax))
            inside = !inside;
        double dx = bx - ax, dy = by - ay;
        double lengthSquared = dx * dx + dy * dy;
        double t = lengthSquared ? ((x - ax) * dx + (y - ay) * dy) / lengthSquared : 0;
        t = std::max(0.0, std::min(1.0, t));
        double px = x - (ax + t * dx), py = y - (ay + t * dy);
        distanceSquared = std::min(distanceSquared, px * px + py * py);
    }
    if(inside)
        return 0;
    if(!part.mMaskFeather || (distanceSquared >= part.mMaskFeather * part.mMaskFeather))
        return 255;
    return static_cast<uint32_t>(255 * std::sqrt(distanceSquared) / part.mMaskFeather + 0.5);
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
}

void tintPart(Part& part, const std::string& catalogId, const std::string& optionName, const PortraitTint* tint,
    const std::string& creatureName)
{
    if((tint == nullptr) || !part.mImage.isValid())
        return;

    // A block for this catalog id replaces the generic ones (slot, slot and option) for the part
    std::vector<std::string> keys;
    std::string optionKey = part.mSlot + ":" + optionName;
    std::string catalogKey = catalogId + ":" + optionKey;
    if(tint->hasMesh(catalogKey))
    {
        keys.push_back(catalogKey);
    }
    else
    {
        if(tint->hasMesh(part.mSlot))
            keys.push_back(part.mSlot);
        if(tint->hasMesh(optionKey))
            keys.push_back(optionKey);
    }
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

RgbaImage compose(const RgbaImage& base, const std::vector<Part>& parts)
{
    if(!base.isValid())
        return RgbaImage();

    RgbaImage result = base;

    for(std::vector<Part>::const_iterator it = parts.begin(); it != parts.end(); ++it)
    {
        const Part& part = *it;
        if(!part.mImage.isValid())
            continue;

        uint32_t width = part.mWidth ? part.mWidth : part.mImage.mWidth;
        uint32_t height = part.mHeight ? part.mHeight : part.mImage.mHeight;
        for(uint32_t y = 0; y < height; ++y)
        {
            uint32_t canvasY = part.mY + y;
            if(canvasY >= result.mHeight)
                break;

            for(uint32_t x = 0; x < width; ++x)
            {
                uint32_t canvasX = part.mX + x;
                if(canvasX >= result.mWidth)
                    break;

                uint32_t sourceX = (static_cast<uint64_t>(x) * 2 + 1) * part.mImage.mWidth / (static_cast<uint64_t>(width) * 2);
                uint32_t sourceY = (static_cast<uint64_t>(y) * 2 + 1) * part.mImage.mHeight / (static_cast<uint64_t>(height) * 2);
                const uint8_t* src = &part.mImage.mPixels[(static_cast<size_t>(sourceY) * part.mImage.mWidth + sourceX) * 4];
                uint32_t alpha = src[3] * polygonCoverage(part, sourceX, sourceY) / 255;
                if(!part.mMaskSlot.empty())
                {
                    for(std::vector<Part>::const_iterator mask = parts.begin(); mask != it; ++mask)
                    {
                        if((mask->mSlot != part.mMaskSlot) || !mask->mImage.isValid())
                            continue;
                        uint32_t maskWidth = mask->mWidth ? mask->mWidth : mask->mImage.mWidth;
                        uint32_t maskHeight = mask->mHeight ? mask->mHeight : mask->mImage.mHeight;
                        if((canvasX < mask->mX) || (canvasY < mask->mY) ||
                            (canvasX - mask->mX >= maskWidth) || (canvasY - mask->mY >= maskHeight))
                            continue;
                        uint32_t maskX = (static_cast<uint64_t>(canvasX - mask->mX) * 2 + 1) *
                            mask->mImage.mWidth / (static_cast<uint64_t>(maskWidth) * 2);
                        uint32_t maskY = (static_cast<uint64_t>(canvasY - mask->mY) * 2 + 1) *
                            mask->mImage.mHeight / (static_cast<uint64_t>(maskHeight) * 2);
                        uint32_t maskAlpha = mask->mImage.mPixels[(static_cast<size_t>(maskY) *
                            mask->mImage.mWidth + maskX) * 4 + 3];
                        maskAlpha = maskAlpha * polygonCoverage(*mask, maskX, maskY) / 255;
                        alpha = alpha * (255 - maskAlpha) / 255;
                    }
                }

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
