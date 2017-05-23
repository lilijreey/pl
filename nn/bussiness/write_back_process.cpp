#include <unistd.h>
#include <pthread.h>
#include "cache_mgr.h"
#include "write_back_process.h"
#include "aws_table_report_user_cache.h"

CWritebackProcess::CWritebackProcess()
{
    m_poConf = NULL;              //配置
    m_poLog = NULL;              //Log
    m_poWritebackQ = NULL;         //更新队列
}

CWritebackProcess::~CWritebackProcess()
{
}

TINT32 CWritebackProcess::Init(CConf *poConf, CTseLogger *poLog, CWritebackQueue *poWritebackQ)
{
    TINT32 dwRetCode = 0;

    if (0 == poConf || 0 == poLog || 0 == poWritebackQ)
    {
        return S_FAIL;
    }

    m_poConf = poConf;
    m_poLog = poLog;
    m_poWritebackQ = poWritebackQ;

    m_udwWritebackInterval = poConf->m_udwWritebackInterval;
    m_udwWritebackStepSize = poConf->m_udwWritebackSpeed;
    m_udwWritebackStepSizeFactor = poConf->m_udwWritebackSpeedFactor;

    m_udwWritebackQueInitSize = poConf->m_udwWritebackQueInitSize;
    m_udwWritebackQueMaxSize = poConf->m_udwWritebackQueMaxSize;
    m_udwWritebackDelay = poConf->m_udwWritebackDelay;

    m_udwMaxRetryTime = poConf->m_udwMaxRetryTime;

    m_objDelayUpt.Init(&m_objTaskQueue, m_poLog, 1);

    m_bPromoting = false;

    return dwRetCode;
}

TVOID * CWritebackProcess::Start(TVOID *pParam)
{
    if (NULL == pParam)
    {
        return NULL;
    }
    CWritebackProcess * poWritebackProcess = (CWritebackProcess *)pParam;
    poWritebackProcess->WorkRoutine();
    return NULL;
}

TINT32 CWritebackProcess::WorkRoutine()
{
    CWritebackSession *poSession = NULL;
    //TBOOL bFirstTime = FALSE;
    while (1)
    {
        if (S_OK == m_poWritebackQ->WaitTillPop(poSession))
        {
            if (m_poWritebackQ->Size()<m_udwWritebackQueInitSize)
            {
                usleep(m_udwWritebackDelay);
            }
            ProcessReq(poSession);
            CWritebackSessionMgr::Instance()->ReleaseSesion(poSession);
        }
    }
    return S_OK;
}

