#include <algorithm>
#include "work_process.h"
#include "cache_mgr.h"
#include "work_session_mgr.h"
#include "unpack_util.h"
#include "my_define.h"
#include "statistic.h"

TINT32 CWorkProcess::Init(CConf *poConf, CTseLogger *poServLog, CTseLogger *poReqLog, CWorkQueue *poUpdateQue, CWritebackQueue *poWritebackQ, ILongConn *poQueryLongConn, ILongConn *poSearchLongConn)
{
    if (0 == poConf || 0 == poServLog || 0 == poUpdateQue || 0 == poReqLog)
    {
        return S_FAIL;
    }

    m_poConf = poConf;
    m_poLog = poServLog;
    m_poReqLog = poReqLog;
    m_poWorkQueue = poUpdateQue;
    m_poWritebackQ = poWritebackQ;
    m_poQueryLongConn = poQueryLongConn;
    m_poSearchLongConn = poSearchLongConn;

    m_poUnpack = new CBaseProtocolUnpack;
    m_poUnpack->Init();
    m_poPack = new CBaseProtocolPack;
    m_poPack->Init();

    return 0;
}

TVOID * CWorkProcess::Start(TVOID *pParam)
{
    if (NULL == pParam)
    {
        return NULL;
    }
    CWorkProcess * poUpdateProcess = (CWorkProcess *)pParam;
    poUpdateProcess->WorkRoutine();
    return NULL;
}

TINT32 CWorkProcess::WorkRoutine()
{
    CWorkSession *poSession = NULL;
    while (1)
    {
        if (S_OK == m_poWorkQueue->WaitTillPop(poSession))
        {
            ProcessDispatch(poSession);
        }
    }
    return S_OK;
}

TINT32 CWorkProcess::ProcessDispatch(CWorkSession *poSession)
{
    TINT32 dwRetCode = 0;
    TUINT64 uddwTimeA = 0, uddwTimeB = 0;

    if (poSession->m_ucWorkType == EN_WORK_TYPE__REQUEST)
    {
        uddwTimeA = CTimeUtils::GetCurTimeUs();
        poSession->m_uddwInQueueCostTimeUs = uddwTimeA - poSession->m_uddwInQueueTimeBegUs;
        TSE_LOG_INFO(m_poLog, ("ProcessDispatch:request[%s],seq[%u],cseq[%u]", poSession->m_szClientReqBuf, poSession->m_udwSeq, poSession->m_udwClientSeq));
        //dwRetCode = UnpackUtil::GetRequestParam(string((const char*)poSession->m_szClientReqBuf), poSession->m_objReqInfo);
        dwRetCode = UnpackUtil::GetRequestParamInJson(string((const char*)poSession->m_szClientReqBuf), poSession->m_objReqInfo);
        if (0 != dwRetCode)
        {
            TSE_LOG_ERROR(m_poLog, ("ProcessDispatch:get request params failed with[%d],seq[%u],cseq[%u]", dwRetCode, poSession->m_udwSeq, poSession->m_udwClientSeq));
            poSession->m_dwRetCode = EN_RET_CODE__PARAM_ERROR;
            poSession->m_udwCommandStep = EN_COMMAND_STEP__END;
            goto LABEL_STEP_END;
        }
        uddwTimeB = CTimeUtils::GetCurTimeUs();
        poSession->m_uddwUnpackCostTimeUs = uddwTimeB - uddwTimeA;

        if (poSession->m_objReqInfo.m_ddwProcessCmd == EN_REPORT_USER_OP__REPORT_READ
            || poSession->m_objReqInfo.m_ddwProcessCmd == EN_REPORT_USER_OP__REPROT_DEL
            || poSession->m_objReqInfo.m_ddwProcessCmd == EN_REPORT_USER_OP__REPORT_SEND
            || poSession->m_objReqInfo.m_ddwProcessCmd == EN_REPORT_USER_OP__REPORT_MULTI_SEND)
        {
            if (poSession->m_bNeedRsp)//需要响应表示该请求为正式请求，预填充的请求不需要再次记录请求
            {
                dwRetCode = RequestWriteFile(poSession);
                if (dwRetCode != 0)
                {
                    TSE_LOG_ERROR(m_poLog, ("ProcessDispatch:write file failed with[%d],seq[%u],cseq[%u]", dwRetCode, poSession->m_udwSeq, poSession->m_udwClientSeq));
                    poSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
                    poSession->m_udwCommandStep = EN_COMMAND_STEP__END;
                    goto LABEL_STEP_END;
                }
            }
        }
        uddwTimeA = CTimeUtils::GetCurTimeUs();
        poSession->m_uddwWriteFileCostTimeUs = uddwTimeA - uddwTimeB;
    }
    else if (poSession->m_ucWorkType == EN_WORK_TYPE__RESPONSE)
    {
        TSE_LOG_DEBUG(m_poLog, ("ProcessDispatch:down node responded with[%d],seq[%u],cseq[%u]", poSession->m_dwRetCode, poSession->m_udwSeq, poSession->m_udwClientSeq));
        if (poSession->m_dwRetCode != EN_RET_CODE__SUCCESS)
        {
            poSession->m_udwCommandStep = EN_COMMAND_STEP__END;
            goto LABEL_STEP_END;
        }
    }
    else
    {
        TSE_LOG_ERROR(m_poLog, ("ProcessDispatch:invalid work type[%u],seq[%u],cseq[%u]", poSession->m_ucWorkType, poSession->m_udwSeq, poSession->m_udwClientSeq));
        poSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
        poSession->m_udwCommandStep = EN_COMMAND_STEP__END;
        goto LABEL_STEP_END;
    }

    switch (poSession->m_objReqInfo.m_ddwProcessCmd)
    {
    case EN_REPORT_USER_OP__REPORT_GET:
        ProcessCmd_ReportGet(poSession, poSession->m_bNeedRsp);
        break;
    case EN_REPORT_USER_OP__REPORT_DETAIL_GET:
        ProcessCmd_ReportDetailGet(poSession, poSession->m_bNeedRsp);
        break;
    case EN_REPORT_USER_OP__REPORT_READ:
        ProcessCmd_ReportRead(poSession, poSession->m_bNeedRsp);
        break;
    case EN_REPORT_USER_OP__REPROT_DEL:
        ProcessCmd_ReportDel(poSession, poSession->m_bNeedRsp);
        break;
    case EN_REPORT_USER_OP__ALLIANCE_REPORT_GET:
        ProcessCmd_AllianceReportGet(poSession, poSession->m_bNeedRsp);
        break;
    case EN_REPORT_USER_OP__REPORT_SEND:
        Processcmd_ReportPut(poSession, poSession->m_bNeedRsp);
        break;
    case EN_REPORT_USER_OP__REPORT_MULTI_SEND:
        Processcmd_ReportMultiPut(poSession, poSession->m_bNeedRsp);
        break;
    case EN_REPORT_USER_OP__REPORT_SCAN_FOR_DEBUG:
        ProcessCmd_ReportScanForDebug(poSession, poSession->m_bNeedRsp);
        break;
    default:
        TSE_LOG_ERROR(m_poLog, ("Invalid Process cmd[%u],seq[%u],cseq[%u]", poSession->m_objReqInfo.m_ddwProcessCmd, poSession->m_udwSeq, poSession->m_udwClientSeq));
        poSession->m_dwRetCode = EN_RET_CODE__PARAM_ERROR;
        poSession->m_udwCommandStep = EN_COMMAND_STEP__END;
        break;
    }

LABEL_STEP_END:
    if (poSession->m_udwCommandStep == EN_COMMAND_STEP__END)
    {
        uddwTimeA = CTimeUtils::GetCurTimeUs();
        if (poSession->m_objReqInfo.m_ddwProcessCmd == EN_REPORT_USER_OP__REPORT_READ
            || poSession->m_objReqInfo.m_ddwProcessCmd == EN_REPORT_USER_OP__REPROT_DEL
            || poSession->m_objReqInfo.m_ddwProcessCmd == EN_REPORT_USER_OP__REPORT_SEND
            || poSession->m_objReqInfo.m_ddwProcessCmd == EN_REPORT_USER_OP__REPORT_MULTI_SEND)
        {
            dwRetCode = RequestWriteback(poSession);
            if (dwRetCode != 0)
            {
                TSE_LOG_ERROR(m_poLog, ("ProcessDispatch:writeback failed with[%d],seq[%u],cseq[%u]", dwRetCode, poSession->m_udwSeq, poSession->m_udwClientSeq));
            }
        }
        uddwTimeB = CTimeUtils::GetCurTimeUs();
        poSession->m_uddwReqWritebackCostTimeUs = uddwTimeB - uddwTimeA;

        if (poSession->m_bNeedRsp)
        {
            ProcessResponse(poSession);
        }
        else
        {
            TSE_LOG_INFO(m_poLog, ("[wavetest---ReleaseSesion] session[%p], cmd[%u], step[%d], ret[%d], btime[%llu] ctime[%lld] seq[%u],cseq[%u]", \
                poSession, poSession->m_objReqInfo.m_ddwProcessCmd, poSession->m_udwCommandStep, poSession->m_dwRetCode, \
                poSession->m_uddwReqTimeUs, CTimeUtils::GetCurTimeUs() - poSession->m_uddwReqTimeUs,
                poSession->m_udwSeq, poSession->m_udwClientSeq));
            CWorkSessionMgr::Instance()->ReleaseSesion(poSession);
        }
    }

    return 0;
}

