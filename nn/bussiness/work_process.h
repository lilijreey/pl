#pragma once
#include "session.h"
#include "cache_mgr.h"
#include "aws_table_report_user_cache.h"
#include "report_user_cache_proxy.h"

/*************************************************************************************
��̨�����߳�
**************************************************************************************/
class CWorkProcess
{
public:
    CWorkProcess();
    ~CWorkProcess();
    /*
    *    ��ʼ������
    */
    TINT32 Init(CConf *poConf, CTseLogger *poServLog, CTseLogger *poReqLog, CWorkQueue *poUpdateQue, CWritebackQueue *poWritebackQ,
        ILongConn *poQueryLongConn, ILongConn *poSearchLongConn);
    /*
    *    �߳���������
    */
    static TVOID * Start(TVOID *pParam);
    /*
    *    ����ѭ��
    */
    TINT32    WorkRoutine();

private:
    // ����ǰ������
    TINT32 ProcessDispatch(CWorkSession *poSession);
    TINT32 ProcessResponse(CWorkSession *poSession);
    TINT32 RequestWriteFile(CWorkSession* poSession);
    TINT32 RequestWriteback(CWorkSession *poSession);

    /***********************************report����ز���**************************************/
    TINT32 Processcmd_ReportPut(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 Processcmd_ReportMultiPut(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 ProcessCmd_ReportGet(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 ProcessCmd_ReportDetailGet(CWorkSession *pstSession, TBOOL &bNeedResponse);

    TINT32 ProcessCmd_ReportRead(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 ProcessCmd_ReportDel(CWorkSession *pstSession, TBOOL &bNeedResponse);
    TINT32 ProcessCmd_ReportScanForDebug(CWorkSession *pstSession, TBOOL &bNeedResponse);

    // function  ==> ��ȡ��ǰҳreport��id�б�
    TINT32 ProcessCmd_AllianceReportGet(CWorkSession *pstSession, TBOOL &bNeedResponse);

    // function  ==> ��ȡ��ǰҳreport��id�б�
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
    ILongConn	*m_poQueryLongConn;		//������������
    ILongConn	*m_poSearchLongConn;		//���γ�����
    CConf	*m_poConf;				    //����

    CTseLogger	*m_poLog;					//Log
    CTseLogger	*m_poReqLog;                 // ������־

    CWorkQueue	*m_poWorkQueue;         //���¶���
    CWritebackQueue *m_poWritebackQ;

    CBaseProtocolUnpack	*m_poUnpack;              // �������
    CBaseProtocolPack	*m_poPack;				// �������

    SReportUserCacheProxy m_stProxy;          //report_user_cache��Ĵ����ߣ��򻯶����ֶε�����
};