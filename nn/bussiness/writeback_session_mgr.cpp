#include "writeback_session_mgr.h"

#define SESSION__WAITE_TIME_US (1000*1000)

CWritebackSessionMgr * CWritebackSessionMgr::m_poCSessionMgr = NULL;
CWritebackSessionMgr::CWritebackSessionMgr()
{
}

CWritebackSessionMgr::~CWritebackSessionMgr()
{

}

CWritebackSessionMgr * CWritebackSessionMgr::CWritebackSessionMgr::Instance()
{
    if (0 == m_poCSessionMgr)
    {
        m_poCSessionMgr = new CWritebackSessionMgr;
    }
    return m_poCSessionMgr;
}

HRESULT CWritebackSessionMgr::Init(CConf * poConf, CTseLogger *poLog)
{
    if (0 == poConf || 0 == poLog)
    {
        return S_FAIL;
    }
    TUINT32            udwI = 0;
    // 1. log
    m_poLog = poLog;
    // 2. queue
    m_poMemQue = new CWritebackQueue;
    if (!m_poMemQue || (0 != m_poMemQue->Init(poConf->m_udwWritebackQueSize + 1)))
    {
        TSE_LOG_ERROR(m_poLog, ("CSessionMgr init mem queue fail"));
        return S_FAIL;
    }
    // 3. session num
    m_udwSessionNum = poConf->m_udwWritebackQueSize;
    // 4. session wrapprList
    m_apoSessionWrapprList = new CWritebackSession[poConf->m_udwWritebackQueSize];
    if (NULL == m_apoSessionWrapprList)
    {
        TSE_LOG_ERROR(m_poLog, ("new m_apoSessionWrapprList fail"));
        return S_FAIL;
    }
    for (udwI = 0; udwI < m_udwSessionNum; udwI++)
    {
        m_apoSessionWrapprList[udwI].Init();
        m_poMemQue->WaitTillPush(&m_apoSessionWrapprList[udwI]);
    }
    // 6. bready
    m_bReady = TRUE;
    TSE_LOG_INFO(m_poLog, ("CWritebackSessionMgr init succ"));
    return S_OK;
}

HRESULT CWritebackSessionMgr::Uninit()
{
    m_bReady = FALSE;
    m_poLog = NULL;
    m_udwSessionNum = 0;

    //3.sessionWrapprList
    if (NULL != m_apoSessionWrapprList)
        delete[]m_apoSessionWrapprList;

    //4. queue
    if (NULL != m_poMemQue)
        delete[] m_poMemQue;

    return S_OK;
}

HRESULT CWritebackSessionMgr::WaitTillSession(CWritebackSession **ppoSessionWrapper)
{
    if (FALSE == m_bReady)
    {
        *ppoSessionWrapper = 0;
        return S_FAIL;
    }
    if (0 != m_poMemQue->WaitTillPop(*ppoSessionWrapper))
    {
        return S_FAIL;
    }
    return S_OK;
}

HRESULT CWritebackSessionMgr::WaitTimeSession(CWritebackSession **ppoSessionWrapper)
{
    if (FALSE == m_bReady)
    {
        *ppoSessionWrapper = 0;
        return S_FAIL;
    }
    if (0 != m_poMemQue->WaitTimePop(*ppoSessionWrapper, SESSION__WAITE_TIME_US))
    {
        return S_FAIL;
    }
    return S_OK;
}

HRESULT CWritebackSessionMgr::ReleaseSesion(CWritebackSession *poSessionWrapper)
{
    if (0 == m_poMemQue || 0 == poSessionWrapper)
    {
        return S_FAIL;
    }

    poSessionWrapper->Reset();
    if (0 != m_poMemQue->WaitTillPush(poSessionWrapper))
    {
        return S_FAIL;
    }
    return S_OK;
}
