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

#include <blpwtk2_browsercontextimpl.h>

#include <blpwtk2_prefstore.h>
#include <blpwtk2_proxyconfig.h>
#include <blpwtk2_proxyconfigimpl.h>
#include <blpwtk2_resourcecontextimpl.h>
#include <blpwtk2_spellcheckconfig.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_urlrequestcontextgetterimpl.h>
#include <blpwtk2_fontcollectionimpl.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>  // for CHECK
#include <components/prefs/pref_service_factory.h>
#include <components/prefs/pref_service.h>
#include <base/threading/thread_restrictions.h>
#include <chrome/browser/spellchecker/spellcheck_factory.h>
#include <chrome/common/pref_names.h>
#include <chrome/common/spellcheck_common.h>
#include <content/public/browser/browser_thread.h>
#include <content/public/browser/render_process_host.h>
#include <content/public/browser/spellcheck_data.h>
#include <content/public/browser/storage_partition.h>
#include <components/keyed_service/content/browser_context_dependency_manager.h>
#include <components/pref_registry/pref_registry_syncable.h>
#include <components/user_prefs/user_prefs.h>

namespace blpwtk2 {

BrowserContextImpl::BrowserContextImpl(const std::string& dataDir,
                                       bool diskCacheEnabled,
                                       bool cookiePersistenceEnabled)
: d_numWebViews(0)
, d_isIncognito(dataDir.empty())
, d_isDestroyed(false)
{
    // If disk-cache/cookie-persistence is enabled, then it must not be
    // incognito.
    DCHECK(!diskCacheEnabled || !d_isIncognito);
    DCHECK(!cookiePersistenceEnabled || !d_isIncognito);

    if (Statics::isOriginalThreadMode() || Statics::isSingleThreadMode()) {
        ++Statics::numProfiles;
    }

    base::FilePath path;

    if (!d_isIncognito) {
        // allow IO during creation of data directory
        base::ThreadRestrictions::ScopedAllowIO allowIO;

        path = base::FilePath::FromUTF8Unsafe(dataDir);
        if (!base::PathExists(path))
            base::CreateDirectory(path);
    }
    else {
        // It seems that even incognito browser contexts need to return a valid
        // data path.  Not providing one causes a DCHECK failure in
        // fileapi::DeviceMediaAsyncFileUtil::Create.
        // For now, just create a temporary directory for this.
        // TODO: see if we can remove this requirement.

        // allow IO during creation of temporary directory
        base::ThreadRestrictions::ScopedAllowIO allowIO;
        base::CreateNewTempDirectory(L"blpwtk2_", &path);
    }

    d_requestContextGetter =
        new URLRequestContextGetterImpl(path,
                                        diskCacheEnabled,
                                        cookiePersistenceEnabled);

    {
        // Initialize prefs for this context.
        d_prefRegistry = new user_prefs::PrefRegistrySyncable();
        d_userPrefs = new PrefStore();

        PrefServiceFactory factory;
        factory.set_user_prefs(d_userPrefs);
        d_prefService = factory.Create(d_prefRegistry.get());
        user_prefs::UserPrefs::Set(this, d_prefService.get());
        d_prefRegistry->RegisterBooleanPref(prefs::kPrintingEnabled, true);
    }

    content::SpellcheckData::CreateForContext(this);

    SpellcheckServiceFactory::GetInstance();  // This needs to be initialized before
                                              // calling CreateBrowserContextServices.

    BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(this);
    content::BrowserContext::Initialize(this, base::FilePath());
}

BrowserContextImpl::~BrowserContextImpl()
{
    DCHECK(d_isDestroyed);
}

URLRequestContextGetterImpl* BrowserContextImpl::requestContextGetter() const
{
    DCHECK(!d_isDestroyed);
    return d_requestContextGetter.get();
}

void BrowserContextImpl::incrementWebViewCount()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);
    ++d_numWebViews;
}

void BrowserContextImpl::decrementWebViewCount()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(0 < d_numWebViews);
    DCHECK(!d_isDestroyed);
    --d_numWebViews;
}

bool BrowserContextImpl::diskCacheEnabled() const
{
    DCHECK(!d_isDestroyed);
    return d_requestContextGetter->diskCacheEnabled();
}

bool BrowserContextImpl::cookiePersistenceEnabled() const
{
    DCHECK(!d_isDestroyed);
    return d_requestContextGetter->cookiePersistenceEnabled();
}

void BrowserContextImpl::reallyDestroy()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(0 == d_numWebViews);
    DCHECK(!d_isDestroyed);

    if (Statics::isOriginalThreadMode() || Statics::isSingleThreadMode()) {
        DCHECK(0 < Statics::numProfiles);
        --Statics::numProfiles;
    }

    BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(this);

    d_userPrefs = 0;
    d_prefRegistry = 0;

    content::BrowserThread::DeleteSoon(content::BrowserThread::UI,
                                       FROM_HERE,
                                       d_prefService.release());

    if (d_resourceContext.get()) {
        content::BrowserThread::DeleteSoon(content::BrowserThread::IO,
                                           FROM_HERE,
                                           d_resourceContext.release());
    }

    if (d_isIncognito) {
        // Delete the temporary directory that we created in the constructor.

        // allow IO during deletion of temporary directory
        base::ThreadRestrictions::ScopedAllowIO allowIO;
        DCHECK(base::PathExists(d_requestContextGetter->path()));
        base::DeleteFile(d_requestContextGetter->path(), true);
    }

    d_requestContextGetter = 0;
    d_isDestroyed = true;
}

