/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_toolkitcreateparams.h>

#include <blpwtk2_channelinfo.h>
#include <blpwtk2_constants.h>
#include <blpwtk2_products.h>
#include <blpwtk2_stringref.h>

#include <base/logging.h>  // for DCHECK
#include <content/public/common/content_switches.h>

#include <string>
#include <vector>

namespace blpwtk2 {

struct ToolkitCreateParamsImpl {
    ThreadMode::Value d_threadMode;
    PumpMode::Value d_pumpMode;
    int d_maxSocketsPerProxy;
    std::vector<std::string> d_commandLineSwitches;
    std::vector<std::string> d_sideLoadedFonts;
    ResourceLoader* d_inProcessResourceLoader;
    std::string d_dictionaryPath;
    std::string d_hostChannel;
    NativeFont d_tooltipFont;

    ToolkitCreateParamsImpl()
    : d_threadMode(ThreadMode::ORIGINAL)
    , d_pumpMode(PumpMode::MANUAL)
    , d_maxSocketsPerProxy(-1)
    , d_inProcessResourceLoader(0)
    , d_tooltipFont(0)
    {
    }
};

ToolkitCreateParams::ToolkitCreateParams()
: d_impl(new ToolkitCreateParamsImpl())
{
}

ToolkitCreateParams::ToolkitCreateParams(const ToolkitCreateParams& other)
: d_impl(new ToolkitCreateParamsImpl(*other.d_impl))
{
}

ToolkitCreateParams::~ToolkitCreateParams()
{
    delete d_impl;
}

ToolkitCreateParams& ToolkitCreateParams::operator=(
    const ToolkitCreateParams& rhs)
{
    if (this != &rhs) {
        *d_impl = *rhs.d_impl;
    }
    return *this;
}

void ToolkitCreateParams::setThreadMode(ThreadMode::Value mode)
{
    d_impl->d_threadMode = mode;
}

void ToolkitCreateParams::setPumpMode(PumpMode::Value mode)
{
    d_impl->d_pumpMode = mode;
}

void ToolkitCreateParams::setMaxSocketsPerProxy(int count)
{
    DCHECK(1 <= count);
    DCHECK(99 >= count);
    d_impl->d_maxSocketsPerProxy = count;
}

void ToolkitCreateParams::appendCommandLineSwitch(const StringRef& switchString)
{
    d_impl->d_commandLineSwitches.push_back(std::string());
    d_impl->d_commandLineSwitches.back().assign(switchString.data(),
                                                switchString.length());
}

void ToolkitCreateParams::appendSideLoadedFontInProcess(const StringRef& fontFile)
{
    d_impl->d_sideLoadedFonts.push_back(std::string());
    d_impl->d_sideLoadedFonts.back().assign(fontFile.data(),
                                            fontFile.length());
}

void ToolkitCreateParams::disablePluginDiscovery()
{
    appendCommandLineSwitch(switches::kDisablePluginsDiscovery);
}

void ToolkitCreateParams::setInProcessResourceLoader(
    ResourceLoader* loader)
{
    DCHECK(loader);
    d_impl->d_inProcessResourceLoader = loader;
}

void ToolkitCreateParams::setDictionaryPath(const StringRef& path)
{
    d_impl->d_dictionaryPath.assign(path.data(), path.length());
}

void ToolkitCreateParams::setHostChannel(const StringRef& channelInfoString)
{
    CHECK(channelInfoString.isEmpty() || isValidHostChannelVersion(channelInfoString));
    d_impl->d_hostChannel.assign(
        channelInfoString.data(),
        channelInfoString.length());
}

// static
bool ToolkitCreateParams::isValidHostChannelVersion(const StringRef& channelInfoString)
{
    ChannelInfo channelInfo;
    if (!channelInfo.deserialize(channelInfoString))
        return false;
    const std::string& channelId = channelInfo.d_channelId;
    static StringRef expected(BLPWTK2_VERSION ".");
    return channelId.empty() ||
        (channelId.length() > expected.length() &&
         expected.equals(StringRef(channelId.data(), expected.length())));
}

void ToolkitCreateParams::setTooltipStyle(NativeFont font)
{
    d_impl->d_tooltipFont = font;
}

ThreadMode::Value ToolkitCreateParams::threadMode() const
{
    return d_impl->d_threadMode;
}

PumpMode::Value ToolkitCreateParams::pumpMode() const
{
    return d_impl->d_pumpMode;
}

bool ToolkitCreateParams::isMaxSocketsPerProxySet() const
{
    return -1 != d_impl->d_maxSocketsPerProxy;
}

int ToolkitCreateParams::maxSocketsPerProxy() const
{
    DCHECK(isMaxSocketsPerProxySet());
    return d_impl->d_maxSocketsPerProxy;
}

size_t ToolkitCreateParams::numCommandLineSwitches() const
{
    return d_impl->d_commandLineSwitches.size();
}

StringRef ToolkitCreateParams::commandLineSwitchAt(size_t index) const
{
    DCHECK(index < d_impl->d_commandLineSwitches.size());
    return d_impl->d_commandLineSwitches[index];
}

size_t ToolkitCreateParams::numSideLoadedFonts() const
{
    return d_impl->d_sideLoadedFonts.size();
}

StringRef ToolkitCreateParams::sideLoadedFontAt(size_t index) const
{
    DCHECK(index < d_impl->d_sideLoadedFonts.size());
    return d_impl->d_sideLoadedFonts[index];
}

ResourceLoader* ToolkitCreateParams::inProcessResourceLoader() const
{
    return d_impl->d_inProcessResourceLoader;
}

StringRef ToolkitCreateParams::dictionaryPath() const
{
    return d_impl->d_dictionaryPath;
}

StringRef ToolkitCreateParams::hostChannel() const
{
    return d_impl->d_hostChannel;
}

NativeFont ToolkitCreateParams::tooltipFont() const
{
    return d_impl->d_tooltipFont;
}

}  // close namespace blpwtk2