TINT32 CWorkProcess::ProcessResponse(CWorkSession *poSession)
{
    TUCHAR *pucPackage = NULL;
    TUINT32 udwPackageLen = 0;
    TUINT64 uddwTimeA, uddwTimeB;

    uddwTimeA = CTimeUtils::GetCurTimeUs();

    CBaseProtocolPack	*poPackTool = m_poPack  ;
    poPackTool->ResetContent();
    poPackTool->SetServiceType(EN_SERVICE_TYPE_REPORT_CENTER_RSP);
    poPackTool->SetSeq(poSession->m_udwClientSeq);
    poPackTool->SetKey(EN_GLOBAL_KEY__RES_CODE, poSession->m_dwRetCode);

    if (poSession->m_dwRetCode > 0)
    {
        //UnpackUtil::GetResponseContent(poSession->m_objRspInfo, poSession->m_szRspContent);
        UnpackUtil::GetResponseContentInJson(poSession->m_objReqInfo, poSession->m_objRspInfo, poSession->m_szRspContent);
        m_poPack->SetKey(EN_GLOBAL_KEY__RES_BUF, (unsigned char*)(poSession->m_szRspContent.c_str()), poSession->m_szRspContent.size());
    }
    m_poPack->GetPackage(&pucPackage, &udwPackageLen);

    poSession->m_uddwResTimeUs = CTimeUtils::GetCurTimeUs();
    if (poSession->m_dwRetCode == EN_RET_CODE__SUCCESS)
    {
        CStatistic::Instance()->AddSearchSucc(poSession->m_uddwResTimeUs - poSession->m_uddwReqTimeUs);
    }
    else
    {
        CStatistic::Instance()->AddSearchFail(poSession->m_uddwResTimeUs - poSession->m_uddwReqTimeUs);
    }
    poSession->m_uddwRspCostTimeUs = poSession->m_uddwResTimeUs - uddwTimeA;
    TSE_LOG_HOUR(m_poReqLog, ("ret=%d,req=%s,seq=%u,cseq=%u,rseq=%u,TotalCost=%lu"
        "[WaitCost=%lu,PushCost=%lu,InQueue=%lu,UnpackCost=%lu,FileCost=%lu,QryPlayer=%lu,QryAl=%lu,QrySvr=%lu,TaskCost=%lu,Writeback=%lu,RspCost=%lu]",
        poSession->m_dwRetCode, poSession->m_szClientReqBuf,
        poSession->m_udwSeq, poSession->m_udwClientSeq, poSession->m_udwReqSeq, poSession->m_uddwResTimeUs - poSession->m_uddwReqTimeUs,
        poSession->m_uddwWaitSessionCostTimeUs, poSession->m_uddwPushSessionCostTimeUs,poSession->m_uddwInQueueCostTimeUs, 
        poSession->m_uddwUnpackCostTimeUs, poSession->m_uddwWriteFileCostTimeUs, poSession->m_uddwQueryPlayerCostTimeUs,
        poSession->m_uddwQueryAlCostTimeUs, poSession->m_uddwQuerySvrCostTimeUs, poSession->m_uddwTaskProcessCostTimeUs,
        poSession->m_uddwReqWritebackCostTimeUs, poSession->m_uddwRspCostTimeUs));

    uddwTimeA = CTimeUtils::GetCurTimeUs();

    // 2. send back
    LTasksGroup        stTasks;
    stTasks.m_Tasks[0].SetConnSession(poSession->m_stHandle);
    stTasks.m_Tasks[0].SetSendData(pucPackage, udwPackageLen);
    stTasks.m_Tasks[0].SetNeedResponse(0);
    stTasks.SetValidTasks(1);
    if (!m_poQueryLongConn->SendData(&stTasks))
    {
        TSE_LOG_ERROR(m_poLog, ("[wavetest---SendRsp] send response failed, session[%p] seq[%u],cseq[%u]",\
            poSession, poSession->m_udwSeq,poSession->m_udwClientSeq));
        CWorkSessionMgr::Instance()->ReleaseSesion(poSession);
        return -1;
    }
    uddwTimeB = CTimeUtils::GetCurTimeUs();
    TSE_LOG_INFO(m_poLog, ("[wavetest---SendRsp] SendRsp ok, session[%p] retcode:%d, seq:%u, clientseq:%u, packlen:%u, cost:%lu",
        poSession, poSession->m_dwRetCode, poSession->m_udwSeq, poSession->m_udwClientSeq, udwPackageLen,
        uddwTimeB - poSession->m_uddwReqTimeUs));

    // release session
    CWorkSessionMgr::Instance()->ReleaseSesion(poSession);

    return 0;
}

TINT32 CWorkProcess::RequestWriteFile(CWorkSession* poSession)
{
    TINT32 dwRetCode = 0;
    // 写磁盘
    SRawDataOffsetInfo stRawDataOffsetInfo;
    stRawDataOffsetInfo.Reset();
    dwRetCode = CFileMgr::GetInstance()->WriteRawDataFile((const char*)poSession->m_szClientReqBuf, stRawDataOffsetInfo);
    if (0 != dwRetCode)
    {
        TSE_LOG_ERROR(m_poLog, ("RequestWriteFile: WriteRawDataFile failed with [%d],seq=%u,cseq=%u",
            dwRetCode, poSession->m_udwSeq,poSession->m_udwClientSeq));
        dwRetCode = -1;
    }

    poSession->m_udwFileIndex = stRawDataOffsetInfo.m_udwFileIndex;
    poSession->m_uddwFileRow = stRawDataOffsetInfo.m_uddwFileRow;

    return dwRetCode;
}

TINT32 CWorkProcess::RequestWriteback(CWorkSession *poSession)
{
    // 获取回写队列的处理节点
    CWritebackSession *pstWritebackSession = 0;
    if (0 != CWritebackSessionMgr::Instance()->WaitTillSession(&pstWritebackSession))
    {
        TSE_LOG_ERROR(m_poLog, ("RequestWriteback:Get WritebackSession failed, seq=%u, cseq=%u", poSession->m_udwSeq,poSession->m_udwClientSeq));
        return -1;
    }

    //copy data to write back session
    pstWritebackSession->Reset();
    pstWritebackSession->m_udwClientSeq = poSession->m_udwClientSeq;
    pstWritebackSession->m_udwSeq = poSession->m_udwSeq;
    if (poSession->m_objReqInfo.m_vecKeys.empty())
    {//非群发
        pstWritebackSession->m_vecKeys.push_back(poSession->m_objReqInfo.m_ddwKey);
    }
    else//群发
    {
        pstWritebackSession->m_vecKeys.insert(pstWritebackSession->m_vecKeys.begin(), 
            poSession->m_objReqInfo.m_vecKeys.begin(), 
            poSession->m_objReqInfo.m_vecKeys.end());
    }
    pstWritebackSession->m_ddwRid = 0;
    pstWritebackSession->m_udwType = EN_WRITEBACK_TYPE_REQ;
    pstWritebackSession->m_udwFileIndex = poSession->m_udwFileIndex;
    pstWritebackSession->m_uddwFileRow = poSession->m_uddwFileRow;

    // push write back session to write back queue
    m_poWritebackQ->WaitTillPush(pstWritebackSession);

    return 0;
}

