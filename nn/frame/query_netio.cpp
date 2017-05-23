#include "query_netio.h"
#include "work_session_mgr.h"
#include "writeback_session_mgr.h"
#include "time_utils.h"
#include "file_mgr.h"
#include "unpack_util.h"


TBOOL CQueryNetIO::m_bRunLongConn = FALSE;


//=================================================================
// initialize
CQueryNetIO::CQueryNetIO()
{
	// do nothing
}

CQueryNetIO::~CQueryNetIO()
{
	if (NULL != m_pLongConn)
	{
		m_pLongConn->UninitLongConn();
		m_pLongConn->Release();
		m_pLongConn = NULL;
	}

	m_poUnpack->Uninit();
	m_poPack->Uninit();
}

TINT32 CQueryNetIO::Init(CConf *poConf, CTseLogger *poLog, CWorkQueue *poWorkQueue, CWritebackQueue *poDelayTaskQueue)
{
    if (NULL == poConf || NULL == poLog || NULL == poWorkQueue || NULL == poDelayTaskQueue)
    {
        return -1;
    }

    TBOOL bRet = -1;

    // 1. 设置配置对象,日志对象和任务队列
    m_poLog = poLog;
    m_poTaskQueue = poWorkQueue;
    m_poDelayTaskQueue = poDelayTaskQueue;

    // 2. 创建长连接对象
    m_pLongConn = CreateLongConnObj();
    if (0 == m_pLongConn)
    {
        TSE_LOG_ERROR(m_poLog, ("QueryNetIO Create longconn fail"));
        return -2;
    }

    // 3. 侦听端口
    m_hListenSock = CreateListenSocket((TCHAR *)poConf->m_szServerIp, poConf->m_uwServerPort);
    if (m_hListenSock < 0)
    {
        TSE_LOG_ERROR(m_poLog, ("QueryNetIO Create listen socket fail"));
        return -3;
    }

    // 4. 初始化长连接
    bRet = m_pLongConn->InitLongConn(this, MAX_NETIO_LONGCONN_SESSION_NUM, m_hListenSock, MAX_NETIO_LONGCONN_TIMEOUT_MS);
    if (bRet == FALSE)
    {
        TSE_LOG_ERROR(m_poLog, ("QueryNetIO Init longconn failed!"));
        return -4;
    }

    // 5. 初始化打包/解包工具
    m_poPack = new CBaseProtocolPack();
    m_poPack->Init();
    m_poUnpack = new CBaseProtocolUnpack();
    m_poUnpack->Init();

    m_bServNew = FALSE;
    m_bRelease = FALSE;

    CQueryNetIO::m_bRunLongConn = TRUE;

    m_udwSeqno = 1000;

    return 0;
}

TINT32 CQueryNetIO::Uninit()
{
	m_bServNew = FALSE;
	// do nothing else
	return 0;
}

//=================================================================
// control
TVOID *CQueryNetIO::RoutineNetIO(TVOID *pParam)
{
	CQueryNetIO *pNetIO  = 0;
	pNetIO = (CQueryNetIO*)pParam;

    /*
    while (1)
    {
        pNetIO->m_pLongConn->RoutineLongConn(100);
    }*/   

    
	pNetIO->StartNetServ();
	for(;;)
	{
	    //收发数据,不能停
	    if(m_bRunLongConn)
	    {
		    pNetIO->m_pLongConn->RoutineLongConn(100);
		    pNetIO->m_bRelease = TRUE;
	    }
	    else
	    {
	        pNetIO->m_bRelease = TRUE;
	        break;
	    }
	    pNetIO->m_bRelease = FALSE;
	}

	return 0;

}

TVOID CQueryNetIO::StartNetServ()
{
	m_bServNew = TRUE;
	TSE_LOG_INFO(m_poLog, ("QueryNetIO start net service"));
}

TVOID CQueryNetIO::StopNetServ()
{
	m_bServNew = FALSE;
	TSE_LOG_INFO(m_poLog, ("QueryNetIO stop net service"));
}

TVOID CQueryNetIO::StopLongConn()
{
	CQueryNetIO::m_bRunLongConn = FALSE;
	sleep(1);
	while (FALSE == m_bRelease)
	{
		usleep(1000);
	}

	if (NULL != m_pLongConn)
	{
		m_pLongConn->UninitLongConn();
		m_pLongConn->Release();
		m_pLongConn = NULL;
	}
}

TVOID CQueryNetIO::OnTasksFinishedCallBack(LTasksGroup *pstTasksGrp)
{
    //do nothing
}

