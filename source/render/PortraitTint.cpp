/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/PortraitTint.h"

#include "social/SocialRng.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace
{
const float HUE_FEATHER = 10.0f;
const float SV_FEATHER = 0.08f;
const float BOX_FEATHER = 0.02f;

float clamp01(float value)
{
    return std::min(1.0f, std::max(0.0f, value));
}

//! 1 inside [lo, hi], falling linearly to 0 within feather outside of it
float ramp(float value, float lo, float hi, float feather)
{
    float below = clamp01(1.0f - (lo - value) / feather);
    float above = clamp01(1.0f - (value - hi) / feather);
    return std::min(below, above);
}

float circularDistance(float a, float b)
{
    float distance = std::fabs(a - b);
    return std::min(distance, 360.0f - distance);
}

float hueWeight(float hue, float lo, float hi)
{
    bool inside;
    if(lo <= hi)
        inside = (hue >= lo) && (hue <= hi);
    else
        inside = (hue >= lo) || (hue <= hi);
    if(inside)
        return 1.0f;
    float distance = std::min(circularDistance(hue, lo), circularDistance(hue, hi));
    return clamp01(1.0f - distance / HUE_FEATHER);
}

void rgbToHsv(float r, float g, float b, float& h, float& s, float& v)
{
    float mx = std::max(r, std::max(g, b));
    float mn = std::min(r, std::min(g, b));
    float d = mx - mn;
    h = 0.0f;
    if(d > 0.0f)
    {
        if(mx == r)
            h = std::fmod((g - b) / d, 6.0f);
        else if(mx == g)
            h = (b - r) / d + 2.0f;
        else
            h = (r - g) / d + 4.0f;
        h *= 60.0f;
        if(h < 0.0f)
            h += 360.0f;
    }
    s = (mx > 0.0f) ? d / mx : 0.0f;
    v = mx;
}

void hsvToRgb(float h, float s, float v, float& r, float& g, float& b)
{
    h = h - 360.0f * std::floor(h / 360.0f);
    float hp = h / 60.0f;
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float m = v - c;
    int sector = static_cast<int>(std::floor(hp)) % 6;
    float rr = 0.0f;
    float gg = 0.0f;
    float bb = 0.0f;
    switch(sector)
    {
        case 0: rr = c; gg = x; break;
        case 1: rr = x; gg = c; break;
        case 2: gg = c; bb = x; break;
        case 3: gg = x; bb = c; break;
        case 4: rr = x; bb = c; break;
        default: rr = c; bb = x; break;
    }
    r = rr + m;
    g = gg + m;
    b = bb + m;
}

std::string trim(const std::string& text)
{
    std::string::size_type first = text.find_first_not_of(" \t\r\n");
    if(first == std::string::npos)
        return std::string();
    std::string::size_type last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::vector<std::string> splitColumns(const std::string& line)
{
    std::vector<std::string> columns;
    std::string::size_type start = 0;
    while(true)
    {
        std::string::size_type tab = line.find('\t', start);
        if(tab == std::string::npos)
        {
            columns.push_back(trim(line.substr(start)));
            break;
        }
        columns.push_back(trim(line.substr(start, tab - start)));
        start = tab + 1;
    }
    return columns;
}

bool parseFloat(const std::string& text, float& value)
{
    if(text.empty())
        return false;
    char* end = nullptr;
    value = static_cast<float>(std::strtod(text.c_str(), &end));
    return *end == '\0';
}

bool parseFloats(const std::string& text, std::vector<float>& values)
{
    values.clear();
    std::istringstream stream(text);
    std::string item;
    while(std::getline(stream, item, ','))
    {
        float value;
        if(!parseFloat(trim(item), value))
            return false;
        values.push_back(value);
    }
    return !values.empty();
}
}

void PortraitTint::addError(uint32_t lineNumber, const std::string& message)
{
    std::ostringstream stream;
    stream << mSource << ":" << lineNumber << ": " << message;
    mErrors.push_back(stream.str());
}

bool PortraitTint::parseRegion(const std::vector<std::string>& columns, Region& region, std::string& error) const
{
    if(columns.size() < 3)
    {
        error = "Region needs a name and settings";
        return false;
    }
    region.mName = columns[1];
    bool hasBox = false;
    bool hasHue = false;
    bool hasSat = false;
    bool hasVal = false;
    bool hasMode = false;
    for(size_t i = 2; i < columns.size(); ++i)
    {
        std::string::size_type equals = columns[i].find('=');
        if(equals == std::string::npos)
        {
            error = "expected key=value but found '" + columns[i] + "'";
            return false;
        }
        std::string key = columns[i].substr(0, equals);
        std::string text = columns[i].substr(equals + 1);
        if(key == "palette")
        {
            for(size_t p = 0; p < mPalettes.size(); ++p)
            {
                if(mPalettes[p].mName == text)
                    region.mPalette = static_cast<int>(p);
            }
            if(region.mPalette < 0)
            {
                error = "unknown palette '" + text + "'";
                return false;
            }
            hasMode = true;
            continue;
        }
        std::vector<float> values;
        if(!parseFloats(text, values))
        {
            error = "bad numbers in '" + columns[i] + "'";
            return false;
        }
        if((key == "shift") && (values.size() == 3))
        {
            for(int k = 0; k < 3; ++k)
                region.mShift[k] = values[k];
            hasMode = true;
        }
        else if((key == "box") && (values.size() == 4))
        {
            for(int k = 0; k < 4; ++k)
                region.mBox[k] = values[k];
            hasBox = true;
        }
        else if((key == "not") && (values.size() % 4 == 0))
            region.mCutOut = values;
        else if((key == "hue") && (values.size() == 2))
        {
            region.mHue[0] = values[0];
            region.mHue[1] = values[1];
            hasHue = true;
        }
        else if((key == "sat") && (values.size() == 2))
        {
            region.mSaturation[0] = values[0];
            region.mSaturation[1] = values[1];
            hasSat = true;
        }
        else if((key == "val") && (values.size() == 2))
        {
            region.mValue[0] = values[0];
            region.mValue[1] = values[1];
            hasVal = true;
        }
        else
        {
            error = "unknown key or wrong number of values in '" + columns[i] + "'";
            return false;
        }
    }
    if(!hasMode || !hasBox || !hasHue || !hasSat || !hasVal)
    {
        error = "Region '" + region.mName + "' needs palette= or shift=, box=, hue=, sat= and val=";
        return false;
    }
    return true;
}

bool PortraitTint::loadFromFile(const std::string& path)
{
    mPalettes.clear();
    mPortraits.clear();
    mErrors.clear();
    mSource = path;
    std::ifstream file(path.c_str());
    if(!file.is_open())
    {
        addError(0, "cannot open the portrait tint file, portraits stay unchanged");
        return false;
    }

    // 0 inside a palette block, 1 inside a portrait block
    int current = -1;
    std::string line;
    uint32_t lineNumber = 0;
    while(std::getline(file, line))
    {
        ++lineNumber;
        std::string::size_type comment = line.find('#');
        if(comment != std::string::npos)
            line = line.substr(0, comment);
        if(trim(line).empty())
            continue;
        std::vector<std::string> columns = splitColumns(line);
        const std::string& key = columns[0];
        if(key == "[Palette]")
        {
            mPalettes.push_back(Palette());
            current = 0;
        }
        else if(key == "[Portrait]")
        {
            mPortraits.push_back(Portrait());
            current = 1;
        }
        else if((key == "Name") && (current == 0) && (columns.size() >= 2))
            mPalettes.back().mName = columns[1];
        else if((key == "Colour") && (current == 0) && (columns.size() >= 5))
        {
            Colour colour;
            if(!parseFloat(columns[2], colour.mHue) || !parseFloat(columns[3], colour.mSaturation) ||
                !parseFloat(columns[4], colour.mValue))
            {
                addError(lineNumber, "bad Colour numbers");
                continue;
            }
            mPalettes.back().mColours.push_back(colour);
        }
        else if((key == "Mesh") && (current == 1) && (columns.size() >= 2))
            mPortraits.back().mMesh = columns[1];
        else if((key == "Region") && (current == 1))
        {
            Region region;
            std::string error;
            if(!parseRegion(columns, region, error))
                addError(lineNumber, error);
            else if((region.mPalette >= 0) && mPalettes[region.mPalette].mColours.empty())
                addError(lineNumber, "palette '" + mPalettes[region.mPalette].mName + "' has no colours");
            else
                mPortraits.back().mRegions.push_back(region);
        }
        else
            addError(lineNumber, "unexpected line '" + key + "'");
    }
    return true;
}

bool PortraitTint::hasMesh(const std::string& meshName) const
{
    for(size_t i = 0; i < mPortraits.size(); ++i)
    {
        if((mPortraits[i].mMesh == meshName) && !mPortraits[i].mRegions.empty())
            return true;
    }
    return false;
}

void PortraitTint::computeWeights(const Region& region, const std::vector<float>& hue,
    const std::vector<float>& saturation, const std::vector<float>& value, uint32_t width, uint32_t height,
    std::vector<float>& weights) const
{
    std::vector<float> columnWeight(width);
    std::vector<float> rowWeight(height);
    for(uint32_t x = 0; x < width; ++x)
        columnWeight[x] = ramp((x + 0.5f) / width, region.mBox[0], region.mBox[2], BOX_FEATHER);
    for(uint32_t y = 0; y < height; ++y)
        rowWeight[y] = ramp((y + 0.5f) / height, region.mBox[1], region.mBox[3], BOX_FEATHER);

    weights.assign(static_cast<size_t>(width) * height, 0.0f);
    for(uint32_t y = 0; y < height; ++y)
    {
        for(uint32_t x = 0; x < width; ++x)
        {
            size_t index = static_cast<size_t>(y) * width + x;
            float weight = rowWeight[y] * columnWeight[x];
            if(weight <= 0.0f)
                continue;
            weight *= hueWeight(hue[index], region.mHue[0], region.mHue[1]);
            weight *= ramp(saturation[index], region.mSaturation[0], region.mSaturation[1], SV_FEATHER);
            weight *= ramp(value[index], region.mValue[0], region.mValue[1], SV_FEATHER);
            for(size_t c = 0; (weight > 0.0f) && (c + 3 < region.mCutOut.size()); c += 4)
            {
                float cut = ramp((y + 0.5f) / height, region.mCutOut[c + 1], region.mCutOut[c + 3], BOX_FEATHER) *
                    ramp((x + 0.5f) / width, region.mCutOut[c], region.mCutOut[c + 2], BOX_FEATHER);
                weight *= 1.0f - cut;
            }
            weights[index] = weight;
        }
    }
}

void PortraitTint::apply(const std::string& meshName, const std::string& creatureName, std::vector<float>& rgb,
    uint32_t width, uint32_t height) const
{
    size_t pixels = static_cast<size_t>(width) * height;
    if((pixels == 0) || (rgb.size() != pixels * 3))
        return;

    std::vector<float> hue(pixels);
    std::vector<float> saturation(pixels);
    std::vector<float> value(pixels);
    for(size_t i = 0; i < pixels; ++i)
        rgbToHsv(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2], hue[i], saturation[i], value[i]);

    std::vector<float> weights;
    for(size_t p = 0; p < mPortraits.size(); ++p)
    {
        if(mPortraits[p].mMesh != meshName)
            continue;
        for(size_t r = 0; r < mPortraits[p].mRegions.size(); ++r)
        {
            const Region& region = mPortraits[p].mRegions[r];
            computeWeights(region, hue, saturation, value, width, height, weights);
            double total = 0.0;
            double valueSum = 0.0;
            for(size_t i = 0; i < pixels; ++i)
            {
                total += weights[i];
                valueSum += weights[i] * value[i];
            }
            if(total < 1.0)
                continue;

            social::Rng rng = social::makeFieldRng(creatureName, "portrait:" + region.mName);
            Colour target = {0.0f, 0.0f, 0.0f};
            float meanValue = 0.05f;
            float shiftHue = 0.0f;
            float shiftSaturation = 1.0f;
            float shiftValue = 1.0f;
            if(region.mPalette >= 0)
            {
                const std::vector<Colour>& colours = mPalettes[region.mPalette].mColours;
                target = colours[rng.below(static_cast<uint32_t>(colours.size()))];
                meanValue = std::max(0.05f, static_cast<float>(valueSum / total));
            }
            else
            {
                shiftHue = (rng.below(2001) / 1000.0f - 1.0f) * region.mShift[0];
                shiftSaturation = 1.0f + (rng.below(2001) / 1000.0f - 1.0f) * region.mShift[1];
                shiftValue = 1.0f + (rng.below(2001) / 1000.0f - 1.0f) * region.mShift[2];
            }

            for(size_t i = 0; i < pixels; ++i)
            {
                float weight = weights[i];
                if(weight <= 0.0f)
                    continue;
                float newHue;
                float newSaturation;
                float newValue;
                if(region.mPalette >= 0)
                {
                    newHue = target.mHue;
                    newSaturation = target.mSaturation;
                    newValue = std::min(1.0f, target.mValue * std::pow(std::max(value[i], 0.01f) / meanValue, 0.85f));
                }
                else
                {
                    newHue = hue[i] + shiftHue;
                    newSaturation = clamp01(saturation[i] * shiftSaturation);
                    newValue = clamp01(value[i] * shiftValue);
                }
                float r;
                float g;
                float b;
                hsvToRgb(newHue, newSaturation, newValue, r, g, b);
                rgb[i * 3] = rgb[i * 3] * (1.0f - weight) + r * weight;
                rgb[i * 3 + 1] = rgb[i * 3 + 1] * (1.0f - weight) + g * weight;
                rgb[i * 3 + 2] = rgb[i * 3 + 2] * (1.0f - weight) + b * weight;
            }
        }
    }
    for(size_t i = 0; i < rgb.size(); ++i)
        rgb[i] = clamp01(rgb[i]);
}
