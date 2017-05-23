#include "pre_processing_mgr.h"
#include "unpack_util.h"


CPreProcessingMgr *CPreProcessingMgr::m_poPreProcessingMgr = NULL;

CPreProcessingMgr * CPreProcessingMgr::GetInstance()
{

    if (NULL == m_poPreProcessingMgr)
    {
        try
        {
            m_poPreProcessingMgr = new CPreProcessingMgr;
        }
        catch (const std::bad_alloc &memExp)
        {
            assert(m_poPreProcessingMgr);
        }
    }
    return m_poPreProcessingMgr;

}

TINT32 CPreProcessingMgr::Init(CTseLogger *poLog, CWorkQueue *poTaskQueue, CWritebackQueue *poDelayTaskQueue)
{
    m_poServLog = poLog;
    m_poTaskQueue = poTaskQueue;
    m_poDelayTaskQueue = poDelayTaskQueue;

    // 活跃玩家和联盟列表
    m_setActivePlayer.clear();
    m_setActiveAlliance.clear();



    if (0 != ActiveDataInit(DATA_FILE__ACTIVE_PLAYER, DATA_FILE__ACTIVE_ALLIANCE))
    {
        TSE_LOG_ERROR(m_poServLog, ("CPreProcessingMgr::Init, init player and alliance file failed!"));
        return -1;
    }

   
    return 0;
}

TINT32 CPreProcessingMgr::ActiveDataInit(const string &strActicvePlayerFile, const string &strActicveAllianceFile)
{
    TCHAR *pCur = NULL;
    TCHAR szLine[512];
    TINT64 ddwKey = 0;
    FILE *fp = NULL;

    // 加载活跃玩家的列表
    fp = fopen(strActicvePlayerFile.c_str(), "at+");
    if (NULL == fp)
    {
        return -1;
    }
    fclose(fp);
    fp = fopen(strActicvePlayerFile.c_str(), "rt+");
    if (NULL == fp)
    {
        return -1;
    }


    while (fgets(szLine, 512, fp))
    {
        pCur = strtok(szLine, "\t\r\n");
        ddwKey = strtoul(pCur, NULL, 10);

        if (0 == ddwKey)
        {
            continue;        
        }

        m_setActivePlayer.insert(ddwKey);
    }
    fclose(fp);



    // 加载活跃联盟的列表
    fp = fopen(strActicveAllianceFile.c_str(), "at+");
    if (NULL == fp)
    {
        return -2;
    }
    fclose(fp);
    fp = fopen(strActicvePlayerFile.c_str(), "rt+");
    if (NULL == fp)
    {
        return -1;
    }

    while (fgets(szLine, 512, fp))
    {
        pCur = strtok(szLine, "\t\r\n");
        ddwKey = strtoul(pCur, NULL, 10);

        if (0 == ddwKey)
        {
            continue;
        }

        m_setActiveAlliance.insert(ddwKey);
    }
    fclose(fp);

    return 0;

   
}


TINT32 CPreProcessingMgr::PreCreateCacheIndexFromList (vector<SRawDataInfo> &vecRawDataInfo)
{
    // todo::从工作队列的session池中获取session节点
    // CWorkSession *pWorkSession = NULL;
    set<TINT64>::iterator it;
    SRawDataInfo stRawDataInfo;
    stRawDataInfo.Reset();


    // 预充活跃玩家
    for (it = m_setActivePlayer.begin(); it != m_setActivePlayer.end(); ++it)
    {
        if (0 != GetQueryRequest(*it, stRawDataInfo))
        {
            continue;
        }
        vecRawDataInfo.push_back(stRawDataInfo);
    }


    // 预充活跃联盟
    for (it = m_setActivePlayer.begin(); it != m_setActivePlayer.end(); ++it)
    {
        if (0 != GetQueryRequest(-1 * (*it), stRawDataInfo))
        {
            continue;
        }
        vecRawDataInfo.push_back(stRawDataInfo);
    }

    return 0;
}

TINT32 CPreProcessingMgr::PreCreateCacheIndexFromOffset(vector<SRawDataInfo> &vecRawDataInfo)
{
    TINT32 dwRet = 0;

    SRawDataOffsetInfo stRawDataOffsetInfo;
    stRawDataOffsetInfo.Reset();
    if (0 != CFileMgr::GetInstance()->ReadRawDataOffsetFile(stRawDataOffsetInfo))
    {
        TSE_LOG_ERROR(m_poServLog, ("CPreProcessingMgr::PreCreateCacheIndexFromOffset, ReadRawDataOffsetFile failed!"));
        return -1;
    }


    dwRet = CFileMgr::GetInstance()->ReadRawDataFile(stRawDataOffsetInfo, vecRawDataInfo);
    if (0 != dwRet)
    {
        return -3;
    }

    return 0;
}




TINT32 CPreProcessingMgr::GetQueryRequest(TINT64 ddwKey, SRawDataInfo &stRawDataInfo)
{
    ostringstream ossReq;
    ossReq.str("");
    ossReq << "0\t";
    ossReq << ddwKey << "\t";
    ossReq << "0";
    stRawDataInfo.m_strRawData = ossReq.str().c_str();
    return 0;
}



