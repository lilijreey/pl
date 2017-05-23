#pragma once
#include "session.h"
#include "my_include.h"
#include "global_serv.h"
#include "jsoncpp/json/json.h"

class UnpackUtil
{
public:
    // function ==> 解析上游请求参数
    static int GetRequestParam(const string strRequestBuf, ReportUserReq &stReportUserReq);
    static int GetRequestParamInJson(const string strRequestBuf, ReportUserReq &stReportUserReq);

    // function ==> 封装响应包
    static int GetResponseContent(const ReportUserRsp &stReportUserRsp, string &sResponseBuf);
    static int GetResponseContentInJson(const ReportUserReq &stReportUserReq, const ReportUserRsp &stReportUserRsp, string &sResponseBuf);

    static void GetVectorFromString(const char* pszBuf, const char ch, vector<string>& vecList);

private:
    // 检查JSON数据合法性
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