TINT32 CWorkProcess::Processcmd_ReportPut(CWorkSession *pstSession, TBOOL &bNeedResponse)
{
    TINT32 dwRetCode = 0;
    //TUINT64 uddwATime, uddwBTime;

    TUINT32 udwCacheType;
    SReportInfo stReportUser;
    stReportUser.Set(pstSession->m_objReqInfo.m_ddwRid,
                     pstSession->m_objReqInfo.m_ucReportType,
                     pstSession->m_objReqInfo.m_ucGetType,
                     pstSession->m_objReqInfo.m_ucStatus);

    // 0. 检查参数
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__INIT)
    {
        pstSession->m_uddwQueryTimeBegUs = CTimeUtils::GetCurTimeUs();
        if (pstSession->m_objReqInfo.m_ddwKey > 0)
        {
            udwCacheType = CACHE_TYPE_PLAYER;
        }
        else if (pstSession->m_objReqInfo.m_ddwKey < 0)
        {
            udwCacheType = CACHE_TYPE_ALLIANCE;
        }
        else
        {
            TSE_LOG_ERROR(m_poLog, ("Processcmd_ReportPut:invalid Uid, seq=%u, cseq=%u", pstSession->m_udwSeq, pstSession->m_udwClientSeq));
            pstSession->m_dwRetCode = EN_RET_CODE__PARAM_ERROR;
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
        }

        pstSession->m_udwCacheType = udwCacheType;
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__1;
    }

    // 1. 检查Uid对应的数据是否存在缓存当中
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__1)
    {
        //一定要先检查非活跃缓存，然后再检查活跃缓存，顺序不能颠倒
        dwRetCode = CCacheMgr::Instance()->Touch(CACHE_TYPE_UNLIVE, pstSession->m_objReqInfo.m_ddwKey, FALSE);
        if (dwRetCode == 0)
        {
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__3;
            pstSession->m_setDeref.insert(CACHE_TYPE_UNLIVE);
        }
        else
        {
            dwRetCode = CCacheMgr::Instance()->Touch(udwCacheType, pstSession->m_objReqInfo.m_ddwKey, FALSE);
            if (dwRetCode == 0)
            {
                pstSession->m_udwCommandStep = EN_COMMAND_STEP__3;
                pstSession->m_setDeref.insert(udwCacheType);
            }
            else
            {
                QueryLatestReportId(pstSession);
                // send request
                pstSession->m_udwCommandStep = EN_COMMAND_STEP__2;
                pstSession->m_udwExpectProcedure = EN_EXPECT_PROCEDURE__AWS;
                dwRetCode = SendAwsRequest(pstSession, EN_SERVICE_TYPE_QUERY_DYNAMODB_REQ);
                if (dwRetCode < 0)
                {
                    pstSession->m_dwRetCode = EN_RET_CODE__SEND_FAIL;
                    pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
                    TSE_LOG_ERROR(m_poLog, ("Processcmd_ReportPut: send latest report get request failed with [%d], [seq=%u] [cseq=%u]",
                        dwRetCode, pstSession->m_udwSeq,pstSession->m_udwClientSeq));
                    return -1;
                }
                return 0;
            }
        }
    }

    // 2. 获取并解析从Dynamodb返回的结果，并回写至缓存当中
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__2)
    {
        SItemSeq objItemSeq;
        objItemSeq.Reset();
        TUINT32 udwItemSeqNum = 0;
        // parse data
        TbReport_user_cache atbReportUserCache;
        atbReportUserCache.Reset();
        dwRetCode = CAwsResponse::OnQueryRsp(*pstSession->m_vecAwsRsp[0],&atbReportUserCache, sizeof(TbReport_user_cache), 1);
        if (dwRetCode > 0)
        {
            objItemSeq.m_ddwSeq = atbReportUserCache.Get_Rid_seq();
            objItemSeq.m_udwSize = AwsTable2Cache(&atbReportUserCache, &objItemSeq, TRUE);
            udwItemSeqNum = 1;
        }

        // put the data into cache
        dwRetCode = CCacheMgr::Instance()->CreateCache(CACHE_TYPE_UNLIVE, pstSession->m_objReqInfo.m_ddwKey, &objItemSeq, udwItemSeqNum);
        if (dwRetCode < 0)
        {
            pstSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
            TSE_LOG_ERROR(m_poLog, ("Processcmd_ReportPut:create cache fail with errcode[%d]!seq[%u],cseq[%u]", dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
            return -2;
        }
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__3;
    }

    // 3. 把新数据写入缓存,并解引用如有必要
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__3)
    {
        TINT64 ddwKey = pstSession->m_objReqInfo.m_ddwKey;
        TUINT64 uddwTimeA = CTimeUtils::GetCurTimeUs();
        if (ddwKey > 0)
        {
            pstSession->m_uddwQueryPlayerCostTimeUs = (uddwTimeA - pstSession->m_uddwQueryTimeBegUs);
        }
        else if (ddwKey < 0)
        {
            pstSession->m_uddwQueryAlCostTimeUs = (uddwTimeA - pstSession->m_uddwQueryTimeBegUs);
        }
        else
        {
            pstSession->m_uddwQuerySvrCostTimeUs = (uddwTimeA - pstSession->m_uddwQueryTimeBegUs);
        }
        dwRetCode = CCacheMgr::Instance()->AppendItem(pstSession->m_udwCacheType, CACHE_TYPE_UNLIVE, pstSession->m_objReqInfo.m_ddwKey, &stReportUser);
        if (dwRetCode < 0)
        {
            pstSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
            TSE_LOG_ERROR(m_poLog, ("Processcmd_ReportPut:Append fail with errcode[%d]!seq[%u],cseq[%u]", dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
        }

        for (set<TUINT32>::iterator it = pstSession->m_setDeref.begin(); it != pstSession->m_setDeref.end(); it++)
        {
            CCacheMgr::Instance()->DereferenceItem(*it, pstSession->m_objReqInfo.m_ddwKey);
        }

        pstSession->m_uddwTaskProcessCostTimeUs = (CTimeUtils::GetCurTimeUs() - uddwTimeA);
    }
    pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
    return dwRetCode;
}

TINT32 CWorkProcess::Processcmd_ReportMultiPut(CWorkSession *pstSession, TBOOL &bNeedResponse)
{
    TINT32 dwRetCode = 0;
    //TUINT64 uddwATime, uddwBTime;

    TUINT32 udwCacheType;
    TINT64 ddwKey;
    SReportInfo stReportUser;

    for (; pstSession->m_udwKeyIndex < pstSession->m_objReqInfo.m_vecKeys.size(); pstSession->m_udwKeyIndex++)
    {
        ddwKey = pstSession->m_objReqInfo.m_vecKeys[pstSession->m_udwKeyIndex];
        stReportUser.Reset();
        stReportUser.Set(pstSession->m_objReqInfo.m_vecReports[pstSession->m_udwKeyIndex].m_nRid,
            pstSession->m_objReqInfo.m_vecReports[pstSession->m_udwKeyIndex].m_nReport_type,
            pstSession->m_objReqInfo.m_vecReports[pstSession->m_udwKeyIndex].m_nType,
            pstSession->m_objReqInfo.m_vecReports[pstSession->m_udwKeyIndex].m_nStatus);
        // 0. 检查参数
        if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__INIT)
        {
            pstSession->m_uddwQueryTimeBegUs = CTimeUtils::GetCurTimeUs();

            if (ddwKey > 0)
            {
                udwCacheType = CACHE_TYPE_PLAYER;
            }
            else if (ddwKey < 0)
            {
                udwCacheType = CACHE_TYPE_ALLIANCE;
            }
            else
            {
                TSE_LOG_ERROR(m_poLog, ("Processcmd_ReportMultiPut:invalid Uid, seq=%u, cseq=%u", pstSession->m_udwSeq, pstSession->m_udwClientSeq));
                pstSession->m_udwCommandStep = EN_COMMAND_STEP__INIT;
            }

            pstSession->m_setDeref.clear();
            pstSession->m_udwCacheType = udwCacheType;
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__1;
        }

        // 1. 检查Uid对应的数据是否存在缓存当中
        if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__1)
        {
            //一定要先检查非活跃缓存，然后再检查活跃缓存，顺序不能颠倒
            dwRetCode = CCacheMgr::Instance()->Touch(CACHE_TYPE_UNLIVE, ddwKey, FALSE);
            if (dwRetCode == 0)
            {
                pstSession->m_udwCommandStep = EN_COMMAND_STEP__3;
                pstSession->m_setDeref.insert(CACHE_TYPE_UNLIVE);
            }
            else
            {
                dwRetCode = CCacheMgr::Instance()->Touch(udwCacheType, ddwKey, FALSE);
                if (dwRetCode == 0)
                {
                    pstSession->m_udwCommandStep = EN_COMMAND_STEP__3;
                    pstSession->m_setDeref.insert(udwCacheType);
                }
                else
                {
                    QueryLatestReportId(pstSession);
                    // send request
                    pstSession->m_udwCommandStep = EN_COMMAND_STEP__2;
                    pstSession->m_udwExpectProcedure = EN_EXPECT_PROCEDURE__AWS;
                    dwRetCode = SendAwsRequest(pstSession, EN_SERVICE_TYPE_QUERY_DYNAMODB_REQ);
                    if (dwRetCode < 0)
                    {
                        pstSession->m_udwErrCnt++;
                        pstSession->m_udwCommandStep = EN_COMMAND_STEP__INIT;
                        TSE_LOG_ERROR(m_poLog, ("Processcmd_ReportMultiPut: send latest report get request failed with [%d], [seq=%u] [cseq=%u]",
                            dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
                        continue;
                    }
                    break;
                }
            }
        }

        // 2. 获取并解析从Dynamodb返回的结果，并回写至缓存当中
        if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__2)
        {
            SItemSeq objItemSeq;
            objItemSeq.Reset();
            TUINT32 udwItemSeqNum = 0;
            // parse data
            TbReport_user_cache atbReportUserCache;
            atbReportUserCache.Reset();
            dwRetCode = CAwsResponse::OnQueryRsp(*pstSession->m_vecAwsRsp[0], &atbReportUserCache, sizeof(TbReport_user_cache), 1);
            if (dwRetCode > 0)
            {
                objItemSeq.m_ddwSeq = atbReportUserCache.Get_Rid_seq();
                objItemSeq.m_udwSize = AwsTable2Cache(&atbReportUserCache, &objItemSeq, TRUE);
                udwItemSeqNum = 1;
            }

            // put the data into cache
            dwRetCode = CCacheMgr::Instance()->CreateCache(CACHE_TYPE_UNLIVE, ddwKey, &objItemSeq, udwItemSeqNum);
            if (dwRetCode < 0)
            {
                pstSession->m_udwErrCnt++;
                pstSession->m_udwCommandStep = EN_COMMAND_STEP__INIT;
                TSE_LOG_ERROR(m_poLog, ("Processcmd_ReportMultiPut:create cache fail with errcode[%d]!seq[%u],cseq[%u]", dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
                continue;
            }
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__3;
        }

        // 3. 把新数据写入缓存,并解引用如有必要
        if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__3)
        {
            TUINT64 uddwTimeA = CTimeUtils::GetCurTimeUs();
            if (ddwKey > 0)
            {
                pstSession->m_uddwQueryPlayerCostTimeUs += (uddwTimeA - pstSession->m_uddwQueryTimeBegUs);
            }
            else if (ddwKey < 0)
            {
                pstSession->m_uddwQueryAlCostTimeUs += (uddwTimeA - pstSession->m_uddwQueryTimeBegUs);
            }
            else
            {
                pstSession->m_uddwQuerySvrCostTimeUs += (uddwTimeA - pstSession->m_uddwQueryTimeBegUs);
            }
            dwRetCode = CCacheMgr::Instance()->AppendItem(pstSession->m_udwCacheType, CACHE_TYPE_UNLIVE, ddwKey, &stReportUser);
            if (dwRetCode < 0)
            {
                pstSession->m_udwErrCnt++;
                pstSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
                TSE_LOG_ERROR(m_poLog, ("Processcmd_ReportMultiPut:Append fail with errcode[%d]!seq[%u],cseq[%u]", dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
            }

            for (set<TUINT32>::iterator it = pstSession->m_setDeref.begin(); it != pstSession->m_setDeref.end(); it++)
            {
                CCacheMgr::Instance()->DereferenceItem(*it, ddwKey);
            }

            pstSession->m_udwCommandStep = EN_COMMAND_STEP__INIT;
            pstSession->m_uddwTaskProcessCostTimeUs += (CTimeUtils::GetCurTimeUs() - uddwTimeA);
        }
    }

    if (pstSession->m_udwKeyIndex == pstSession->m_objReqInfo.m_vecKeys.size())
    {
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
        if (pstSession->m_udwErrCnt > 0)
        {
            pstSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
        }
        else
        {
            pstSession->m_dwRetCode = EN_RET_CODE__SUCCESS;
        }
    }

    return dwRetCode;
}

TINT32 CWorkProcess::ProcessCmd_ReportGet(CWorkSession * pstSession, TBOOL & bNeedResponse)
{
    TINT32 dwRetCode = 0;
    SItemExt szItemExt[MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ];
    TUINT32 udwItemNum = 0;

    // 1. 初始化并检查参数
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__INIT)
    {
        pstSession->m_udwCacheType = CACHE_TYPE_PLAYER;
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__1;
    }

    dwRetCode = QueryCache(pstSession, szItemExt, &udwItemNum);

    // 2. 获取查询结果，并进行业务处理
    if (dwRetCode == 0)
    {
        TUINT64 uddwTimeA = CTimeUtils::GetCurTimeUs();
        // 组包
        pstSession->m_objRspInfo.m_udwReportTotalNum = 0;
        pstSession->m_objRspInfo.m_udwReportUnreadNum = 0;
        TINT64 ddwNewestReportId = 0;
        //聚类
        std::map<TINT64/* 显示类型 */, SReportEntry> mapReportEntries;
        SItem* poCurItem = NULL;
        for (TUINT32 udwIdx = 0; udwIdx < udwItemNum; ++udwIdx)
        {
            poCurItem = &(szItemExt[udwIdx].m_objItem);

            pstSession->m_objRspInfo.m_udwReportTotalNum++;

            if (!(poCurItem->m_nStatus & EN_STATUS_READ))
            {
                pstSession->m_objRspInfo.m_udwReportUnreadNum++; //未读
            }
            if (ddwNewestReportId < poCurItem->m_nRid)
            {
                ddwNewestReportId = poCurItem->m_nRid; //最大Rid
            }

            SReportEntry stNewReportEntry; //显示类别
            TINT64 ddwDisplayClass = GetReportDisplayClass(*poCurItem);
            stNewReportEntry.ddwRid = poCurItem->m_nRid;
            stNewReportEntry.dwUnread = (poCurItem->m_nStatus & EN_STATUS_READ) ? 0 : 1;
            stNewReportEntry.dwReportType = poCurItem->m_nReport_type;
            stNewReportEntry.ddwDisplayClass = ddwDisplayClass;
            stNewReportEntry.dwStatus = poCurItem->m_nStatus;
            stNewReportEntry.dwNum = 1;

            if (mapReportEntries.find(ddwDisplayClass) == mapReportEntries.end())
            {
                //pstSession->m_objRspInfo.m_udwReportTotalNum++;
                mapReportEntries[ddwDisplayClass] = stNewReportEntry;
            }
            else
            {
                if (ReportEntryGreater(stNewReportEntry, mapReportEntries[ddwDisplayClass]) == TRUE)
                {
                    stNewReportEntry.dwUnread += mapReportEntries[ddwDisplayClass].dwUnread;
                    stNewReportEntry.dwNum += mapReportEntries[ddwDisplayClass].dwNum;
                    mapReportEntries[ddwDisplayClass] = stNewReportEntry;
                }
                else
                {
                    mapReportEntries[ddwDisplayClass].dwUnread += stNewReportEntry.dwUnread;
                    mapReportEntries[ddwDisplayClass].dwNum += stNewReportEntry.dwNum;
                }
            }
        }

        std::vector<SReportEntry> vecTopReportEntries;
        //转换为list,装备排序
        for (std::map<TINT64, SReportEntry>::iterator it = mapReportEntries.begin(); it != mapReportEntries.end(); ++it)
        {
            vecTopReportEntries.push_back(it->second);
        }
        TUINT32 udwTopReportEntries = vecTopReportEntries.size();
        std::sort(vecTopReportEntries.begin(), vecTopReportEntries.end(), ReportEntryGreater);

        TUINT32 udwCurPage = pstSession->m_objReqInfo.m_udwCurPage;
        TUINT32 udwBeginIndex = udwCurPage > 0 ? (udwCurPage - 1) * MAX_PERPAGE_NUM : 0;
        if (udwTopReportEntries == 0)//没有report
        {
            pstSession->m_objRspInfo.m_ddwNewestRid = 0;
        }
        else
        {
            pstSession->m_objRspInfo.m_ddwNewestRid = vecTopReportEntries[0].ddwRid;
        }

        pstSession->m_objRspInfo.m_udwReportEntryNum = 0;
        for (TUINT32 udwIdx = udwBeginIndex; udwIdx < udwTopReportEntries && udwIdx < udwBeginIndex + MAX_PERPAGE_NUM; ++udwIdx)
        {
            pstSession->m_objRspInfo.m_aReportToReturn[pstSession->m_objRspInfo.m_udwReportEntryNum++] = vecTopReportEntries[udwIdx];
        }
        //只返回下一页

        pstSession->m_dwRetCode = EN_RET_CODE__SUCCESS;
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;

        TSE_LOG_INFO(m_poLog, ("ProcessCmd_ReportGet:step[%u],total[%u],unread[%u],entry[%u],rid[%lld]!seq[%u],cseq[%u]", pstSession->m_udwCommandStep,
            pstSession->m_objRspInfo.m_udwReportTotalNum, pstSession->m_objRspInfo.m_udwReportUnreadNum, pstSession->m_objRspInfo.m_udwReportEntryNum, pstSession->m_objRspInfo.m_ddwNewestRid,
            pstSession->m_udwSeq, pstSession->m_udwClientSeq));
        TUINT64 uddwTimeB = CTimeUtils::GetCurTimeUs();

        pstSession->m_uddwTaskProcessCostTimeUs = uddwTimeB - uddwTimeA;
    }    

    return 0;
}

TINT32 CWorkProcess::ProcessCmd_ReportDetailGet(CWorkSession *pstSession, TBOOL &bNeedResponse)
{
    TINT32 dwRetCode = 0;
    SItemExt szItemExt[MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ];
    TUINT32 udwItemNum = 0;

    // 1. 初始化并检查参数
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__INIT)
    {
        pstSession->m_udwCacheType = CACHE_TYPE_PLAYER;
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__1;
    }

    dwRetCode = QueryCache(pstSession, szItemExt, &udwItemNum);

    // 2. 获取查询结果，并进行业务处理
    if (dwRetCode == 0)
    {
        TUINT64 uddwTimeA = CTimeUtils::GetCurTimeUs();
        //组包
        std::vector<SReportEntry> vecDetailReportEntries;
        pstSession->m_objRspInfo.m_udwReportTotalNum = 0;
        pstSession->m_objRspInfo.m_udwReportUnreadNum = 0;
        SItem* poCurItem = NULL;
        for (TUINT32 udwIdx = 0; udwIdx < udwItemNum; ++udwIdx)
        {
            poCurItem = &(szItemExt[udwIdx].m_objItem);

            if (poCurItem->m_nReport_type != pstSession->m_objReqInfo.m_ucReportType)
            {
                continue;
            }

            pstSession->m_objRspInfo.m_udwReportTotalNum++;
            if (!(poCurItem->m_nStatus & EN_STATUS_READ))
            {
                pstSession->m_objRspInfo.m_udwReportUnreadNum++;
            }

            SReportEntry stNewReportEntry;
            TINT64 ddwDisplayClass = GetReportDisplayClass(*poCurItem);
            stNewReportEntry.ddwRid = poCurItem->m_nRid;
            stNewReportEntry.dwUnread = (poCurItem->m_nStatus & EN_STATUS_READ) ? 0 : 1;
            stNewReportEntry.dwReportType = poCurItem->m_nReport_type;
            stNewReportEntry.ddwDisplayClass = ddwDisplayClass;
            stNewReportEntry.dwStatus = poCurItem->m_nStatus;
            stNewReportEntry.dwNum = 1;

            vecDetailReportEntries.push_back(stNewReportEntry);
        }

        TUINT32 udwTopReportEntries = vecDetailReportEntries.size();
        std::sort(vecDetailReportEntries.begin(), vecDetailReportEntries.end(), ReportEntryGreater);

        TUINT32 udwCurPage = pstSession->m_objReqInfo.m_udwCurPage;

        TUINT32 udwBeginIndex = udwCurPage > 0 ? (udwCurPage - 1) * MAX_PERPAGE_NUM : 0;
        if (udwTopReportEntries == 0)
        {
            pstSession->m_objRspInfo.m_ddwNewestRid = 0;
        }
        else
        {
            pstSession->m_objRspInfo.m_ddwNewestRid = vecDetailReportEntries[0].ddwRid;
        }

        pstSession->m_objRspInfo.m_udwReportEntryNum = 0;
        for (TUINT32 udwIdx = udwBeginIndex; udwIdx < udwTopReportEntries && udwIdx < udwBeginIndex + MAX_PERPAGE_NUM; ++udwIdx)
        {
            pstSession->m_objRspInfo.m_aReportToReturn[pstSession->m_objRspInfo.m_udwReportEntryNum++] = vecDetailReportEntries[udwIdx];
        }

        pstSession->m_dwRetCode = EN_RET_CODE__SUCCESS;
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;

        TSE_LOG_INFO(m_poLog, ("ProcessCmd_ReportDetailGet:step[%u],total[%u],unread[%u],entry[%u],rid[%lld]!seq[%u],cseq[%u]", pstSession->m_udwCommandStep,
            pstSession->m_objRspInfo.m_udwReportTotalNum, pstSession->m_objRspInfo.m_udwReportUnreadNum, pstSession->m_objRspInfo.m_udwReportEntryNum, pstSession->m_objRspInfo.m_ddwNewestRid,
            pstSession->m_udwSeq, pstSession->m_udwClientSeq));
        TUINT64 uddwTimeB = CTimeUtils::GetCurTimeUs();
        pstSession->m_uddwTaskProcessCostTimeUs = uddwTimeB - uddwTimeA;
    }
    
    
    return 0;
}

TINT32 CWorkProcess::ProcessCmd_ReportRead(CWorkSession *pstSession, TBOOL &bNeedResponse)
{
    return SetReportStatus(EN_STATUS_READ, pstSession, bNeedResponse);
}

TINT32 CWorkProcess::ProcessCmd_ReportDel(CWorkSession *pstSession, TBOOL &bNeedResponse)
{
    return SetReportStatus(EN_STATUS_DEL, pstSession, bNeedResponse);
}

TINT32 CWorkProcess::ProcessCmd_ReportScanForDebug(CWorkSession *pstSession, TBOOL &bNeedResponse)
{
    TINT32 dwRetCode = 0;
    SItemExt szItemExt[MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ];
    TUINT32 udwItemNum = 0;

    // 1. 初始化并检查参数
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__INIT)
    {
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__1;
        if (pstSession->m_objReqInfo.m_ddwKey > 0)
        {
            pstSession->m_udwCacheType = CACHE_TYPE_PLAYER;
        }
        else if (pstSession->m_objReqInfo.m_ddwKey < 0)
        {
            pstSession->m_udwCacheType = CACHE_TYPE_ALLIANCE;
        }
        else
        {
            TSE_LOG_ERROR(m_poLog, ("ProcessCmd_ReportScanForDebug:invalid key[%ld]!seq[%u],cseq[%u]", pstSession->m_objReqInfo.m_ddwKey,
                pstSession->m_udwSeq, pstSession->m_udwClientSeq));
            pstSession->m_dwRetCode = EN_RET_CODE__PARAM_ERROR;
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
            dwRetCode = -1;
        }
    }

    dwRetCode = QueryCache(pstSession, szItemExt, &udwItemNum, TRUE);

    // 2. 获取查询结果，并进行业务处理
    if (dwRetCode == 0)
    {
        TUINT64 uddwTimeA = CTimeUtils::GetCurTimeUs();
        TUINT32 udwDelCnt = 0, udwReadCnt = 0;
        SItem* poCurItem = NULL;
        for (TUINT32 udwIdx = 0; udwIdx < udwItemNum; udwIdx++)
        {
            poCurItem = &(szItemExt[udwIdx].m_objItem);
            TSE_LOG_INFO(m_poLog, ("ProcessCmd_ReportScanForDebug:curItem[rid=%ld,rtype=%u,gtype=%u,status=%u], Uid[%ld],type[%u]"
                "ridseq[%ld],pos[%u],seq[%u],cseq[%u]",
                poCurItem->m_nRid, poCurItem->m_nReport_type,
                poCurItem->m_nType, poCurItem->m_nStatus,
                pstSession->m_objReqInfo.m_ddwKey, pstSession->m_udwCacheType,
                szItemExt[udwIdx].m_ddwSeq,szItemExt[udwIdx].m_udwPos,pstSession->m_udwSeq,pstSession->m_udwClientSeq));

            if (poCurItem->m_nStatus&EN_STATUS_DEL)
            {
                udwDelCnt++;
                continue;
            }

            if (poCurItem->m_nStatus&EN_STATUS_READ)
            {
                udwReadCnt++;
            }
        }

        TSE_LOG_INFO(m_poLog, ("ProcessCmd_ReportScanForDebug:total[%u],del[%u],read[%u],seq[%u],cseq[%u]",
            udwItemNum, udwDelCnt, udwReadCnt,
            pstSession->m_udwSeq, pstSession->m_udwClientSeq));

        pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
        TUINT64 uddwTimeB = CTimeUtils::GetCurTimeUs();
        pstSession->m_uddwTaskProcessCostTimeUs = uddwTimeB - uddwTimeA;
    }

    return dwRetCode;
}

TINT32 CWorkProcess::ProcessCmd_AllianceReportGet(CWorkSession *pstSession, TBOOL &bNeedResponse)
{
    TINT32 dwRetCode = 0;
    SItemExt szItemExt[MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ];
    TUINT32 udwItemNum = 0;

    // 1. 初始化并检查参数
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__INIT)
    {
        pstSession->m_udwCacheType = CACHE_TYPE_ALLIANCE;
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__1;
    }

    dwRetCode = QueryCache(pstSession, szItemExt, &udwItemNum);

    // 2. 获取查询结果，并进行业务处理
    if (dwRetCode == 0)
    {
        TUINT64 uddwTimeA = CTimeUtils::GetCurTimeUs();
        TUINT32 udwCurIdNum = 0;
        std::vector<SReportEntry> vecDetailReportEntries;
        pstSession->m_objRspInfo.m_udwReportTotalNum = udwItemNum;
        if (udwItemNum == 0)
        {
            pstSession->m_objRspInfo.m_ddwNewestRid = 0;
        }
        else
        {
            std::sort(szItemExt, &szItemExt[udwItemNum], ReportUserExtGreater);
            pstSession->m_objRspInfo.m_ddwNewestRid = szItemExt[0].m_objItem.m_nRid;

            udwCurIdNum = GetCurPageReportIdList(szItemExt, udwItemNum, 
                                                 pstSession->m_objReqInfo.m_ucGetType, 
                                                 pstSession->m_objReqInfo.m_udwCurPage,
                                                 pstSession->m_objReqInfo.m_udwPerPage, 
                                                 vecDetailReportEntries,
                                                 pstSession->m_objRspInfo.m_udwReportTotalNum);
        }

        pstSession->m_objRspInfo.m_udwReportEntryNum = 0;
        if (udwCurIdNum != 0)
        {
            for (TUINT32 udwIdx = 0; udwIdx < udwCurIdNum; udwIdx++)
            {
                pstSession->m_objRspInfo.m_aReportToReturn[pstSession->m_objRspInfo.m_udwReportEntryNum++] = vecDetailReportEntries[udwIdx];
            }
        }

        pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
        pstSession->m_dwRetCode = EN_RET_CODE__SUCCESS;

        TSE_LOG_INFO(m_poLog, ("Processcmd_AllianceReportGet:step[%u],total[%u],unread[%u],entry[%u],rid[%lld]!seq[%u],cseq[%u]", pstSession->m_udwCommandStep,
            pstSession->m_objRspInfo.m_udwReportTotalNum, pstSession->m_objRspInfo.m_udwReportUnreadNum, pstSession->m_objRspInfo.m_udwReportEntryNum, pstSession->m_objRspInfo.m_ddwNewestRid,
            pstSession->m_udwSeq, pstSession->m_udwClientSeq));
        TUINT64 uddwTimeB = CTimeUtils::GetCurTimeUs();
        pstSession->m_uddwTaskProcessCostTimeUs = uddwTimeB - uddwTimeA;
    }

    return 0;
}

