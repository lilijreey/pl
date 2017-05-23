#pragma once
#include "session.h"
#include "cache_mgr.h"
#include "aws_table_report_user_cache.h"
#include "report_user_cache_proxy.h"

/*************************************************************************************
后台工作线程
**************************************************************************************/
class CWorkProcess
{
public:
    CWorkProcess();
    ~CWorkProcess();
    /*
    *    初始化函数
    */
    TINT32 Init(CConf *poConf, CTseLogger *poServLog, CTseLogger *poReqLog, CWorkQueue *poUpdateQue, CWritebackQueue *poWritebackQ,
        ILongConn *poQueryLongConn, ILongConn *poSearchLongConn);
    /*
    *    线程启动函数
    */
    static TVOID * Start(TVOID *pParam);
    /*
    *    工作循环
    */
    TINT32    WorkRoutine();

private:
    // 处理前端请求
    TINT32 ProcessDispatch(CWorkSession *poSession);
    TINT32 ProcessResponse(CWorkSession *poSession);
    TINT32 RequestWriteFile(CWorkSession* poSession);
    TINT32 RequestWriteback(CWorkSession *poSession);

    /***********************************report的相关操作**************************************/
    TINT32 Processcmd_ReportPut(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 Processcmd_ReportMultiPut(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 ProcessCmd_ReportGet(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 ProcessCmd_ReportDetailGet(CWorkSession *pstSession, TBOOL &bNeedResponse);

    TINT32 ProcessCmd_ReportRead(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 ProcessCmd_ReportDel(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 ProcessCmd_ReportScanForDebug(CWorkSession *pstSession, TBOOL &bNeedResponse);

    // function  ==> 获取当前页report的id列表
    TINT32 ProcessCmd_AllianceReportGet(CWorkSession *pstSession, TBOOL &bNeedResponse);

    // function  ==> 获取当前页report的id列表
    TUINT32 GetCurPageReportIdList(SItemExt*pstRawList, TUINT32 udwRawNum, TINT8 cType, TUINT32 udwPage,
        TUINT32 udwPerPage, std::vector<SReportEntry>& vecDetailReportEntries, TUINT32& udwTotalNum);

    static bool TbReport_user_Compare_Reverse(const SReportInfo stA, const SReportInfo stB);

    TINT32 QueryReportId(CWorkSession* pstSession);
    TINT32 QueryLatestReportId(CWorkSession* pstSession);
    TINT64 GetReportDisplayClass(const SReportInfo& tbReportUser);
    static TBOOL ReportEntryGreater(const SReportEntry& stA, const SReportEntry stB);
    static TBOOL ReportUserExtGreater(const SItemExt& stA, const SItemExt& stB);

    TINT32 QueryCache(CWorkSession* pstSession, SItemExt* szItemExt, TUINT32* pudwItemNum, TBOOL bIncludeDel = FALSE);
    TINT32 SetReportStatus(TINT32 dwStatus, CWorkSession* pstSession, TBOOL& bNeedResponse, TBOOL bClear = FALSE);
    TBOOL UpdateReportStatus(SReportInfo* ptbReportUser, TBOOL bClear, TINT32 dwStatus);
    TUINT32 AwsTable2Cache(TbReport_user_cache* ptbReportUserCache, SItemSeq* poCacheEntry, TBOOL bCheckNum);
    TINT32 SendAwsRequest(CWorkSession *pstSession, TUINT16 uwReqServiceType);

private:
    ILongConn	*m_poQueryLongConn;		//上游请求长连接
    ILongConn	*m_poSearchLongConn;		//下游长连接
    CConf	*m_poConf;				    //配置

    CTseLogger	*m_poLog;					//Log
    CTseLogger	*m_poReqLog;                 // 请求日志

    CWorkQueue	*m_poWorkQueue;         //更新队列
    CWritebackQueue *m_poWritebackQ;

    CBaseProtocolUnpack	*m_poUnpack;              // 解包工具
    CBaseProtocolPack	*m_poPack;				// 打包工具

    SReportUserCacheProxy m_stProxy;          //report_user_cache表的代理工具，简化对其字段的引用
};