// Profile overrides

void BrowserContextImpl::destroy()
{
    // This is a no-op because BrowserContextImplManager needs to keep the
    // BrowserContextImpl objects alive until Toolkit is destroyed.
}

void BrowserContextImpl::setProxyConfig(const ProxyConfig& config)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);
    d_requestContextGetter->setProxyConfig(
        getProxyConfigImpl(config)->d_config);
}

void BrowserContextImpl::useSystemProxyConfig()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);
    d_requestContextGetter->useSystemProxyConfig();
}

void BrowserContextImpl::setSpellCheckConfig(const SpellCheckConfig& config)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    PrefService* prefs = user_prefs::UserPrefs::Get(this);

    bool wasEnabled = prefs->GetBoolean(prefs::kEnableContinuousSpellcheck);

    base::ListValue languages;
    for (size_t i = 0; i < config.numLanguages(); ++i) {
        StringRef str = config.languageAt(i);
        languages.AppendString(str.toStdString());
    }
    prefs->SetBoolean(prefs::kEnableContinuousSpellcheck,
                      config.isSpellCheckEnabled());
    prefs->Set(prefs::kSpellCheckDictionaries, languages);

    if (!wasEnabled && config.isSpellCheckEnabled()) {
        // Ensure the spellcheck service is created for this context if we just
        // turned it on.
        SpellcheckServiceFactory::GetForContext(this);
    }
}

void BrowserContextImpl::addCustomWords(const StringRef* words,
                                        size_t numWords)
{
    std::vector<base::StringPiece> wordsVector(numWords);
    for (size_t i = 0; i < numWords; ++i) {
        wordsVector[i].set(words[i].data(), words[i].length());
    }
    content::SpellcheckData::FromContext(this)->AdjustCustomWords(
        wordsVector,
        std::vector<base::StringPiece>());
}

void BrowserContextImpl::removeCustomWords(const StringRef* words,
                                           size_t numWords)
{
    std::vector<base::StringPiece> wordsVector(numWords);
    for (size_t i = 0; i < numWords; ++i) {
        wordsVector[i].set(words[i].data(), words[i].length());
    }
    content::SpellcheckData::FromContext(this)->AdjustCustomWords(
        std::vector<base::StringPiece>(),
        wordsVector);
}

// ======== content::BrowserContext implementation =============

std::unique_ptr<content::ZoomLevelDelegate>
BrowserContextImpl::CreateZoomLevelDelegate(const base::FilePath& partition_path)
{
    return std::unique_ptr<content::ZoomLevelDelegate>();
}

base::FilePath BrowserContextImpl::GetPath() const
{
    DCHECK(!d_isDestroyed);
    return d_requestContextGetter->path();
}

bool BrowserContextImpl::IsOffTheRecord() const
{
    DCHECK(!d_isDestroyed);
    return d_isIncognito;
}

content::ResourceContext* BrowserContextImpl::GetResourceContext()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    if (!d_resourceContext.get()) {
        d_resourceContext.reset(
            new ResourceContextImpl(d_requestContextGetter.get()));
    }
    return d_resourceContext.get();
}

content::DownloadManagerDelegate*
BrowserContextImpl::GetDownloadManagerDelegate()
{
    return 0;
}

content::BrowserPluginGuestManager*
BrowserContextImpl::GetGuestManager()
{
    return 0;
}

storage::SpecialStoragePolicy* BrowserContextImpl::GetSpecialStoragePolicy()
{
    return 0;
}

content::PushMessagingService* BrowserContextImpl::GetPushMessagingService()
{
    return 0;
}

content::SSLHostStateDelegate* BrowserContextImpl::GetSSLHostStateDelegate()
{
    return 0;
}

bool BrowserContextImpl::AllowDictionaryDownloads()
{
    return false;
}

content::PermissionManager* BrowserContextImpl::GetPermissionManager()
{
    return 0;
}

content::BackgroundSyncController* BrowserContextImpl::GetBackgroundSyncController()
{
    return 0;
}

net::URLRequestContextGetter* BrowserContextImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors)
{
    requestContextGetter()->setProtocolHandlers(protocol_handlers,
                                                std::move(request_interceptors));
    return requestContextGetter();
}

net::URLRequestContextGetter* BrowserContextImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors)
{
    return nullptr;
}

net::URLRequestContextGetter* BrowserContextImpl::CreateMediaRequestContext()
{
	return requestContextGetter();
}

net::URLRequestContextGetter* BrowserContextImpl::CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory)
{
    return nullptr;
}

content::FontCollection* BrowserContextImpl::GetFontCollection()
{
	return FontCollectionImpl::GetCurrent();
}

}  // close namespace blpwtk2

