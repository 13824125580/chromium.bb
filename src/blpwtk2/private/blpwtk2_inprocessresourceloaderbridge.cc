/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
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

#include <blpwtk2_inprocessresourceloaderbridge.h>

#include <blpwtk2_resourcecontext.h>
#include <blpwtk2_resourceloader.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_string.h>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <content/child/request_info.h>
#include <content/child/sync_load_response.h>
#include <content/public/child/request_peer.h>
#include <net/base/mime_sniffer.h>
#include <net/base/net_errors.h>
#include <net/http/http_response_headers.h>
#include <url/gurl.h>

namespace blpwtk2 {

class InProcessResourceLoaderBridge::InProcessResourceContext
    : public base::RefCounted<InProcessResourceContext>
    , public ResourceContext {
  public:
    InProcessResourceContext(const content::RequestInfo& requestInfo);

    // accessors
    const GURL& url() const;

    // manipulators
    bool start(content::RequestPeer* peer);
    void cancel();
    void dispose();

    // ResourceContext overrides
    virtual void replaceStatusLine(const StringRef& newStatus) OVERRIDE;
    virtual void addResponseHeader(const StringRef& header) OVERRIDE;
    virtual void addResponseData(const char* buffer, int length) OVERRIDE;
    virtual void failed() OVERRIDE;
    virtual void finish() OVERRIDE;

  private:
    void startLoad();
    void cancelLoad();
    void ensureResponseHeadersSent(const char* buffer, int length);

    GURL d_url;
    scoped_refptr<net::HttpResponseHeaders> d_responseHeaders;
    content::RequestPeer* d_peer;
    void* d_userData;
    int64 d_totalTransferSize;
    bool d_started;
    bool d_waitingForCancelLoad;  // waiting for cancelLoad()
    bool d_canceled;
    bool d_failed;
    bool d_finished;

    DISALLOW_COPY_AND_ASSIGN(InProcessResourceContext);
};

// InProcessResourceContext

InProcessResourceLoaderBridge::InProcessResourceContext::InProcessResourceContext(
    const content::RequestInfo& requestInfo)
: d_url(requestInfo.url)
, d_peer(0)
, d_userData(0)
, d_totalTransferSize(0)
, d_started(false)
, d_waitingForCancelLoad(false)
, d_canceled(false)
, d_failed(false)
, d_finished(false)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::inProcessResourceLoader);
    DCHECK(Statics::inProcessResourceLoader->canHandleURL(d_url.spec()));
    d_responseHeaders = new net::HttpResponseHeaders("HTTP/1.1 200 OK\0\0");
}

// accessors
const GURL& InProcessResourceLoaderBridge::InProcessResourceContext::url() const
{
    return d_url;
}

// manipulators
bool InProcessResourceLoaderBridge::InProcessResourceContext::start(content::RequestPeer* peer)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_started);
    DCHECK(!d_waitingForCancelLoad);
    DCHECK(!d_canceled);
    DCHECK(!d_failed);
    DCHECK(!d_finished);

    d_peer = peer;

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&InProcessResourceContext::startLoad,
                   this));
    return true;
}

void InProcessResourceLoaderBridge::InProcessResourceContext::cancel()
{
    DCHECK(Statics::isInApplicationMainThread());

    if (d_waitingForCancelLoad || d_canceled) {
        // Sometimes Cancel() gets called twice.  If we already got canceled,
        // then ignore any further calls to Cancel().
        return;
    }

    d_waitingForCancelLoad = true;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&InProcessResourceContext::cancelLoad,
                   this));
}

void InProcessResourceLoaderBridge::InProcessResourceContext::dispose()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_peer = NULL;
}

// ResourceContext overrides

void InProcessResourceLoaderBridge::InProcessResourceContext::replaceStatusLine(
    const StringRef& newStatus)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_failed);
    DCHECK(d_responseHeaders.get());

    std::string str(newStatus.data(), newStatus.length());
    d_responseHeaders->ReplaceStatusLine(str);
}

void InProcessResourceLoaderBridge::InProcessResourceContext::addResponseHeader(const StringRef& header)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_failed);
    DCHECK(d_responseHeaders.get());

    std::string str(header.data(), header.length());
    d_responseHeaders->AddHeader(str);
}

void InProcessResourceLoaderBridge::InProcessResourceContext::addResponseData(const char* buffer,
                                                                              int length)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_failed);

    if (0 == length)
        return;

    d_totalTransferSize += length;

    if (!d_peer)
        return;

    ensureResponseHeadersSent(buffer, length);

    // The bridge might have been disposed when sending the response
    // headers, so we need to check again.
    if (!d_peer)
        return;

    d_peer->OnReceivedData(buffer, length, length);
}

