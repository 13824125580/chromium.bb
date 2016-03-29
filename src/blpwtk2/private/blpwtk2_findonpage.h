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

#ifndef INCLUDED_BLPWTK2_FINDONPAGE_H
#define INCLUDED_BLPWTK2_FINDONPAGE_H

#include <blpwtk2_config.h>

#include <string>

namespace content {
class RenderViewHost;
}

namespace blpwtk2 {

struct FindOnPageRequest
{
    int reqId;
    std::wstring text;
    bool matchCase;
    bool findNext;
    bool forward;
};

class FindOnPage
{
public:
    FindOnPage()
    : d_reqId(0)
    , d_numberOfMatches(0)
    , d_activeMatchOrdinal(0)
    {
    }

    int numberOfMatches() const { return d_numberOfMatches; }

    // zero-based index of the active match
    int activeMatchIndex() const { return d_activeMatchOrdinal - 1; }

    FindOnPageRequest makeRequest(const std::wstring& text,
                                  bool matchCase,
                                  bool forward);
    bool applyUpdate(int reqId, int numberOfMatches, int activeMatchOrdinal);


private:
    std::wstring d_text;
    int d_reqId;
    int d_numberOfMatches;
    int d_activeMatchOrdinal; // 1-based index
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_FINDONPAGE_H