TUINT32 CWorkProcess::GetCurPageReportIdList(SItemExt*pstRawList, TUINT32 udwRawNum, TINT8 cType, TUINT32 udwPage, TUINT32 udwPerPage, std::vector<SReportEntry>& vecDetailReportEntries, TUINT32& udwTotalNum)
{
    TUINT32 udwRetNum = 0;
    TUINT32 udwItemNum = 0;
    TUINT32 udwBegIdx = 0;
    TUINT32 udwIdx = 0;

    if (udwRawNum == 0)
    {
        return 0;
    }
    udwBegIdx = udwPage > 0 ? (udwPage - 1) * udwPerPage : 0;
    if (udwBegIdx >= udwRawNum)
    {
        return 0;
    }

    SItem* poCurItem = NULL;
    switch (cType)
    {
    case EN_ALLIANCE_REPORT_GET_TYPE__ALL:
        for (udwIdx = udwBegIdx; udwIdx < udwRawNum && udwRetNum < udwPerPage; udwIdx++)
        {
            poCurItem = &(pstRawList[udwIdx].m_objItem);
            SReportEntry stNewReportEntry;
            TINT64 ddwDisplayClass = GetReportDisplayClass(*poCurItem);
            stNewReportEntry.ddwRid = poCurItem->m_nRid;
            stNewReportEntry.dwUnread = (poCurItem->m_nStatus & EN_STATUS_READ) ? 0 : 1;
            stNewReportEntry.dwReportType = poCurItem->m_nReport_type;
            stNewReportEntry.ddwDisplayClass = ddwDisplayClass;
            stNewReportEntry.dwStatus = poCurItem->m_nStatus;
            stNewReportEntry.dwNum = 1;

            vecDetailReportEntries.push_back(stNewReportEntry);
            udwRetNum++;
        }
        udwTotalNum = udwRawNum;
        break;
    case EN_ALLIANCE_REPORT_GET_TYPE__OUT:
    case EN_ALLIANCE_REPORT_GET_TYPE__IN:
        for (udwIdx = 0; udwIdx < udwRawNum; udwIdx++)
        {
            poCurItem = &(pstRawList[udwIdx].m_objItem);
            if (poCurItem->m_nType != cType)
            {
                continue;
            }

            udwItemNum++;
            if (udwBegIdx >= udwItemNum)
            {
                continue;
            }

            if (udwRetNum < udwPerPage)
            {
                SReportEntry stNewReportEntry;
                TINT64 ddwDisplayClass = GetReportDisplayClass(*poCurItem);
                stNewReportEntry.ddwRid = poCurItem->m_nRid;
                stNewReportEntry.dwUnread = (poCurItem->m_nStatus & EN_STATUS_READ) ? 0 : 1;
                stNewReportEntry.dwReportType = poCurItem->m_nReport_type;
                stNewReportEntry.ddwDisplayClass = ddwDisplayClass;
                stNewReportEntry.dwStatus = poCurItem->m_nStatus;
                stNewReportEntry.dwNum = 1;

                vecDetailReportEntries.push_back(stNewReportEntry);
                udwRetNum++;
            }
        }
        udwTotalNum = udwItemNum;
        break;
    }

    return udwRetNum;
}

