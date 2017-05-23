#include "search_netio.h"
#include "session.h"
#include "time_utils.h"
#include "jsoncpp/json/json.h"

TINT32 CSearchNetIO::Init(CConf *pobjConf, CTseLogger *pLog, CWorkQueue *poTaskQueue)
{
    if (NULL == pobjConf || NULL == pLog || NULL == poTaskQueue)
    {
        return -1;
    }

    // 1. 设置配置对象,日志对象和任务队列
    m_poConf = pobjConf;
    m_poServLog = pLog;
    m_poTaskQueue = poTaskQueue;

    // 2. 创建长连接对象
    m_pLongConn = CreateLongConnObj();
    if (m_pLongConn == NULL)
    {
        TSE_LOG_ERROR(m_poServLog, ("CSearchNetIO Create longconn failed!"));
        return -2;
    }

    // 3. 侦听端口
    m_hListenSock = CreateListenSocket((TCHAR *)pobjConf->m_szServerIp, pobjConf->m_uwRegPort);
    if (m_hListenSock < 0)
    {
        TSE_LOG_ERROR(m_poServLog, ("QueryNetIO Create listen socket fail"));
        return -3;
    }

    // 4. 初始化长连接
    TBOOL bRet = m_pLongConn->InitLongConn(this, MAX_NETIO_LONGCONN_SESSION_NUM, m_hListenSock, MAX_NETIO_LONGCONN_TIMEOUT_MS);
    if (bRet == FALSE)
    {
        TSE_LOG_ERROR(m_poServLog, ("QueryNetIO Init longconn failed!"));
        return -4;
    }

    // 5. 初始化解包工具
    m_pUnPackTool = new CBaseProtocolUnpack();
    m_pUnPackTool->Init();

    return 0;
}

void * CSearchNetIO::RoutineNetIO(void *pParam)
{
    CSearchNetIO *net_io = (CSearchNetIO *)pParam;
    while (1)
    {
        net_io->m_pLongConn->RoutineLongConn(100);
    }

    return NULL;
}

void CSearchNetIO::OnUserRequest(LongConnHandle hSession, const TUCHAR *pszData, TUINT32 udwDataLen, BOOL &bWillResponse)
{
    //Do nothing
    return;
}

void CSearchNetIO::OnTasksFinishedCallBack(LTasksGroup *pstTasksGrp)
{
    CWorkSession *poWorkSession = 0;

    if (0 == pstTasksGrp)
    {
        TSE_LOG_DEBUG(m_poServLog, ("pstTasksGrp is null"));
        return;
    }

    if (0 == pstTasksGrp->m_Tasks[0].ucNeedRecvResponse)
    {
        TSE_LOG_DEBUG(m_poServLog, ("Invalid call back"));
        return;
    }

    // 1> get session wrapper
    poWorkSession = (CWorkSession *)(pstTasksGrp->m_UserData1.ptr);
    pstTasksGrp->m_UserData1.ptr = 0;

    if (0 == poWorkSession)
    {
        TSE_LOG_ERROR(m_poServLog, ("No session attached in task_grp"));
        return;
    }

    // 2> copy the msg mem to session wrapper
    TSE_LOG_DEBUG(m_poServLog, ("recv res next=%u,exp_serv=%u,session[%p] [seq=%u]", 
        poWorkSession->m_udwNextProcedure, poWorkSession->m_udwExpectProcedure, poWorkSession, poWorkSession->m_udwSeq));
    switch (poWorkSession->m_udwExpectProcedure)
    {
    case EN_EXPECT_PROCEDURE__AWS:
        OnAwsResponse(pstTasksGrp, poWorkSession);
        break;
    default:
        break;
    }

    poWorkSession->m_udwExpectProcedure = EN_EXPECT_PROCEDURE__INIT;
    poWorkSession->m_ucWorkType = EN_WORK_TYPE__RESPONSE;

    // 3> push session wrapper to work queue
    m_poTaskQueue->WaitTillPush(poWorkSession);
}

