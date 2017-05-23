#pragma once
#include "session.h"
#include "my_include.h"
#include "global_serv.h"
#include "jsoncpp/json/json.h"

class UnpackUtil
{
public:
    // function ==> ���������������
    static int GetRequestParam(const string strRequestBuf, ReportUserReq &stReportUserReq);
    static int GetRequestParamInJson(const string strRequestBuf, ReportUserReq &stReportUserReq);

    // function ==> ��װ��Ӧ��
    static int GetResponseContent(const ReportUserRsp &stReportUserRsp, string &sResponseBuf);
    static int GetResponseContentInJson(const ReportUserReq &stReportUserReq, const ReportUserRsp &stReportUserRsp, string &sResponseBuf);

    static void GetVectorFromString(const char* pszBuf, const char ch, vector<string>& vecList);

private:
    // ���JSON���ݺϷ���
    static bool CheckHeader(Json::Value &jsnRoot);
    static bool CheckReportGet(Json::Value &jsnParam);
    static bool CheckReportDetailGet(Json::Value &jsnParam);
    static bool CheckReportRead(Json::Value &jsnParam);
    static bool CheckReportDel(Json::Value &jsnParam);
    static bool CheckAllianceReportGet(Json::Value &jsnParam);
    static bool CheckReportSend(Json::Value &jsnParam);
    static bool CheckReportMultiSend(Json::Value &jsnParam);
    static bool CheckReportScanForDebug(Json::Value &jsnParam);
};