bool CWorkProcess::TbReport_user_Compare_Reverse(const SReportInfo stA, const SReportInfo stB)
{
    if (stA.m_nRid > stB.m_nRid)
    {
        return true;
    }
    return false;
}

TINT32 CWorkProcess::QueryReportId(CWorkSession* pstSession)
{
    CompareDesc stCompareInfo;
    TbReport_user_cache stUserReportCache;
    stUserReportCache.Set_Uid(pstSession->m_objReqInfo.m_ddwKey);
    stUserReportCache.Set_Rid_seq(0);
    stCompareInfo.dwCompareType = COMPARE_TYPE_GE;

    AwsTable* pAwsTbl = &stUserReportCache;
    vector<AwsReqInfo*>& vecReq = pstSession->m_vecAwsReq;
    AwsReqInfo* pAwsReq = new AwsReqInfo(pAwsTbl, pAwsTbl->GetTableName(), "Query", ETbUSER_REPORT_CACHE_OPEN_TYPE_PRIMARY);
    pAwsTbl->OnQueryReq(pAwsReq->sReqContent, ETbUSER_REPORT_CACHE_OPEN_TYPE_PRIMARY, stCompareInfo,
        true, true, false, MAX_SEQ_NUM);
    vecReq.push_back(pAwsReq);

    return 0;
}

