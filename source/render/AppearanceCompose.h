/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APPEARANCECOMPOSE_H
#define APPEARANCECOMPOSE_H

#include <stdint.h>
#include <string>
#include <vector>

class CreatureAppearance;
class PortraitTint;

//! \brief Composing of the Dungeonbook picture on raw RGBA buffers, plus the logic of the picture cache.
//! Pure source without any Ogre or CEGUI dependency, so the pixels can be hashed in a test.
namespace AppearanceCompose
{
//! Pixels are stored row by row, four bytes (red, green, blue, alpha, not premultiplied) each
struct RgbaImage
{
    RgbaImage() :
        mWidth(0),
        mHeight(0)
    {
    }

    RgbaImage(uint32_t width, uint32_t height) :
        mWidth(width),
        mHeight(height),
        mPixels(static_cast<size_t>(width) * height * 4, 0)
    {
    }

    bool isValid() const
    { return (mWidth > 0) && (mHeight > 0) && (mPixels.size() == static_cast<size_t>(mWidth) * mHeight * 4); }

    uint32_t mWidth;
    uint32_t mHeight;
    std::vector<uint8_t> mPixels;
};

//! One chosen part with its slot position on the base
struct Part
{
    std::string mSlot;
    uint32_t mX;
    uint32_t mY;
    RgbaImage mImage;
};

//! Slot names the helmet rule needs (as written in the manifests)
extern const char* const SLOT_HELMET;
extern const char* const SLOT_SCAR;

//! \brief True for the catalog ids whose scar slot holds helmet damage (Knight and Cultist): there the
//! scar is limited to the alpha of the chosen helmet.
bool isHelmetDamageClipped(const std::string& catalogId);

//! \brief Colours one chosen part (hair, beard and the like) for the creature, before the parts are composed.
//! The bases are bare, so hair, eyes and beards get their colours from the parts: tint is the part tint file
//! (config/dungeonbook-part-tints.cfg), its entries are keyed by the slot ("hair") or by slot and option
//! name ("chin:forked"), or by catalog id, slot and option name ("Orc.mesh:eyes:round", for the eyes, whose
//! iris lies at another place in every base); all keys that exist are applied. Same code path as the base tint
//! (PortraitTint::apply, the colour of a region is chosen from the creature name and the region name), so a
//! creature gets the same hair and beard colour wherever the region is called the same. Only visible pixels
//! (alpha above 0) are coloured, the alpha is never changed. Nothing happens if tint is null or has no
//! entry for the part.
void tintPart(Part& part, const std::string& catalogId, const std::string& optionName, const PortraitTint* tint,
    const std::string& creatureName);

//! \brief Draws the parts onto a copy of the base, in the order of the vector (the order of the Slot
//! lines of the manifest), with alpha blending at the slot position. Parts that are invalid or reach
//! outside of the base are cut at its border. If clipDamageToHelmet is set and a part of the helmet slot
//! is present, the alpha of the scar part is multiplied with the alpha of the helmet at the same pixel
//! (zero outside of the helmet), so helmet damage never leaves the helmet. Without a helmet part the
//! scar is drawn as it is. An invalid base gives an empty image.
RgbaImage compose(const RgbaImage& base, const std::vector<Part>& parts, bool clipDamageToHelmet);

//! \brief Puts the image over the dark portrait background, averages blocks of downscale x downscale
//! pixels (alpha ignored afterwards, like the portrait of the creature bar), colours it with
//! PortraitTint::apply(tintKey, creatureName, ...) (the same code path and the same colour per creature
//! as the profile portrait; nothing happens if tint is null or has no regions for tintKey) and returns
//! opaque RGBA bytes. width and height receive the size of the result; the result is empty if the image is
//! invalid or smaller than one block.
std::vector<uint8_t> flattenAndTint(const RgbaImage& image, uint32_t downscale, const PortraitTint* tint,
    const std::string& tintKey, const std::string& creatureName, uint32_t& width, uint32_t& height);

//! \brief Fixed 32 bit FNV-1a hash of the pixel bytes (for tests and diagnostics).
uint32_t hashPixels(const std::vector<uint8_t>& pixels);

//! \brief Cache key of a composed picture: catalog id, chosen options and the tint (the creature name, the
//! tint colours are a function of it), e.g. "Knight.mesh-male:hair=1,helmet=2|Brak".
std::string makePictureKey(const CreatureAppearance& appearance, const std::string& creatureName);

//! One cached picture; the vector of entries is ordered by use, the least recently used is first
struct CacheEntry
{
    std::string mKey;
    uint64_t mBytes;
};

//! Number of newest entries that are never evicted, so a picture that was just handed out cannot vanish
//! while its window still shows it
extern const uint32_t CACHE_KEEP_NEWEST;

//! Marks the entry as most recently used. Returns false if there is no entry with this key.
bool cacheTouch(std::vector<CacheEntry>& entries, const std::string& key);

//! Adds an entry as the most recently used one (an existing entry with the same key is replaced)
void cacheAdd(std::vector<CacheEntry>& entries, const std::string& key, uint64_t bytes);

//! \brief Removes the least recently used entries while there are more than maxPictures of them or
//! together more than maxBytes bytes, but never one of the newest keepNewest entries. Returns the keys of
//! the removed entries, oldest first, so the caller can release their textures.
std::vector<std::string> cacheEvict(std::vector<CacheEntry>& entries, uint32_t maxPictures, uint64_t maxBytes,
    uint32_t keepNewest);
}

#endif // APPEARANCECOMPOSE_H
