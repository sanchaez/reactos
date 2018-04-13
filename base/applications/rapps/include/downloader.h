/*
* PROJECT:     ReactOS Applications Manager
* LICENSE:     GPL-2.0+ (https://spdx.org/licenses/GPL-2.0+)
* FILE:        base/applications/rapps/downloader.cpp
* PURPOSE:     Download Logic
*              Copyright 2018 Alexander Shaposhikov (sanchaez@reactos.org)
*/

#pragma once

enum DownloadStatus
{
    DLSTATUS_WAITING = IDS_STATUS_WAITING,
    DLSTATUS_DOWNLOADING = IDS_STATUS_DOWNLOADING,
    DLSTATUS_DOWNLOAD_FINISHED = IDS_STATUS_DOWNLOADED,
    DLSTATUS_INSTALLING = IDS_STATUS_INSTALLING,
    DLSTATUS_INSTALLED = IDS_STATUS_INSTALLED,
    DLSTATUS_FINISHED = IDS_STATUS_FINISHED
};

ATL::CStringW LoadStatusString(DownloadStatus StatusParam)
{
    ATL::CStringW szString;
    szString.LoadStringW(StatusParam);
    return szString;
}

// Download info struct
struct DownloadInfo
{
    DownloadInfo() {}

    // implicitly convertable from CAvailableApplicationInfo
    DownloadInfo(const CAvailableApplicationInfo& AppInfo) :
        szUrl(AppInfo.m_szUrlDownload),
        szName(AppInfo.m_szName),
        szSHA1(AppInfo.m_szSHA1)
    {
    }

    ATL::CStringW szUrl;
    ATL::CStringW szName;
    ATL::CStringW szSHA1;
};

// Download callback interface
// TODO: document
template <typename CCallbackImpl>
struct IDownloadCallbacks
{
    VOID Progress(_In_ const ULONG Progress,
                    _In_ const ULONG ProgressTotal)
    {
        static_cast<CCallbackImpl*>(this)->OnProgress(Progress,
                                                      ProgressTotal,
                                                      szStatus);
    }

    VOID SetStatus(_In_ const DownloadStatus Status)
    {
        static_cast<CCallbackImpl*>(this)->OnSetStatus(Status);
    }

    VOID Failure(_In_ HRESULT ErrorCode)
    {
        static_cast<CCallbackImpl*>(this)->OnFailure(ErrorCode);
    }

    VOID ProgressReset()
    {
        static_cast<CCallbackImpl*>(this)->OnProgressReset();
    }
};

/// Downloader handles downloads using "strategy" and "protocol"...
/// TODO: document

template <typename CStrategyImpl, typename CProtocolImpl, typename CDownloadCallbacksImpl>
class CDownloader : protected CStrategyImpl, protected CProtocolImpl
{
    CDownloadCallbacksImpl* m_pCallbacks;

    /* wrappers */

    VOID OnSetStatus(const DownloadStatus& Status)
    {
        if (m_pCallbacks)
            m_pCallbacks->OnSetStatus(Status);
    }

    VOID OnProgress(_In_ const ULONG Progress,
                    _In_ const ULONG ProgressTotal)
    {
        if (m_pCallbacks)
            m_pCallbacks->OnProgress(Progress, ProgressTotal);
    }

    VOID OnResetProgress()
    {
        if (m_pCallbacks)
            m_pCallbacks->OnResetProgress();
    }

    /* download stages */

    HRESULT InitStage(const DownloadInfo& Info)
    {
        OnSetStatus(DLSTATUS_WAITING);
        OnResetProgress();

        HRESULT result = CStrategyImpl::Init(Info);

        if(SUCCEEDED(result))
            result = CProtocolImpl::Init(Info);

        return result;
    }

    HRESULT PreDownloadStage()
    {
        OnSetStatus(DLSTATUS_DOWNLOADING);

        HRESULT result = CStrategyImpl::OnDownloadStarts();

        return result;
    }

    HRESULT DownloadStage()
    {
        OnSetStatus(DLSTATUS_DOWNLOADING);

        ULONG DownloadedBytes;
        do
        {
            OnProgress(CProtocolImpl::GetBytes(), CProtocolImpl::GetTotalBytes());
            result = CProtocolImpl::DownloadPart(DownloadedBytes);
        } while (DownloadedBytes && SUCCEEDED(result));

        if (FAILED(result))
        {
            Reset();
            return result;
        }

        return CProtocolImpl::FinalizeDownload();
    }

    HRESULT PostDownloadStage()
    {
        if (!CStrategyImpl::IsInstallable())
        {
            OnSetStatus(DLSTATUS_FINISHED);
            return S_OK;
        }
        OnSetStatus(DLSTATUS_INSTALLING);

        HRESULT result = CStrategyImpl::OnDownloadFinished();

        if (FAILED(result))
        {
            return result;
        }

        OnSetStatus(DLSTATUS_INSTALLED);
    }

    HRESULT FinishStage()
    {
        Reset();
        OnSetStatus(DLSTATUS_FINISHED);
        return S_OK;
    }

    VOID Reset()
    {
        CProtocolImpl::Reset();
        CStrategyImpl::Reset();
    }

    /* helpers */

    BOOL CheckFailure(HRESULT result)
    {
        if (FAILED(result))
        {
            if (m_pCallbacks)
                m_pCallbacks->OnFailure(result);

            Reset();
            return TRUE;
        }

        return FALSE;
    }

public:
    CDownloader(const CDownloadCallbacksImpl* pCallbacks = NULL)
        : m_pCallbacks(pNewCallbacks) {};