TINT32 CPreProcessingMgr::PreProcessing()
{
    vector<SRawDataInfo> vecRawDataInfo;
    vecRawDataInfo.clear();
    CWorkSession *pstWorkSession = 0;
    //CWritebackSession *pstWritebackSession = 0;


    // 1. 活跃用户
    vecRawDataInfo.clear();
    if (0 != PreCreateCacheIndexFromList(vecRawDataInfo))
    {
        TSE_LOG_ERROR(m_poServLog, ("pre-processing active list failed"));
        return -3;
    }
    for (TUINT64 uddwIdx = 0; uddwIdx < vecRawDataInfo.size(); ++uddwIdx)
    {
        if (0 != CWorkSessionMgr::Instance()->WaitTillSession(&pstWorkSession))
        {
            TSE_LOG_ERROR(m_poServLog, ("Get WorkSession failed"));
            return -4;
        }
        TSE_LOG_INFO(m_poServLog, ("[FOR MEMORY] Get WorkSession OK, session[%p], empty_size=%u",
            pstWorkSession, CWorkSessionMgr::Instance()->GetEmptySize()));
        ConvertRawdataInfoToWorkTask(pstWorkSession, vecRawDataInfo[uddwIdx]);
        m_poTaskQueue->WaitTillPush(pstWorkSession);
    }
    // 判读工作队列的大小决定是否完成活跃列表的内存索引的重建
    sleep(1);
    while (!m_poTaskQueue->BeEmpty())
    {
        sleep(2);
    }

    // 2. 磁盘命令
    vecRawDataInfo.clear();
    if (PreCreateCacheIndexFromOffset(vecRawDataInfo))
    {
        TSE_LOG_ERROR(m_poServLog, ("pre-processing active offset failed"));
        return -3;
    }
    for (TUINT64 uddwIdx = 0; uddwIdx < vecRawDataInfo.size(); ++uddwIdx)
    {
        if (0 != CWorkSessionMgr::Instance()->WaitTillSession(&pstWorkSession))
        {
            TSE_LOG_ERROR(m_poServLog, ("Get WorkSession failed"));
            return -4;
        }
        TSE_LOG_INFO(m_poServLog, ("[FOR MEMORY] Get WorkSession OK, session[%p], empty_size=%u, seq=%lu",
            pstWorkSession, CWorkSessionMgr::Instance()->GetEmptySize(), vecRawDataInfo[uddwIdx].m_uddwFileRow));
        //if (0 != CWritebackSessionMgr::Instance()->WaitTillSession(&pstWritebackSession))
        //{
        //    CWorkSessionMgr::Instance()->ReleaseSesion(pstWorkSession);
        //    TSE_LOG_ERROR(m_poServLog, ("Get WritebackSession failed"));
        //    return -4;
        //}

        ConvertRawdataInfoToWorkTask(pstWorkSession, vecRawDataInfo[uddwIdx]);
        m_poTaskQueue->WaitTillPush(pstWorkSession);
        //ConvertRawdataInfoToWriteBack(pstWritebackSession, vecRawDataInfo[uddwIdx]);
        //m_poDelayTaskQueue->WaitTillPush(pstWritebackSession);
    }

    // 判读回写队列的大小决定是否完成磁盘命令的恢复
    sleep(1);
    while (!m_poTaskQueue->BeEmpty()
        /*&& 0 != m_poDelayTaskQueue->BeEmpty()*/)
    {
        sleep(2);
    }
    return 0;
}


TINT32 CPreProcessingMgr::ConvertRawdataInfoToWorkTask(CWorkSession *poWorkSession, SRawDataInfo &stRawDataInfo)
{

    TUINT64 uddwTimeStart = CTimeUtils::GetCurTimeUs();
    poWorkSession->Reset();
    poWorkSession->m_ucIsUsing = 1;
    memset((char*)&poWorkSession->m_stHandle, 0, sizeof(poWorkSession->m_stHandle));
    poWorkSession->m_udwSeq = stRawDataInfo.m_uddwFileRow;
    poWorkSession->m_udwClientSeq = 100;
    poWorkSession->m_udwServiceType = EN_SERVICE_TYPE_REPORT_CENTER_REQ;
    poWorkSession->m_uddwReqTimeUs = uddwTimeStart;
    memcpy(poWorkSession->m_szClientReqBuf, stRawDataInfo.m_strRawData.c_str(), stRawDataInfo.m_strRawData.length());
    poWorkSession->m_szClientReqBuf[stRawDataInfo.m_strRawData.length()] = 0;
    poWorkSession->m_udwClientReqBufLen = stRawDataInfo.m_strRawData.length();
    poWorkSession->m_udwFileIndex = stRawDataInfo.m_udwFileIndex;
    poWorkSession->m_uddwFileRow = stRawDataInfo.m_uddwFileRow;
    poWorkSession->m_bNeedRsp = FALSE;

    return 0;
}

TINT32 CPreProcessingMgr::ConvertRawdataInfoToWriteBack(CWritebackSession *poWritebackSession, SRawDataInfo &stRawDataInfo)
{
    poWritebackSession->Reset();
    poWritebackSession->m_udwClientSeq = 100;
    poWritebackSession->m_udwSeq = 100;

    ReportUserReq stReportUserReq;
    stReportUserReq.Reset();
    UnpackUtil::GetRequestParam(stRawDataInfo.m_strRawData, stReportUserReq);
    //poWritebackSession->m_ddwKey = stReportUserReq.m_ddwKey;
    poWritebackSession->m_ddwRid = 0;
    poWritebackSession->m_udwType = 0;
    poWritebackSession->m_udwFileIndex = stRawDataInfo.m_udwFileIndex;
    poWritebackSession->m_uddwFileRow = stRawDataInfo.m_uddwFileRow;

    return 0;
}