TVOID CQueryNetIO::OnUserRequest(LongConnHandle stHandle, const TUCHAR *pszData, TUINT32 udwDataLen, TINT32 &dwWillResponse)
{
	if (FALSE == m_bServNew)
	{
		TSE_LOG_DEBUG(m_poLog, ("QueryNetIO on user request but service not started"));
		return;
	}

    m_udwSeqno++;

	TUINT16 uwServType = 0;
	TUINT32 udwReqSeq = 0;

	// 1> Get Service Type
	m_poUnpack->UntachPackage();
	m_poUnpack->AttachPackage((TUCHAR*)pszData, udwDataLen);
	uwServType = m_poUnpack->GetServiceType();
	udwReqSeq = m_poUnpack->GetSeq();

    TSE_LOG_DEBUG(m_poLog, ("onUserRequest Handle [serv_type=%u] [seq=%u] [hadle:%lu] [sn:%u]", uwServType, m_udwSeqno, (TUINT64)(stHandle.handle), stHandle.SerialNum));
	switch (uwServType)
	{
    case EN_SERVICE_TYPE_QUERY_DYNAMODB_REQ:
    case EN_SERVICE_TYPE_REPORT_CENTER_REQ:
		if (0 != OnRequest(stHandle, udwReqSeq, pszData, udwDataLen, uwServType))
		{
            TSE_LOG_ERROR(m_poLog, ("QueryNetIO Handle data update fail [seq=%u]", m_udwSeqno));
            SendBackErr(stHandle, uwServType, udwReqSeq);
            dwWillResponse = FALSE;
		}
		break;
	default :
        TSE_LOG_ERROR(m_poLog, ("QueryNetIO Unknown service type=[%u] [seq=%u] ", uwServType, m_udwSeqno));
        SendBackErr(stHandle, uwServType, udwReqSeq);
        dwWillResponse = FALSE;
	}
}

TINT32 CQueryNetIO::OnRequest(LongConnHandle stHandle, TUINT32 udwReqSeqNum, const TUCHAR *pszData, TUINT32 udwDataLen, TUINT16 uwServType)
{
    TUINT64 uddwTimeStart = CTimeUtils::GetCurTimeUs();
    TUINT64 uddwTimeA = 0, uddwTimeB = 0;

    TUCHAR *pvValueBuf = NULL;
    TUINT32 udwValueBufLen = 0;
    TUINT32 udwRealReqSeqNum = 0;

    TSE_LOG_DEBUG(m_poLog, ("new data update [seq=%u] [cseq=%u]", m_udwSeqno, udwReqSeqNum));

	// 1> Check DataLen
	if (udwDataLen > MAX_NETIO_PACKAGE_BUF_LEN)
	{
		TSE_LOG_ERROR(m_poLog, ("recv dataLen more than MAX_NETIO_PACKAGE_BUF_LEN [%u:%u] [seq=%u]", \
            udwDataLen, MAX_NETIO_PACKAGE_BUF_LEN, m_udwSeqno));
		return -1;
	}

    if (FALSE == m_poUnpack->Unpack())
    {
        TSE_LOG_ERROR(m_poLog, ("unpack failed, seq[%u], clientseq[%u]", \
            m_udwSeqno, udwReqSeqNum));
        return -2;
    }
    if (!m_poUnpack->GetVal(EN_KEY_HU2HS__REQ_BUF, (TUCHAR **)&pvValueBuf, &udwValueBufLen))
    {
        TSE_LOG_ERROR(m_poLog, ("get EN_KEY_HU2HS__REQ_BUF[%d] failed, seq[%u], clientseq[%u]", \
            EN_KEY_HU2HS__REQ_BUF, m_udwSeqno, udwReqSeqNum));
        return -3;
    }
    if (!m_poUnpack->GetVal(EN_GLOBAL_KEY__REQ_SEQ, &udwRealReqSeqNum))
    {
        TSE_LOG_ERROR(m_poLog, ("get EN_GLOBAL_KEY__REQ_SEQ[%d] failed, seq[%u], cseq[%u]", \
            EN_GLOBAL_KEY__REQ_SEQ, m_udwSeqno, udwReqSeqNum));
        return -4;
    }

    // 获取工作队列的处理节点
    uddwTimeA = CTimeUtils::GetCurTimeUs();
    CWorkSession *pstWorkSession = 0;
    if (0 != CWorkSessionMgr::Instance()->WaitTillSession(&pstWorkSession))
    {
        TSE_LOG_ERROR(m_poLog, ("Get WorkSession failed [seq=%u]", m_udwSeqno));
        return -5;
    }
    else
    {
        TSE_LOG_INFO(m_poLog, ("[FOR MEMORY] Get WorkSession OK, session[%p], empty_size=%u [seq=%u] [cseq=%u] [rseq=%u]",
            pstWorkSession, CWorkSessionMgr::Instance()->GetEmptySize(), m_udwSeqno, udwReqSeqNum, udwRealReqSeqNum));
    }
    uddwTimeB = CTimeUtils::GetCurTimeUs();
    pstWorkSession->m_uddwWaitSessionCostTimeUs = uddwTimeB - uddwTimeA;

    // copy data to work session
    pstWorkSession->Reset();
    pstWorkSession->m_ucIsUsing = 1;
    pstWorkSession->m_stHandle = stHandle;
    pstWorkSession->m_udwSeq = m_udwSeqno;
    pstWorkSession->m_udwClientSeq = udwReqSeqNum;
    pstWorkSession->m_udwReqSeq = udwRealReqSeqNum;
    pstWorkSession->m_udwServiceType = uwServType;
    pstWorkSession->m_uddwReqTimeUs = uddwTimeStart;
    memcpy(pstWorkSession->m_szClientReqBuf, pvValueBuf, udwValueBufLen);
    pstWorkSession->m_szClientReqBuf[udwValueBufLen] = 0;
    pstWorkSession->m_udwClientReqBufLen = udwValueBufLen;
    pstWorkSession->m_ucWorkType = EN_WORK_TYPE__REQUEST;
    pstWorkSession->m_bNeedRsp = TRUE;

	  // push work session to work queue
    uddwTimeA = CTimeUtils::GetCurTimeUs();
    pstWorkSession->m_uddwInQueueTimeBegUs = uddwTimeA; 
    m_poTaskQueue->WaitTillPush(pstWorkSession);
    uddwTimeB = CTimeUtils::GetCurTimeUs();
    pstWorkSession->m_uddwPushSessionCostTimeUs = uddwTimeB - uddwTimeA;

	return 0;
}


