#pragma once
#include <string>
#include "queue_t.h"
#include "base/common/wtsetypedef.h"
#include "svrpublib/BaseProtocol.h"
#include "svrpublib/ILongConn.h"
#include "aws_request.h"
#include "aws_response.h"
#include "my_define.h"
#include "zkutil/zk_util.h"

using namespace std;


enum EReportUserProcessCmd
{
    EN_REPORT_USER_OP__REPORT_GET = 0,
    EN_REPORT_USER_OP__REPORT_DETAIL_GET,
    EN_REPORT_USER_OP__REPORT_READ,
    EN_REPORT_USER_OP__REPROT_DEL,
    EN_REPORT_USER_OP__ALLIANCE_REPORT_GET,
    EN_REPORT_USER_OP__OP_REPORT_GET,
    EN_REPORT_USER_OP__REPORT_SEND,
    EN_REPORT_USER_OP__REPORT_MULTI_SEND,
    EN_REPORT_USER_OP__REPORT_SCAN_FOR_DEBUG = 100,
};

/*************************************************************************************
WorkSession包装器
**************************************************************************************/
struct SReportUserReqInfo;
typedef struct SReportUserReqInfo ReportUserReq;
struct SReportUserReqInfo
{
    TINT64 m_ddwUid;
    TINT64 m_ddwAid;
    TINT64 m_ddwSid;
    TINT64 m_ddwDid;
    TINT64 m_ddwProcessCmd;
    TINT64 m_ddwKey;
    TINT64 m_ddwRid;
    TUINT8 m_ucReportType;
    TUINT8 m_ucGetType;
    TUINT8 m_ucStatus;
    TUINT32 m_udwCurPage;
    TUINT32 m_udwPerPage;
    vector<TUINT8> m_vecOpType;
    vector<TINT64> m_vecRid;

    //下列参数用于群发report
    vector<TINT64> m_vecKeys;
    vector<SReportInfo> m_vecReports;
    TVOID Reset()
    {
        m_ddwUid = 0;
        m_ddwAid = 0;
        m_ddwSid = 0;
        m_ddwDid = 0;
        m_ddwProcessCmd = 0;
        m_ddwKey = 0;
        m_ddwRid = 0;
        m_ucReportType = 0;
        m_ucGetType = 0;
        m_ucStatus = 0;
        m_udwCurPage = 0;
        m_udwPerPage = 0;
        m_vecOpType.clear();
        m_vecRid.clear();
        m_vecKeys.clear();
        m_vecReports.clear();
    }
    SReportUserReqInfo()
    {
        Reset();
    }
};


struct SReportEntry
{
    TINT64 ddwRid;
    TINT32 dwUnread;
    TINT32 dwReportType;
    TINT64 ddwDisplayClass;
    TINT32 dwTime;
    TINT32 dwStatus;
    TINT32 dwNum;

    SReportEntry()
    {
        memset(this, 0, sizeof(*this));
    }
    TVOID Reset()
    {
        memset(this, 0, sizeof(*this));
    }
};



struct SReportUserRspInfo;
typedef struct SReportUserRspInfo ReportUserRsp;
struct SReportUserRspInfo
{
    TUINT32 m_udwReportTotalNum;
    TUINT32 m_udwReportUnreadNum;
    TINT64 m_ddwNewestRid;
    TUINT32 m_udwReportEntryNum;
    SReportEntry m_aReportToReturn[MAX_PERPAGE_NUM];
    TVOID Reset()
    {
        memset(this, 0, sizeof(*this));
    }
        
    SReportUserRspInfo()
    {
        Reset();
    }
};

class CWorkSession
{
public:
    //上游CBU信息
    LongConnHandle      m_stHandle;                             // 请求连接过来的handle
    TUINT32             m_udwClientSeq;                         // 请求连接的seqnum
    TUINT32             m_udwReqSeq;                            // 请求自身的seqnum
    TUINT32             m_udwServiceType;                       // 请求服务类型
    TUINT32             m_udwSeq;                               // 自身的seqnum

    //原始请求
    TUCHAR m_szClientReqBuf[MAX_NETIO_PACKAGE_BUF_LEN];
    TUINT32 m_udwClientReqBufLen;


    //时间参数
    TUINT64             m_uddwReqTimeUs;						//请求时间
    TUINT64             m_uddwResTimeUs;						//返包给上游的时间
    TUINT64             m_uddwWaitSessionCostTimeUs;
    TUINT64             m_uddwPushSessionCostTimeUs;
    TUINT64             m_uddwInQueueTimeBegUs;
    TUINT64             m_uddwInQueueCostTimeUs;
    TUINT64             m_uddwUnpackCostTimeUs;
    TUINT64             m_uddwWriteFileCostTimeUs;
    TUINT64             m_uddwQueryTimeBegUs;
    TUINT64             m_uddwQueryPlayerCostTimeUs;
    TUINT64             m_uddwQueryAlCostTimeUs;
    TUINT64             m_uddwQuerySvrCostTimeUs;
    TUINT64             m_uddwTaskProcessCostTimeUs;
    TUINT64             m_uddwReqWritebackCostTimeUs;
    TUINT64             m_uddwRspCostTimeUs;
    TUINT64             m_uddwDownRqstTimeBeg;
    TUINT64             m_uddwDownRqstTimeEnd;