SOCKET CSearchNetIO::CreateListenSocket(TCHAR *pszListenHost, TUINT16 uwPort)
{
    // 1> 申请SOCKET
    SOCKET lSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (lSocket < 0)
    {
        return -1;
    }

    // 2> 设置端口可重用
    int option = 1;
    if (setsockopt(lSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
    {
        close(lSocket);
        return -1;
    }

    // 2> 绑定端口
    sockaddr_in sa;
    memset(&sa, 0, sizeof(sockaddr_in));
    sa.sin_port = htons(uwPort);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = inet_addr(pszListenHost);
    int rv = bind(lSocket, (struct sockaddr *) &sa, sizeof(sa));

    if (rv < 0)
    {
        close(lSocket);
        return -1;
    }

    // 3> 监听
    rv = listen(lSocket, uwPort);

    if (rv < 0)
    {
        close(lSocket);
        return -1;
    }

    return lSocket;
}

TINT32 CSearchNetIO::CloseListenSocket()
{
    if (m_hListenSock >= 0)
    {
        close(m_hListenSock);
    }

    return 0;
}

TINT32 CSearchNetIO::GetIp2PortByHandle(LongConnHandle stHandle, TUINT16 *puwPort, TCHAR **ppszIp)
{
    TUINT32 udwHost = 0;
    TUINT16 uwPort = 0;

    m_pLongConn->GetPeerName(stHandle, &udwHost, &uwPort);
    *puwPort = ntohs(uwPort);
    *ppszIp = inet_ntoa(*(in_addr *)&udwHost);
    return 0;
}

TINT32 CSearchNetIO::OnAwsResponse(LTasksGroup *pstTasksGrp, CWorkSession *poWorkSession)
{
    LTask *pstTask = 0;
    TCHAR *pszIp = 0;
    TUINT16 uwPort = 0;
    TUINT32 udwIdx = 0;
    TINT32 dwRetCode = 0;

    poWorkSession->m_uddwDownRqstTimeEnd = CTimeUtils::GetCurTimeUs();

    poWorkSession->ResetAwsInfo();
    vector<AwsRspInfo*>& vecRsp = poWorkSession->m_vecAwsRsp;
    AwsRspInfo* pAwsRsp = NULL;

    for (udwIdx = 0; udwIdx < pstTasksGrp->m_uValidTasks; udwIdx++)
    {
        pstTask = &pstTasksGrp->m_Tasks[udwIdx];
        GetIp2PortByHandle(pstTask->hSession, &uwPort, &pszIp);

        pAwsRsp = new AwsRspInfo;
        vecRsp.push_back(pAwsRsp);

        TSE_LOG_DEBUG(m_poServLog, ("aws_proxy res, [ip=%s] [port=%u] [send=%u], [recv=%u], [timeout=%u], "
            "[donw_busy=%u], [socket_closed=%u], [verify_failed=%u] [recv_data_len=%u] [seq=%u]",
            pszIp, uwPort, pstTask->_ucIsSendOK, pstTask->_ucIsReceiveOK, pstTask->_ucTimeOutEvent,
            pstTask->_ucIsDownstreamBusy, pstTask->_ucIsSockAlreadyClosed, pstTask->_ucIsVerifyPackFail,
            pstTask->_uReceivedDataLen, poWorkSession->m_udwSeq));

        if (0 == pstTask->_ucIsSendOK || 0 == pstTask->_ucIsReceiveOK || 1 == pstTask->_ucTimeOutEvent)
        {
            if (poWorkSession->m_bAwsProxyNodeExist)
            {
                CDownMgr::Instance()->zk_AddTimeOut(poWorkSession->m_pstAwsProxyNode);
            }
            TSE_LOG_ERROR(m_poServLog, ("aws_proxy res, [send=%u], [recv=%u], [timeout=%u], [cost_time=%llu]us [seq=%u]",
                pstTask->_ucIsSendOK, pstTask->_ucIsReceiveOK, pstTask->_ucTimeOutEvent,
                poWorkSession->m_uddwDownRqstTimeEnd - poWorkSession->m_uddwDownRqstTimeBeg,
                poWorkSession->m_udwSeq));
            poWorkSession->m_dwRetCode = EN_RET_CODE__TIMEOUT;
            dwRetCode = -1;
            break;
        }
        else
        {
            if (pstTask->_uReceivedDataLen > MAX_NETIO_PACKAGE_BUF_LEN)
            {
                TSE_LOG_ERROR(m_poServLog, ("aws_proxy res, recv_data_len[%u]>MAX_NETIO_PACKAGE_BUF_LEN [cost_time=%llu]us [seq=%u]",
                    pstTask->_uReceivedDataLen, poWorkSession->m_uddwDownRqstTimeEnd - poWorkSession->m_uddwDownRqstTimeBeg,
                    poWorkSession->m_udwSeq));
                poWorkSession->m_dwRetCode = EN_RET_CODE__PACKAGE_LEN_OVERFLOW;
                dwRetCode = -2;
                break;
            }
            else
            {
                dwRetCode = ParseAwsResponse(pstTask->_pReceivedData, pstTask->_uReceivedDataLen, pAwsRsp);
                if (dwRetCode < 0)
                {
                    // 加入错误统计
                    if (poWorkSession->m_bAwsProxyNodeExist)
                    {
                        CDownMgr::Instance()->zk_AddError(poWorkSession->m_pstAwsProxyNode);
                    }

                    TSE_LOG_ERROR(m_poServLog, ("ParseAwsResponse[%u] failed[%d] [seq=%u]", 
                        udwIdx, dwRetCode, poWorkSession->m_udwSeq));
                    poWorkSession->m_dwRetCode = EN_RET_CODE__PARSE_PACKAGE_ERR;
                    dwRetCode = -3;
                    break;
                }
                else
                {
                    TSE_LOG_DEBUG(m_poServLog, ("ParseAwsResponse[%u]: service[%u], ret[%d], table_name[%s], buf_len[%u] [seq=%u]",
                        udwIdx, pAwsRsp->udwServiceType, pAwsRsp->dwRetCode, pAwsRsp->sTableName.c_str(),
                        pAwsRsp->sRspContent.size(), poWorkSession->m_udwSeq));
                }
            }
        }
    }

    return 0;
}

TINT32 CSearchNetIO::ParseAwsResponse(TUCHAR *pszPack, TUINT32 udwPackLen, AwsRspInfo* pAwsRspInfo)
{
    TUCHAR *pszValBuf = NULL;
    TUINT32 udwValBufLen = 0;

    m_pUnPackTool->UntachPackage();
    m_pUnPackTool->AttachPackage(pszPack, udwPackLen);
    if (FALSE == m_pUnPackTool->Unpack())
    {
        return -1;
    }
    pAwsRspInfo->udwServiceType = m_pUnPackTool->GetServiceType();
    m_pUnPackTool->GetVal(EN_GLOBAL_KEY__RES_CODE, &pAwsRspInfo->dwRetCode);
    m_pUnPackTool->GetVal(EN_GLOBAL_KEY__RES_COST_TIME, &pAwsRspInfo->udwCostTime);
    m_pUnPackTool->GetVal(EN_GLOBAL_KEY__INDEX_NO, &pAwsRspInfo->udwIdxNo);
    if (m_pUnPackTool->GetVal(EN_GLOBAL_KEY__TABLE_NAME, &pszValBuf, &udwValBufLen))
    {
        pAwsRspInfo->sTableName.resize(udwValBufLen);
        memcpy((char*)pAwsRspInfo->sTableName.c_str(), pszValBuf, udwValBufLen);
    }

    //2XX的返回码,都是正常的情况
    if (pAwsRspInfo->dwRetCode / 100 != 2)
    {
        TSE_LOG_ERROR(m_poServLog, ("pAwsRspInfo->dwRetCode=%d\n", pAwsRspInfo->dwRetCode));
        return -2;
    }

    if (pAwsRspInfo->udwServiceType == EN_SERVICE_TYPE_QUERY_DYNAMODB_RSP)
    {
        m_pUnPackTool->GetVal(EN_GLOBAL_KEY__RES_BUF, &pszValBuf, &udwValBufLen);
        pAwsRspInfo->sRspContent.resize(udwValBufLen);
        memcpy((char*)pAwsRspInfo->sRspContent.c_str(), pszValBuf, udwValBufLen);
    }
    else
    {
        return -3;
    }
    return 0;
}