SOCKET CQueryNetIO::CreateListenSocket(TCHAR* pszListenHost, TUINT16 uwPort)
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

	// 3> 绑定端口
	sockaddr_in sa;
	memset(&sa, 0, sizeof(sockaddr_in));
	sa.sin_port = htons(uwPort);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr(pszListenHost);
	int rv = bind(lSocket, (struct sockaddr *) &sa,  sizeof(sa));

	if (rv < 0)
	{
		close(lSocket);
		return -1;
	}

	// 4> 监听
	rv = listen(lSocket, uwPort);

	if (rv < 0)
	{
		close(lSocket);
		return -1;
	}

    return lSocket;
}

TINT32 CQueryNetIO::CloseListenSocket()
{
	if (m_hListenSock >= 0)
	{
		close(m_hListenSock);
	}
	return 0;
}

TINT32 CQueryNetIO::GetIp2PortByHandle(LongConnHandle stHandle, TUINT16 *puwPort, TCHAR **ppszIp)
{
	TUINT32 udwHost = 0;
	TUINT16 uwPort = 0;

	m_pLongConn->GetPeerName(stHandle, &udwHost, &uwPort);
	*puwPort = ntohs(uwPort);
	*ppszIp = inet_ntoa(*(in_addr*)&udwHost);
	return 0;
}

TINT32 CQueryNetIO::SendBackErr(LongConnHandle stHandle, TUINT32 udwServType, TUINT32 udwSeq)
{
    TUCHAR *pucPackage = NULL;
    TUINT32 udwPackageLen = 0;

    // 1. get package
    m_poPack->ResetContent();

    if (udwServType == EN_SERVICE_TYPE_QUERY_DYNAMODB_REQ)
    {
        m_poPack->SetServiceType(EN_SERVICE_TYPE_QUERY_DYNAMODB_RSP);
    }
    else
    {
        m_poPack->SetServiceType(EN_SERVICE_TYPE_REPORT_CENTER_RSP);
    }

    m_poPack->SetSeq(udwSeq);
    m_poPack->SetKey(EN_GLOBAL_KEY__RES_CODE, EN_RET_CODE__PARAM_ERROR);
    m_poPack->SetKey(EN_GLOBAL_KEY__RES_COST_TIME, 1);
    m_poPack->GetPackage(&pucPackage, &udwPackageLen);

    // 2. send back
    LTasksGroup stTasks;
    stTasks.m_Tasks[0].SetConnSession(stHandle);
    stTasks.m_Tasks[0].SetSendData(pucPackage, udwPackageLen);
    stTasks.m_Tasks[0].SetNeedResponse(0);
    stTasks.SetValidTasks(1);
    if (!m_pLongConn->SendData(&stTasks))
    {
        TSE_LOG_ERROR(m_poLog, ("send back res failed"));
        return -1;
    }
    TSE_LOG_INFO(m_poLog, ("SendBackErr [seq=%u]", m_udwSeqno));
    return 0;
}