void InProcessResourceLoaderBridge::InProcessResourceContext::failed()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_finished);
    d_failed = true;
}

void InProcessResourceLoaderBridge::InProcessResourceContext::finish()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_finished);

    d_finished = true;

    if (d_waitingForCancelLoad) {
        // Application finished before we could notify it that the resource
        // was canceled.  We should wait for 'cancelLoad()' to get called,
        // where we will destroy ourself.

        // This is to balance the AddRef from startLoad().
        Release();
        return;
    }

    if (d_peer) {
        ensureResponseHeadersSent(0, 0);
    }

    // The bridge might have been disposed when the headers were sent,
    // so check this again.
    if (d_peer) {
        int errorCode = d_failed ? net::ERR_FAILED
                                 : d_canceled ? net::ERR_ABORTED
                                              : net::OK;

        // InProcessResourceLoaderBridge will get deleted inside this callback.
        d_peer->OnCompletedRequest(errorCode, false, false, "",
                                   base::TimeTicks::Now(), d_totalTransferSize);
    }

    // This is to balance the AddRef from startLoad().
    Release();
}

void InProcessResourceLoaderBridge::InProcessResourceContext::startLoad()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_started);
    DCHECK(!d_canceled);

    if (d_waitingForCancelLoad) {
        // We got canceled before we even started the resource on the loader.
        // We should wait for 'cancelLoad()' to get called, where we will
        // destroy ourself.
        return;
    }

    d_started = true;

    // Adding a reference on behalf of the embedder. This is Release'd
    // on finish().
    AddRef();
    Statics::inProcessResourceLoader->start(d_url.spec(), this, &d_userData);
}

void InProcessResourceLoaderBridge::InProcessResourceContext::cancelLoad()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_waitingForCancelLoad);

    if (!d_started || d_finished) {
        if (d_peer) {
            // Resource canceled before we could start it on the loader, or the
            // loader finished before we could notify it of cancellation.  We can
            // now safely destroy ourself.

            // InProcessResourceLoaderBridge will get deleted inside this callback.
            d_peer->OnCompletedRequest(net::ERR_ABORTED,
                                       false,
                                       false,
                                       "",
                                       base::TimeTicks::Now(),
                                       d_totalTransferSize);
        }
        return;
    }

    d_waitingForCancelLoad = false;
    d_canceled = true;
    Statics::inProcessResourceLoader->cancel(this, d_userData);
}

void InProcessResourceLoaderBridge::InProcessResourceContext::ensureResponseHeadersSent(
    const char* buffer,
    int length)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_peer);

    if (!d_responseHeaders.get()) {
        return;
    }

    content::ResourceResponseInfo responseInfo;
    responseInfo.headers = d_responseHeaders;
    responseInfo.content_length = d_responseHeaders->GetContentLength();
    d_responseHeaders->GetMimeTypeAndCharset(&responseInfo.mime_type,
                                             &responseInfo.charset);
    d_responseHeaders = 0;

    if (responseInfo.mime_type.empty() && length > 0) {
        net::SniffMimeType(buffer,
                           std::min(length, net::kMaxBytesToSniff),
                           d_url,
                           "",
                           &responseInfo.mime_type);
    }

    d_peer->OnReceivedResponse(responseInfo);
}


// InProcessResourceLoaderBridge

InProcessResourceLoaderBridge::InProcessResourceLoaderBridge(
    const content::RequestInfo& requestInfo)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::inProcessResourceLoader);

    d_context = new InProcessResourceContext(requestInfo);
}

InProcessResourceLoaderBridge::~InProcessResourceLoaderBridge()
{
    d_context->dispose();
}

// webkit_glue::ResourceLoaderBridge overrides

void InProcessResourceLoaderBridge::SetRequestBody(
    content::ResourceRequestBody* request_body)
{
}

bool InProcessResourceLoaderBridge::Start(content::RequestPeer* peer)
{
    DCHECK(Statics::isInApplicationMainThread());
    return d_context->start(peer);
}

void InProcessResourceLoaderBridge::Cancel()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_context->cancel();
}

void InProcessResourceLoaderBridge::SetDefersLoading(bool value)
{
}

void InProcessResourceLoaderBridge::DidChangePriority(
    net::RequestPriority new_priority,
    int intra_priority_value)
{
}

bool InProcessResourceLoaderBridge::AttachThreadedDataReceiver(
    blink::WebThreadedDataReceiver* threaded_data_receiver)
{
    return false;
}

void InProcessResourceLoaderBridge::SyncLoad(content::SyncLoadResponse* response)
{
    DLOG(ERROR) << "Synchronous requests not supported: url("
                << d_context->url() << ")";
    response->error_code = net::ERR_FAILED;
}


}  // close namespace blpwtk2