TINT32 CWorkProcess::QueryLatestReportId(CWorkSession* pstSession)
{
    CompareDesc stCompareInfo;
    TbReport_user_cache stUserReportCache;
    stUserReportCache.Set_Uid(pstSession->m_objReqInfo.m_ddwKey);
    stUserReportCache.Set_Rid_seq(0);
    stCompareInfo.dwCompareType = COMPARE_TYPE_GE;

    AwsTable* pAwsTbl = &stUserReportCache;
    vector<AwsReqInfo*>& vecReq = pstSession->m_vecAwsReq;
    AwsReqInfo* pAwsReq = new AwsReqInfo(pAwsTbl, pAwsTbl->GetTableName(), "Query", ETbUSER_REPORT_CACHE_OPEN_TYPE_PRIMARY);
    pAwsTbl->OnQueryReq(pAwsReq->sReqContent, ETbUSER_REPORT_CACHE_OPEN_TYPE_PRIMARY, stCompareInfo,
        true, true, false, 1);
    vecReq.push_back(pAwsReq);

    return 0;
}

TINT64 CWorkProcess::GetReportDisplayClass(const SReportInfo& tbReportUser)
{
    if (tbReportUser.m_nReport_type == EN_REPORT_TYPE_OCCUPY)
    {
        return -1 * tbReportUser.m_nReport_type;
    }
    else if (tbReportUser.m_nReport_type == EN_REPORT_TYPE_TRADE)
    {
        return -1 * tbReportUser.m_nReport_type;
    }
    else if (tbReportUser.m_nReport_type == EN_REPORT_TYPE_DRAGON_MONSTER)
    {
        return -1 * tbReportUser.m_nReport_type;
    }
    else if (tbReportUser.m_nReport_type == EN_REPORT_TYPE_TRANSPORT)
    {
        return -1 * tbReportUser.m_nReport_type;
    }
    return tbReportUser.m_nRid;
}

TBOOL CWorkProcess::ReportEntryGreater(const SReportEntry& stA, const SReportEntry stB)
{
    return stA.ddwRid > stB.ddwRid;
}

TBOOL CWorkProcess::ReportUserExtGreater(const SItemExt& stA, const SItemExt& stB)
{
    return stA.m_objItem.m_nRid > stB.m_objItem.m_nRid;
}