    TUINT32             m_udwCommandStep;
    TUINT32             m_udwNextProcedure;
    TUINT32             m_udwExpectProcedure;

    TINT32              m_dwRetCode;
    TUINT8              m_ucIsUsing;
    TBOOL               m_bProcessing;
    TBOOL               m_bNeedRsp;

    //返包信息
    string              m_szRspContent;

    CBaseProtocolPack	*m_poPackTool;
    CBaseProtocolUnpack *m_pUnPackTool;

    //下游结点
    SDownNode           *m_pstAwsProxyNode;
    TBOOL               m_bAwsProxyNodeExist;

    //请求相关信息
    ReportUserReq             m_objReqInfo;
    ReportUserRsp             m_objRspInfo;

    //处理过程信息
    TUINT8              m_ucWorkType;
    TUINT32             m_udwKeyIndex;
    TUINT32             m_udwErrCnt;
    set<TUINT32>        m_setDeref; //作用??
    TUINT32             m_udwFileIndex;
    TUINT64             m_uddwFileRow;
    TUINT32             m_udwCacheType;


    //aws info
    vector<AwsReqInfo*> m_vecAwsReq;
    vector<AwsRspInfo*> m_vecAwsRsp;



    TVOID    Init()
    {
        Reset();
    }

    TVOID    Reset()
    {
        memset((char*)&m_stHandle, 0, sizeof(m_stHandle));
        m_udwClientSeq = 0;
        m_udwReqSeq = 0;
        m_udwServiceType = 0;
        m_udwSeq = 0;

        m_szClientReqBuf[0] = 0;
        m_udwClientReqBufLen = 0;

        m_uddwReqTimeUs = 0;
        m_uddwResTimeUs = 0;
        m_uddwWaitSessionCostTimeUs = 0;
        m_uddwPushSessionCostTimeUs = 0;
        m_uddwInQueueTimeBegUs = 0;
        m_uddwInQueueCostTimeUs = 0;
        m_uddwUnpackCostTimeUs = 0;
        m_uddwWriteFileCostTimeUs = 0;
        m_uddwQueryTimeBegUs = 0;
        m_uddwQueryPlayerCostTimeUs = 0;
        m_uddwQueryAlCostTimeUs = 0;
        m_uddwQuerySvrCostTimeUs = 0;
        m_uddwTaskProcessCostTimeUs = 0;
        m_uddwReqWritebackCostTimeUs = 0;
        m_uddwRspCostTimeUs = 0;
        m_uddwDownRqstTimeBeg = 0;
        m_uddwDownRqstTimeEnd = 0;


        m_udwExpectProcedure = EN_EXPECT_PROCEDURE__INIT;
        m_udwNextProcedure = EN_PROCEDURE__INIT;
        m_udwCommandStep = EN_COMMAND_STEP__INIT;

        m_dwRetCode = EN_RET_CODE__SUCCESS;
        m_ucIsUsing = 0;
        m_bProcessing = FALSE;

        m_szRspContent = "";

        m_pUnPackTool = NULL;
        
        m_bAwsProxyNodeExist = FALSE;
        m_pstAwsProxyNode = NULL;

        m_objReqInfo.Reset();
        m_objRspInfo.Reset();

        m_ucWorkType = EN_WORK_TYPE__REQUEST;
        m_udwKeyIndex = 0;
        m_udwErrCnt = 0;
        m_setDeref.clear();
        m_udwFileIndex = 0;
        m_uddwFileRow = 0;
        m_udwCacheType = 0;

        ResetAwsInfo();
    }

    void ResetAwsReq()
    {
        for (unsigned int i = 0; i < m_vecAwsReq.size(); ++i)
        {
            delete m_vecAwsReq[i];
        }
        m_vecAwsReq.clear();
    }
    void ResetAwsRsp()
    {
        for (unsigned int i = 0; i < m_vecAwsRsp.size(); ++i)
        {
            delete m_vecAwsRsp[i];
        }
        m_vecAwsRsp.clear();
    }
    void ResetAwsInfo()
    {
        ResetAwsReq();
        ResetAwsRsp();
    }
};

typedef CQueueT<CWorkSession * >     CWorkQueue;


/*************************************************************************************
WritebackSession包装器
**************************************************************************************/
enum EWritebackSessionType
{
    EN_WRITEBACK_TYPE_REQ=0,
    EN_WRITEBACK_TYPE_LRU,
    EN_WRITEBACK_TYPE_OVERFLOW
};
class CWritebackSession
{
public:
    //上游CBU信息
    TUINT32             m_udwClientSeq;                         // CBU请求的seqnum
    TUINT32             m_udwSeq;                               // 自身的seqnum

    vector<TINT64>      m_vecKeys;
    TINT64              m_ddwRid;                               // rid
    TUINT32             m_udwType;                              // 内存索引的类型
    
    TUINT32             m_udwFileIndex;
    TUINT64             m_uddwFileRow;
   

    TVOID    Init()
    {
        Reset();
    }

    TVOID    Reset()
    {
        m_udwClientSeq = 0;
        m_udwSeq = 0;

        m_vecKeys.clear();
        m_ddwRid = 0;
        m_udwType = 0;

        m_udwFileIndex = 0;
        m_uddwFileRow = 0;
    }
};

typedef CQueueT<CWritebackSession * >     CWritebackQueue;