    // Set callbacks to use when downloading. 
    // Function doesn't take ownership of the pointer. 
    //VOID SetCallbacks(_In_ const IDownloadCallbacks* pNewCallbacks)
    //{
    //  m_pCallbacks = pNewCallbacks;
    //}

    // Download a file via a given info to a directory specified by
    // a provided download strategy.
    HRESULT DownloadFile(_In_ const DownloadInfo& Info)
    {
        HRESULT result;

        // init stage
        result = InitStage(Info);
        if (CheckFailure(result)
        {
            return result;
        }

        // pre-download stage
        result = PreDownloadStage();
        if (CheckFailure(result)
        {
            return result;
        }

        // download stage
        result = DownloadStage();
        if (CheckFailure(result)
        {
            return result;
        }

        // post-download stage
        result = PostDownloadStage(Info);
        if (CheckFailure(result))
        {
            return result;
        }

        // download finished!
        result = CStrategyImpl::OnDownloadFinished();
        CheckFailure(result);
        return result;
    }
};

/// Download strategy implementations allow flexibility when
/// some downloads need a special treatment.

// Base Strategy class
// TODO: document
template <typename CStrategyImpl>
class CStrategyBase
{
    friend class CStrategyImpl;
    CStrategyBase() : m_bIsValid(FALSE) {};

protected:
    BOOL m_bIsValid;

    /* fallbacks */

    HRESULT InitImpl(const DownloadInfo& Info)
    {
        (void)Info; // unused
        return S_OK;
    }

    HRESULT OnDownloadStartImpl()
    {
        return S_OK;
    }

    HRESULT OnDownloadFinishImpl()
    {
        return S_OK;
    }

    HRESULT OnInstallImpl()
    {
        return S_OK;
    }

    VOID ResetImpl() {}

public:
    BOOL IsValid() const
    {
        return m_bIsValid;
    }

    HRESULT Init(_In_ const DownloadInfo& Data)
    {
        IsInvalid = TRUE;
        return static_cast<CStrategyImpl*>(this)->InitImpl(Data);
    }

    HRESULT OnDownloadStart()
    {
        if (!m_bIsValid)
        {
            return E_UNEXPECTED;
        }
        return static_cast<CStrategyImpl*>(this)->OnDownloadStartImpl();
    }

    HRESULT OnDownloadFinish()
    {
        if (!m_bIsValid)
        {
            return E_UNEXPECTED;
        }
        return static_cast<CStrategyImpl*>(this)->OnDownloadFinishImpl();
    }

    HRESULT OnInstall()
    {
        if (!m_bIsValid)
        {
            return E_UNEXPECTED;
        }
        return static_cast<CStrategyImpl*>(this)->OnInstallImpl();
    }

    VOID Reset()
    {
        IsInvalid = FALSE;
        static_cast<CStrategyImpl*>(this)->ResetImpl();
    }

    ~CStrategyBase()
    {
        Reset(); 
    }
};

/// Download protocols are used to specify protocol used (FTP, HTTP ...)
/// They share a common interface for downloading bytes to abstract 
/// implementation details away.

// Base Protocol class
// TODO: document
template <typename CProtocolImpl>
class CProtocolBase
{
    friend class CProtocolImpl;
    CProtocolBase() : m_bIsValid(TRUE) {};

protected:
    ULONG TotalBytes;
    ULONG TotalDownloadedBytes;
    BOOL m_bIsValid;

    /* fallbacks */

    HRESULT InitImpl(_In_ const DownloadInfo& Info)
    {
        (void)Info; // unused
        return S_OK;
    }

    HRESULT DownloadPartImpl(_Out_ ULONG& ReadBytes)
    {
        ReadBytes = 0;
        return S_OK;
    }

    HRESULT FinalizeDownloadImpl()
    {
        return S_OK;
    }

    VOID ResetImpl() {}

public:
    BOOL IsValid() const
    {
        return m_bIsValid;
    }

    HRESULT Init(_In_ const DownloadInfo& Info)
    {
        TotalBytes = 0;
        TotalDownloadedBytes = 0;
        m_bIsValid = TRUE;

        return static_cast<CProtocolImpl*>(this)->InitImpl(Info);
    }

    HRESULT DownloadPart(_Out_ ULONG& ReadBytes)
    {
        if (!m_bIsValid)
        {
            return E_UNEXPECTED;
        }
        return static_cast<CProtocolImpl*>(this)->DownloadPartImpl(ReadBytes);
    }

    HRESULT FinalizeDownload()
    {
        if (!m_bIsValid)
        {
            return E_UNEXPECTED;
        }
        return static_cast<CProtocolImpl*>(this)->FinalizeDownloadImpl();
    }

    VOID Reset()
    {
        m_bIsValid = FALSE;
        static_cast<CProtocolImpl*>(this)->ResetImpl();
    }

    ULONG GetTotalBytes() const
    {
        return TotalBytes;
    }

    ULONG GetTotalDownloadedBytes() const
    {
        return TotalDownloadedBytes;
    }

    ~CProtocolBase()
    {
        Reset();
    }
};

// Strategies declaration

// A strategy for .cab downloads
class CCabStrategy : public CStrategyBase<CCabStrategy>
{
    friend class CStrategyBase<CCabStrategy>;
};

// A strategy for RAPPS downloads
class CFileStrategy : public CStrategyBase<CFileStrategy>
{
    friend class CStrategyBase<CFileStrategy>;
};

// Protocols declaration

// FTP downloads
class CProtocolFTP : public CProtocolBase<CProtocolFTP>
{
    friend class CProtocolBase<CProtocolFTP>;
};

// HTTP and HTTPS downloads
class CProtocolHTTP : public CProtocolBase<CProtocolHTTP>
{
    friend class CProtocolBase<CProtocolHTTP>;
};