TINT32 CWritebackProcess::ProcessReq(CWritebackSession *poSession)
{
    TUINT32 udwWritbackType = poSession->m_udwType;
    SItemSeq szItemSeq[MAX_SEQ_NUM];
    TUINT32 udwItemSeqNum = 0;

    for (TUINT32 udwKeyIdx = 0; udwKeyIdx < poSession->m_vecKeys.size(); udwKeyIdx++)
    {
        m_ddwCurKey = poSession->m_vecKeys[udwKeyIdx];

        if (udwWritbackType == EN_WRITEBACK_TYPE_LRU)
        {
            m_udwCacheType = CACHE_TYPE_UNLIVE;
            CCacheMgr::Instance()->GetItemUpdated(m_udwCacheType, m_ddwCurKey, szItemSeq, &udwItemSeqNum);
        }
        else if (udwWritbackType == EN_WRITEBACK_TYPE_OVERFLOW)
        {
            m_udwCacheType = CACHE_TYPE_OVERFLOW;
            CCacheMgr::Instance()->GetItemUpdated(m_udwCacheType, m_ddwCurKey, szItemSeq, &udwItemSeqNum);
        }
        else if (udwWritbackType == EN_WRITEBACK_TYPE_REQ) //客户端请求触发
        {
            if (m_ddwCurKey > 0)
            {
                m_udwCacheType = CACHE_TYPE_PLAYER;
            }
            else if (m_ddwCurKey < 0)
            {
                m_udwCacheType = CACHE_TYPE_ALLIANCE;
            }
            else
            {
                m_udwCacheType = CACHE_TYPE_SERVER;
            }

            m_dwRetCode = CCacheMgr::Instance()->GetItemUpdated(CACHE_TYPE_UNLIVE, m_ddwCurKey, szItemSeq, &udwItemSeqNum);
            if (m_dwRetCode != 0)
            {
                CCacheMgr::Instance()->GetItemUpdated(m_udwCacheType, m_ddwCurKey, szItemSeq, &udwItemSeqNum);
            }
            else
            {
                m_udwCacheType = CACHE_TYPE_UNLIVE;
            }
        }

        if (udwItemSeqNum == 0)
        {
            ProcessRsp(poSession);
            continue;
        }
        SReportInfo* poReportUser;
        string sReqContent;

        m_objTask.Reset();
        sReqContent.clear();

        m_objTask.m_sOperatorName = "UpdateItem";
        m_objTask.m_sTableName = "ReportUserCache";
        m_objTask.m_strHashKey = "Uid";
        m_objTask.m_strRangeKey = "Rid_Seq";
        m_objTask.m_uddwRouteThrd = 10000;
        m_objTask.m_strRouteFld = "Uid";
        m_objTask.m_udwCacheMode = 0;
        m_objTask.m_udwIdxNo = 0;
        m_objTask.m_udwSeqNo = poSession->m_udwSeq;
        TbReport_user_cache atbReportUserCache;
        SReportInfo* poDstReportUser = NULL;


        for (TUINT32 udwIdx = 0; udwIdx < udwItemSeqNum; udwIdx++)
        {
            m_stProxy.Reset();
            atbReportUserCache.Reset();
            m_stProxy.Init(&atbReportUserCache);

            atbReportUserCache.Set_Uid(m_ddwCurKey);
            atbReportUserCache.Set_Rid_seq(szItemSeq[udwIdx].m_ddwSeq);

            poReportUser = (SReportInfo*)(szItemSeq[udwIdx].m_szItems);
            for (TUINT32 udwIdy = 0; udwIdy < szItemSeq->m_udwSize; udwIdy++)
            {
                if (poReportUser->m_nRid > 0)
                {
                    poDstReportUser = m_stProxy.GetReportByIndex(udwIdy);
                    *poDstReportUser = *poReportUser;
                    atbReportUserCache.SetFlag(m_stProxy.GetReportFieldByIndex(udwIdy));
                }
                poReportUser++;
            }

            atbReportUserCache.OnUpdateItemReq(sReqContent);

            m_objTask.m_sReqContent = sReqContent;
            m_dwRetCode = m_objDelayUpt.DelayUpdate(&m_objTask);
            TSE_LOG_DEBUG(m_poLog, ("Writeback: Send dynamodb requet:%s,retcode[%d],uid[%ld],type[%u], seq[%u],cseq[%u]", sReqContent.c_str(), m_dwRetCode,
                m_ddwCurKey, udwWritbackType,
                poSession->m_udwSeq, poSession->m_udwClientSeq));
            for (m_udwRetryTime = 0; m_udwRetryTime < m_udwMaxRetryTime && m_dwRetCode != 0; m_udwRetryTime++)
            {
                TSE_LOG_ERROR(m_poLog, ("Writeback fail with err[%d],retry[%u]!seq[%u],cseq[%u]", m_dwRetCode, m_udwRetryTime, poSession->m_udwSeq, poSession->m_udwClientSeq));
                m_dwRetCode = m_objDelayUpt.DelayUpdate(&m_objTask);
            }

            m_udwDynamodbReqTime++;
            if (m_udwDynamodbReqTime >= m_udwWritebackStepSize)
            {
                m_udwDynamodbReqTime = 0;
                sleep(m_udwWritebackInterval);
            }
        }
        //完成一个Uid下所有更新序列的回写后，再做删除处理
        ProcessRsp(poSession);
    }

    //完成一个Session内所有Uid的回写后，再写磁盘
    if (poSession->m_udwType == EN_WRITEBACK_TYPE_REQ)
    {
        m_stRawDataOffsetInfo.Reset();
        m_stRawDataOffsetInfo.m_udwFileIndex = poSession->m_udwFileIndex;
        m_stRawDataOffsetInfo.m_uddwFileRow = poSession->m_uddwFileRow;
        CFileMgr::GetInstance()->WriteRawDataOffsetFile(m_stRawDataOffsetInfo);
    }

    return 0;
}

TINT32 CWritebackProcess::ProcessRsp(CWritebackSession *poSession)
{
    switch (poSession->m_udwType)
    {
    case EN_WRITEBACK_TYPE_LRU:
        CCacheMgr::Instance()->DeleteItem(CACHE_TYPE_UNLIVE, m_ddwCurKey);
        break;
    case EN_WRITEBACK_TYPE_OVERFLOW:
        break;
    case EN_WRITEBACK_TYPE_REQ:
    {
        if (m_udwCacheType == CACHE_TYPE_UNLIVE)
        {
            CCacheMgr::Instance()->DeleteItem(CACHE_TYPE_UNLIVE, m_ddwCurKey);
        }
        break;
    }
    default:
        TSE_LOG_ERROR(m_poLog, ("Wrong writeback type[%u]!seq[%u],cseq[%u]", poSession->m_udwType, poSession->m_udwSeq, poSession->m_udwClientSeq));
        break;
    }
    TSE_LOG_DEBUG(m_poLog, ("Wirte back Success!seq[%u],cseq[%u]", poSession->m_udwSeq, poSession->m_udwClientSeq));

    return 0;
}

TVOID * CWritebackProcess::Promote(TVOID *pParam)
{
    CWritebackProcess * poWritebackProcess = (CWritebackProcess *)pParam;
    poWritebackProcess->m_bPromoting = true;
    while (poWritebackProcess->m_poWritebackQ->Size() > poWritebackProcess->m_udwWritebackQueMaxSize)
    {
        poWritebackProcess->m_udwWritebackStepSize += poWritebackProcess->m_udwWritebackStepSizeFactor;
        sleep(poWritebackProcess->m_udwWritebackInterval);
    }

    poWritebackProcess->m_bPromoting = false;
    return NULL;
}