TINT32 CWorkProcess::QueryCache(CWorkSession* pstSession, SItemExt* szItemExt, TUINT32* pudwItemNum, TBOOL bIncludeDel)
{
    TINT32 dwRetCode = 0;

    // 1. 检查Uid对应的数据是否存在缓存当中
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__1)
    {
        pstSession->m_uddwQueryTimeBegUs = CTimeUtils::GetCurTimeUs();
        dwRetCode = CCacheMgr::Instance()->Touch(pstSession->m_udwCacheType, pstSession->m_objReqInfo.m_ddwKey, TRUE);
        if (dwRetCode == 0)
        {
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__3;
            pstSession->m_setDeref.insert(pstSession->m_udwCacheType);
        }
        else//没找到像Dynamodb查询
        {
            QueryReportId(pstSession);
            // send request
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__2;
            pstSession->m_udwExpectProcedure = EN_EXPECT_PROCEDURE__AWS;
            dwRetCode = SendAwsRequest(pstSession, EN_SERVICE_TYPE_QUERY_DYNAMODB_REQ);
            if (dwRetCode < 0)
            {
                pstSession->m_dwRetCode = EN_RET_CODE__SEND_FAIL;
                pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
                TSE_LOG_ERROR(m_poLog, ("QueryCache: send report get request failed with [%d], [seq=%u] [cseq=%u]",
                    dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
                dwRetCode = -1;
            }
            return 1;
        }
    }

    // 2. 获取并解析从Dynamodb返回的结果，并回写至缓存当中
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__2)
    {
        SItemSeq szItemSeq[MAX_SEQ_NUM];
        TUINT32 udwItemSeqNum = 0;
        // parse data
        TbReport_user_cache atbReportUserCache[MAX_SEQ_NUM];
        TUINT32 udwRecordNum = 0;
        for (TUINT32 udwIdx = 0; udwIdx < pstSession->m_vecAwsRsp.size(); udwIdx++)
        {
            dwRetCode = CAwsResponse::OnQueryRsp(*pstSession->m_vecAwsRsp[udwIdx], &atbReportUserCache[udwRecordNum], sizeof(TbReport_user_cache), MAX_SEQ_NUM - udwRecordNum);
            if (dwRetCode > 0)
            {
                udwRecordNum += dwRetCode;
            }
        }
        //由于查询Dynamodb时是逆序查询，因此在这里转为正序
        TBOOL bCheckNum = FALSE;
        for (TUINT32 udwIdx = udwRecordNum; udwIdx > 0; udwIdx--)
        {
            if (udwIdx == 1)//转换最新一条数据时，需要检查其有效数量，其他默认为最大值
            {
                bCheckNum = TRUE;
            }
            szItemSeq[udwItemSeqNum].m_ddwSeq = atbReportUserCache[udwIdx - 1].Get_Rid_seq();
            szItemSeq[udwItemSeqNum].m_udwSize = AwsTable2Cache(&atbReportUserCache[udwIdx - 1], &szItemSeq[udwItemSeqNum], bCheckNum);
            udwItemSeqNum++;
        }
        //修正脏数据，如果RidSeq不连续，则以最大的RidSeq为基准，重定其他RidSeq
        TINT64 ddwBaseSeq;
        if (udwItemSeqNum < 1)
        {
            ddwBaseSeq = 0;
        }
        else if (szItemSeq[udwItemSeqNum - 1].m_ddwSeq <= (udwItemSeqNum - 1))
        {
            ddwBaseSeq = 0;
        }
        else
        {
            ddwBaseSeq = szItemSeq[udwItemSeqNum - 1].m_ddwSeq - (udwItemSeqNum - 1);
        }
        for (TUINT32 udwIdx = 0; udwIdx < udwItemSeqNum; udwIdx++)
        {
            if (szItemSeq[udwIdx].m_ddwSeq != ddwBaseSeq)
            {
                TSE_LOG_ERROR(m_poLog, ("QueryCache:rid_seq restore from[%ld] to[%ld]!seq[%u],cseq[%u]", szItemSeq[udwIdx].m_ddwSeq, ddwBaseSeq,
                    pstSession->m_udwSeq, pstSession->m_udwClientSeq));
            }
            szItemSeq[udwIdx].m_ddwSeq = ddwBaseSeq;
            ddwBaseSeq++;
        }
        

        // put the data into cache
        dwRetCode = CCacheMgr::Instance()->CreateCache(pstSession->m_udwCacheType, pstSession->m_objReqInfo.m_ddwKey, szItemSeq, udwItemSeqNum);
        if (dwRetCode < 0)
        {
            pstSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
            TSE_LOG_ERROR(m_poLog, ("QueryCache:create cache fail with errcode[%d]!seq[%u],cseq[%u]", dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
            dwRetCode = -2;
        }
        else
        {
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__3;
        }
    }

    // 3. 把新数据写入缓存,并解引用如有必要
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__3)
    {
      //???
        dwRetCode = CCacheMgr::Instance()->Query(pstSession->m_udwCacheType, pstSession->m_objReqInfo.m_ddwKey, szItemExt, pudwItemNum, bIncludeDel);
        if (dwRetCode != 0)
        {
            pstSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
            pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;
            TSE_LOG_ERROR(m_poLog, ("QueryCache:Query fail with errcode[%d]!seq[%u],cseq[%u]", dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
            dwRetCode = -3;
        }

        for (set<TUINT32>::iterator it = pstSession->m_setDeref.begin(); it != pstSession->m_setDeref.end(); it++)
        {
            CCacheMgr::Instance()->DereferenceItem(*it, pstSession->m_objReqInfo.m_ddwKey);
        }

        TINT64 ddwKey = pstSession->m_objReqInfo.m_ddwKey;
        if (ddwKey > 0)
        {
            pstSession->m_uddwQueryPlayerCostTimeUs = CTimeUtils::GetCurTimeUs() - pstSession->m_uddwQueryTimeBegUs;
        }
        else if (ddwKey < 0)
        {
            pstSession->m_uddwQueryAlCostTimeUs = CTimeUtils::GetCurTimeUs() - pstSession->m_uddwQueryTimeBegUs;
        }
        else
        {
            pstSession->m_uddwQuerySvrCostTimeUs = CTimeUtils::GetCurTimeUs() - pstSession->m_uddwQueryTimeBegUs;
        }
    }

    return dwRetCode;
}

TINT32 CWorkProcess::SetReportStatus(TINT32 dwStatus, CWorkSession* pstSession, TBOOL& bNeedResponse, TBOOL bClear /*= FALSE*/)
{
    TINT32 dwRetCode = 0;
    SItemExt szItemExt[MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ];
    TUINT32 udwItemNum = 0;

    // 1. 初始化并检查参数
    if (pstSession->m_udwCommandStep == EN_COMMAND_STEP__INIT)
    {
        pstSession->m_udwCacheType = CACHE_TYPE_PLAYER;
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__1;
    }

    dwRetCode = QueryCache(pstSession, szItemExt, &udwItemNum);

    // 2. 获取查询结果，并进行业务处理
    if (dwRetCode == 0)
    {
        TUINT64 uddwTimeA = CTimeUtils::GetCurTimeUs();
        TUINT32 udwErrorCnt = 0;
        SItem* poCurItem = NULL;

        //组装返回数据包
        pstSession->m_objRspInfo.m_udwReportEntryNum = 0;
        pstSession->m_objRspInfo.m_udwReportTotalNum = 0;
        pstSession->m_objRspInfo.m_udwReportUnreadNum = 0;
        for (TUINT32 udwIdx = 0; udwIdx < udwItemNum; ++udwIdx)
        {
            poCurItem = &(szItemExt[udwIdx].m_objItem);
            pstSession->m_objRspInfo.m_udwReportTotalNum++;
            if (!(poCurItem->m_nStatus & EN_STATUS_READ))
            {
                pstSession->m_objRspInfo.m_udwReportUnreadNum++;
            }
        }
        TUINT32 udwUnreadNumChange = 0;

        pstSession->ResetAwsInfo();
        for (TUINT32 udwIdx = 0; udwIdx < pstSession->m_objReqInfo.m_vecOpType.size(); ++udwIdx)
        {
            if (pstSession->m_objReqInfo.m_vecOpType[udwIdx] == 0)//one report
            {
                for (TUINT32 udwIdy = 0; udwIdy < udwItemNum; ++udwIdy)
                {
                    poCurItem = &(szItemExt[udwIdy].m_objItem);
                    if (poCurItem->m_nRid != pstSession->m_objReqInfo.m_vecRid[udwIdx])
                    {
                        continue;
                    }

                    if (dwStatus == EN_STATUS_DEL
                        && (poCurItem->m_nStatus & EN_STATUS_MARK))
                    {
                        continue;//跳过已经的删除
                    }

                    TINT32 dwOriRead = poCurItem->m_nStatus & EN_STATUS_READ;

                    if (UpdateReportStatus(poCurItem, bClear, dwStatus) == TRUE)
                    {
                        dwRetCode = CCacheMgr::Instance()->WriteItem(pstSession->m_udwCacheType, pstSession->m_objReqInfo.m_ddwKey, &szItemExt[udwIdy]);
                        if (dwRetCode < 0)
                        {
                            udwErrorCnt++;
                            TSE_LOG_ERROR(m_poLog, ("SetReportStatus:set failed with[%d],errcnt[%u]!uid[%ld],rid[%ld],rtype[%u],gtype[%u],status[%u],seq[%u],cseq[%u]",
                                dwRetCode, udwErrorCnt,
                                pstSession->m_objReqInfo.m_ddwKey,poCurItem->m_nRid,poCurItem->m_nReport_type,poCurItem->m_nType,poCurItem->m_nStatus,
                                pstSession->m_udwSeq, pstSession->m_udwClientSeq));
                        }
                        
                    }

                    if (!dwOriRead && (dwStatus == EN_STATUS_READ || dwStatus == EN_STATUS_DEL) && (dwRetCode>=0))
                    {
                        udwUnreadNumChange++;
                    }

                    TSE_LOG_DEBUG(m_poLog, ("SetReportStatus:oriread[%d],status[%d],retcode[%d]!seq[%u],cseq[%u]",
                        dwOriRead,dwStatus,dwRetCode,pstSession->m_udwSeq,pstSession->m_udwClientSeq));
                }
            }
            if (pstSession->m_objReqInfo.m_vecOpType[udwIdx] == 1)//set
            {
                TINT32 dwIndex = -1;
                for (TUINT32 udwIdy = 0; udwIdy < udwItemNum; ++udwIdy) //找到
                {
                    poCurItem = &(szItemExt[udwIdy].m_objItem);
                    if (poCurItem->m_nRid == pstSession->m_objReqInfo.m_vecRid[udwIdx])
                    {
                        dwIndex = udwIdy;
                        break;
                    }
                }
                if (dwIndex != -1)
                {
                    TINT64 ddwDisplayClass = GetReportDisplayClass(szItemExt[dwIndex].m_objItem);
                    TUINT32 udwBegin = 0;
                    TUINT32 udwEnd = udwItemNum;
                    if (ddwDisplayClass > 0)//最多只可能存在1条Item，因此只需要循环1次，判断检索到的该Item就可以
                    {
                        udwBegin = dwIndex;
                        udwEnd = udwBegin + 1;
                    }
                    for (TUINT32 udwIdy = udwBegin; udwIdy < udwEnd; ++udwIdy)
                    {
                        poCurItem = &(szItemExt[udwIdy].m_objItem);

                        if (dwStatus == EN_STATUS_DEL
                            && (poCurItem->m_nStatus & EN_STATUS_MARK))
                        {
                            continue;
                        }

                        TINT64 ddwOtherClass = GetReportDisplayClass(szItemExt[udwIdy].m_objItem);
                        if (ddwOtherClass != ddwDisplayClass)
                        {
                            continue;
                        }

                        TINT32 dwOriRead = poCurItem->m_nStatus & EN_STATUS_READ;

                        if (UpdateReportStatus(poCurItem, bClear, dwStatus) == TRUE)
                        {
                            dwRetCode = CCacheMgr::Instance()->WriteItem(pstSession->m_udwCacheType, pstSession->m_objReqInfo.m_ddwKey, &szItemExt[udwIdy]);
                            if (dwRetCode < 0)
                            {
                                udwErrorCnt++;
                                TSE_LOG_ERROR(m_poLog, ("SetReportStatus:set failed with[%d],errcnt[%u]!uid[%ld],rid[%ld],rtype[%u],gtype[%u],status[%u],seq[%u],cseq[%u]",
                                    dwRetCode, udwErrorCnt,
                                    pstSession->m_objReqInfo.m_ddwKey, poCurItem->m_nRid, poCurItem->m_nReport_type, poCurItem->m_nType, poCurItem->m_nStatus,
                                    pstSession->m_udwSeq, pstSession->m_udwClientSeq));
                            }
                        }

                        if (!dwOriRead && (dwStatus == EN_STATUS_READ || dwStatus == EN_STATUS_DEL) && (dwRetCode >= 0))
                        {
                            udwUnreadNumChange++;
                        }
                        TSE_LOG_DEBUG(m_poLog, ("SetReportStatus:oriread[%d],status[%d],retcode[%d]!seq[%u],cseq[%u]",
                            dwOriRead, dwStatus, dwRetCode, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
                    }
                }
            }
        }

        if (udwUnreadNumChange > pstSession->m_objRspInfo.m_udwReportUnreadNum)
        {
            pstSession->m_objRspInfo.m_udwReportUnreadNum = 0;
        }
        else
        {
            pstSession->m_objRspInfo.m_udwReportUnreadNum -= udwUnreadNumChange;
        }

        if (udwErrorCnt > 0)
        {
            pstSession->m_dwRetCode = EN_RET_CODE__PROCESS_ERROR;
        }
        else
        {
            pstSession->m_dwRetCode = EN_RET_CODE__SUCCESS;
        }
        pstSession->m_udwCommandStep = EN_COMMAND_STEP__END;

        TUINT64 uddwTimeB = CTimeUtils::GetCurTimeUs();
        pstSession->m_uddwTaskProcessCostTimeUs = uddwTimeB - uddwTimeA;
    }

    return 0;
}

TBOOL CWorkProcess::UpdateReportStatus(SReportInfo* ptbReportUser, TBOOL bClear, TINT32 dwStatus)
{
    if (!bClear)
    {
        if (ptbReportUser->m_nStatus&dwStatus)
        {
            return FALSE;
        }
        else
        {
            ptbReportUser->m_nStatus |= dwStatus;
            return TRUE;
        }
    }
    if (bClear)
    {
        if (ptbReportUser->m_nStatus&dwStatus)
        {
            ptbReportUser->m_nStatus &= (~dwStatus);
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    return FALSE;
}

TUINT32 CWorkProcess::AwsTable2Cache(TbReport_user_cache* ptbReportUserCache, SItemSeq* poCacheEntry, TBOOL bCheckNum)
{
    TUINT32 udwValidNum = 0;
    SReportInfo* poSrcReportUser = NULL;
    m_stProxy.Init(ptbReportUserCache);

    TUINT32 udwEnd = MAX_ITEM_NUM_PER_SEQ;

    if (bCheckNum)
    {
        for (; udwEnd > 0; udwEnd--)
        {
            poSrcReportUser = m_stProxy.GetReportByIndex(udwEnd - 1);
            if (poSrcReportUser->m_nRid > 0)
            {
                break;
            }
        }
    }

    for (TUINT32 udwIdx = 0; udwIdx < udwEnd; udwIdx++)
    {
        poSrcReportUser = m_stProxy.GetReportByIndex(udwIdx);
        poCacheEntry->m_szItems[udwIdx] = *poSrcReportUser;
        udwValidNum++;
    }

    return udwValidNum;
}

TINT32 CWorkProcess::SendAwsRequest(CWorkSession *pstSession, TUINT16 uwReqServiceType)
{
    TUCHAR *pszPack = NULL;
    TUINT32 udwPackLen = 0;
    CDownMgr *pobjDownMgr = CDownMgr::Instance();
    LTasksGroup	stTasksGroup;
    vector<AwsReqInfo*>& vecReq = pstSession->m_vecAwsReq;

    if (vecReq.size() == 0)
    {
        TSE_LOG_ERROR(m_poLog, ("SendAwsRequest: m_vecAwsReq.size() == 0,seq[%u],cseq[%u]",pstSession->m_udwSeq,pstSession->m_udwClientSeq));
        return -1;
    }
    //pstSession->m_uddwReqTimeUs = CTimeUtils::GetCurTimeUs();
    // 1. 获取下游——对于一个session来说，每次只需获取一次
    if (pstSession->m_bAwsProxyNodeExist == FALSE)
    {
        pstSession->m_pstAwsProxyNode = NULL;
        /*if(CGlobalServ::m_poConf->m_poAwsProxy->m_udwDbSvrNum)
        {
        pszPriorHost = CGlobalServ::m_poConf->m_poAwsProxy->m_pszPriorHsSvrIp[0];
        }*/
        if (S_OK == pobjDownMgr->zk_GetNode(DOWN_NODE_TYPE__AWS_PROXY, &pstSession->m_pstAwsProxyNode))
        {
            pstSession->m_bAwsProxyNodeExist = TRUE;
            TSE_LOG_DEBUG(m_poLog, ("Get AwsProx node succ[%s:%u],seq[%u],cseq[%u]", \
                pstSession->m_pstAwsProxyNode->m_szIP, pstSession->m_pstAwsProxyNode->m_uwPort, \
                pstSession->m_udwSeq,pstSession->m_udwClientSeq));
        }
        else
        {
            TSE_LOG_ERROR(m_poLog, ("Get AwsProxy node fail seq[%u], cseq[%u]", pstSession->m_udwSeq,pstSession->m_udwClientSeq));
            return -2;
        }

        if (NULL == pstSession->m_pstAwsProxyNode->m_stDownHandle.handle)
        {
            TSE_LOG_ERROR(m_poLog, ("SendAwsRequest:handle [handlevalue=%p] seq[%u] cseq[%u]", \
                pstSession->m_pstAwsProxyNode->m_stDownHandle.handle, pstSession->m_udwSeq,pstSession->m_udwClientSeq));
            return -3;
        }
    }

    TSE_LOG_DEBUG(m_poLog, ("SendAwsRequest:handle [handlevalue=%p] seq[%u] cseq[%u]", \
        pstSession->m_pstAwsProxyNode->m_stDownHandle.handle, pstSession->m_udwSeq, pstSession->m_udwClientSeq));


    // 2. 封装请求、打包、设置task
    for (unsigned int i = 0; i < vecReq.size() && i < MAX_AWS_REQ_TASK_NUM; ++i)
    {

        CBaseProtocolPack* pPack = m_poPack;
        pPack->ResetContent();
        pPack->SetServiceType(uwReqServiceType);
        pPack->SetSeq(CGlobalServ::GenerateHsReqSeq());
        pPack->SetKey(EN_GLOBAL_KEY__REQ_SEQ, pstSession->m_udwSeq);
        pPack->SetKey(EN_GLOBAL_KEY__INDEX_NO, vecReq[i]->udwIdxNo);
        pPack->SetKey(EN_GLOBAL_KEY__TABLE_NAME, (unsigned char*)vecReq[i]->sTableName.c_str(), vecReq[i]->sTableName.size());
        pPack->SetKey(EN_GLOBAL_KEY__OPERATOR_NAME, (unsigned char*)vecReq[i]->sOperatorName.c_str(), vecReq[i]->sOperatorName.size());
        pPack->SetKey(EN_GLOBAL_KEY__AWSTBL_CACHE_MODE, vecReq[i]->m_udwCacheMode);
        pPack->SetKey(EN_GLOBAL_KEY__AWSTBL_ROUTE_FLD, (unsigned char*)vecReq[i]->m_strRouteFld.c_str(), vecReq[i]->m_strRouteFld.size());
        pPack->SetKey(EN_GLOBAL_KEY__AWSTBL_ROUTE_THRD, vecReq[i]->m_uddwRouteThrd);
        pPack->SetKey(EN_GLOBAL_KEY__AWSTBL_HASH_KEY, (unsigned char*)vecReq[i]->m_strHashKey.c_str(), vecReq[i]->m_strHashKey.size());
        if (!vecReq[i]->m_strRangeKey.empty())
            pPack->SetKey(EN_GOLBAL_KEY__AWSTBL_RANGE_KEY, (unsigned char*)vecReq[i]->m_strRangeKey.c_str(), vecReq[i]->m_strRangeKey.size());
        pPack->SetKey(EN_KEY_HU2HS__REQ_BUF, (unsigned char*)vecReq[i]->sReqContent.c_str(), vecReq[i]->sReqContent.size());
        pPack->GetPackage(&pszPack, &udwPackLen);


        TSE_LOG_DEBUG(m_poLog, ("SendAwsRequest2222:handle [handlevalue=%p] seq[%u] cseq[%u]", \
            pstSession->m_pstAwsProxyNode->m_stDownHandle.handle, pstSession->m_udwSeq, pstSession->m_udwClientSeq));
        stTasksGroup.m_Tasks[i].SetConnSession(pstSession->m_pstAwsProxyNode->m_stDownHandle);

        stTasksGroup.m_Tasks[i].SetSendData(pszPack, udwPackLen);
        stTasksGroup.m_Tasks[i].SetNeedResponse(1);
    }

    // c. 设置task
    stTasksGroup.m_UserData1.ptr = pstSession;
    stTasksGroup.m_UserData2.u32 = pstSession->m_udwSeq;//wave@20160104:为解决主程序已处理，但后续有回包返回时的情况

    stTasksGroup.SetValidTasks(vecReq.size());
    stTasksGroup.m_uTaskTimeOutVal = DOWN_SEARCH_NODE_TIMEOUT_MS;

    TSE_LOG_DEBUG(m_poLog, ("aws req: SendAwsRequest: [vaildtasksnum=%u] seq[%u] cseq[%u]", \
        vecReq.size(),
        pstSession->m_udwSeq,
        pstSession->m_udwClientSeq));

    pstSession->ResetAwsInfo();

    // 3. 发送数据    
    pstSession->m_bProcessing = FALSE;
    if (!m_poSearchLongConn->SendData(&stTasksGroup))
    {
        TSE_LOG_ERROR(m_poLog, ("SendGlobalHsRequest send data failed! seq[%u] cseq[%u]", pstSession->m_udwSeq, pstSession->m_udwClientSeq));
        return -4;
    }

    return 0;
}

CWorkProcess::CWorkProcess()
{

}

CWorkProcess::~CWorkProcess()
{

}
