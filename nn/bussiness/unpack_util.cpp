#include "unpack_util.h"

int UnpackUtil::GetRequestParam(const string strRequestBuf, ReportUserReq &stReportUserReq)
{
    
    int dwRetCode = 0;

    vector<string> vecParams;
    vector<string> vecSubParams;
    vecParams.clear();
    vecSubParams.clear();
    UnpackUtil::GetVectorFromString(strRequestBuf.c_str(), '\t', vecParams);
    if (vecParams.size() < 3)
    {
        dwRetCode = -1;
    }
    else
    {
        stReportUserReq.m_ddwProcessCmd = strtoul(vecParams[0].c_str(), NULL, 10);
        stReportUserReq.m_ddwKey = strtoll(vecParams[1].c_str(), NULL, 10);
        UnpackUtil::GetVectorFromString(vecParams[2].c_str(), ',', vecSubParams);
        TUINT32 udwExpectedParamNum = 0;

        switch (stReportUserReq.m_ddwProcessCmd)
        {
        case EN_REPORT_USER_OP__REPORT_GET:
            udwExpectedParamNum = 1;
            if (vecSubParams.size() < udwExpectedParamNum)
            {
                dwRetCode = -2;
            }
            else
            {
                stReportUserReq.m_udwCurPage = strtoul(vecSubParams[0].c_str(), NULL, 10);
            }
            break;
        case EN_REPORT_USER_OP__REPORT_DETAIL_GET:
            udwExpectedParamNum = 2;
            if (vecSubParams.size() < udwExpectedParamNum)
            {
                dwRetCode = -3;
            }
            else
            {
                stReportUserReq.m_ucReportType = atoi(vecSubParams[0].c_str());
                stReportUserReq.m_udwCurPage = strtoul(vecSubParams[1].c_str(), NULL, 10);
            }
            break;
        case EN_REPORT_USER_OP__REPORT_READ:
        case EN_REPORT_USER_OP__REPROT_DEL:
            udwExpectedParamNum = 1;
            if (vecSubParams.size() < udwExpectedParamNum)
            {
                dwRetCode = -4;
            }
            else
            {
                vector<string> vecRepParams;
                vecRepParams.clear();
                for (TUINT32 udwIdx = 0; udwIdx < vecSubParams.size(); udwIdx++)
                {
                    UnpackUtil::GetVectorFromString(vecSubParams[udwIdx].c_str(), ':', vecRepParams);
                    if (vecRepParams.size() < 2)
                    {
                        dwRetCode = -41;
                        break;
                    }
                    else
                    {
                        stReportUserReq.m_vecOpType.push_back(atoi(vecRepParams[0].c_str()));
                        stReportUserReq.m_vecRid.push_back(strtoll(vecRepParams[1].c_str(), NULL, 10));
                    }
                }
            }
            break;
        case EN_REPORT_USER_OP__ALLIANCE_REPORT_GET:
            udwExpectedParamNum = 3;
            if (vecSubParams.size() < udwExpectedParamNum)
            {
                dwRetCode = -5;
            }
            else
            {
                stReportUserReq.m_ucGetType = atoi(vecSubParams[0].c_str());
                stReportUserReq.m_udwCurPage = strtoul(vecSubParams[1].c_str(), NULL, 10);
                stReportUserReq.m_udwPerPage = strtoul(vecSubParams[2].c_str(), NULL, 10);
            }
            break;
        case EN_REPORT_USER_OP__OP_REPORT_GET:
            break;
        case EN_REPORT_USER_OP__REPORT_SEND:
            udwExpectedParamNum = 4;
            if (vecSubParams.size() < udwExpectedParamNum)
            {
                dwRetCode = -6;
            }
            else
            {
                stReportUserReq.m_ddwRid = strtoll(vecSubParams[0].c_str(), NULL, 10);
                stReportUserReq.m_ucReportType = atoi(vecSubParams[1].c_str());
                stReportUserReq.m_ucGetType = atoi(vecSubParams[2].c_str());
                stReportUserReq.m_ucStatus = atoi(vecSubParams[3].c_str());
            }
            break;
        case EN_REPORT_USER_OP__REPORT_MULTI_SEND:
        {
            SReportInfo objReport;
            udwExpectedParamNum = 4;
            for (TUINT32 udwIdx = 1; udwIdx < vecParams.size(); udwIdx+=2)
            {
                vecSubParams.clear();
                UnpackUtil::GetVectorFromString(vecParams[udwIdx + 1].c_str(), ',', vecSubParams);
                if (vecSubParams.size() < udwExpectedParamNum)
                {
                    dwRetCode = -7;
                    break;
                }
                objReport.Reset();
                objReport.m_nRid = strtoll(vecSubParams[0].c_str(), NULL, 10);
                objReport.m_nReport_type = atoi(vecSubParams[1].c_str());
                objReport.m_nType = atoi(vecSubParams[2].c_str());
                objReport.m_nStatus = atoi(vecSubParams[3].c_str());

                stReportUserReq.m_vecReports.push_back(objReport);
                stReportUserReq.m_vecKeys.push_back(strtoll(vecParams[udwIdx].c_str(), NULL, 10));
            }
            break;
        }
        case EN_REPORT_USER_OP__REPORT_SCAN_FOR_DEBUG:
            //do nothing
            break;
        default:
            dwRetCode = -7;
            break;
        }
    }


    return dwRetCode;
}

int UnpackUtil::GetRequestParamInJson(const string strRequestBuf, ReportUserReq &stReportUserReq)
{
    int dwRetCode = 0;

    Json::Value jsnRoot;
    jsnRoot.clear();

    Json::Reader jsnReader;
    if (!jsnReader.parse(strRequestBuf, jsnRoot))
    {
        return -1;
    }
    if (!jsnRoot.isObject())
    {
        return -1;
    }

    if (CheckHeader(jsnRoot))
    {
        stReportUserReq.m_ddwUid = jsnRoot["header"]["uid"].asInt64();
        stReportUserReq.m_ddwAid = jsnRoot["header"]["aid"].asInt64();
        stReportUserReq.m_ddwSid = jsnRoot["header"]["sid"].asInt64();
        stReportUserReq.m_ddwDid = jsnRoot["header"]["did"].asInt64();
    }
    else
    {
        return -2;
    }

    if (jsnRoot["request"]["report"]["cmd"].isString())
    {
        string sCmd = jsnRoot["request"]["report"]["cmd"].asCString();
        if ("report_get" == sCmd)
        {
            stReportUserReq.m_ddwProcessCmd = EN_REPORT_USER_OP__REPORT_GET;
        }
        else if ("report_detail_get" == sCmd)
        {
            stReportUserReq.m_ddwProcessCmd = EN_REPORT_USER_OP__REPORT_DETAIL_GET;
        }
        else if ("report_read" == sCmd)
        {
            stReportUserReq.m_ddwProcessCmd = EN_REPORT_USER_OP__REPORT_READ;
        }
        else if ("report_del" == sCmd)
        {
            stReportUserReq.m_ddwProcessCmd = EN_REPORT_USER_OP__REPROT_DEL;
        }
        else if ("alliance_report_get" == sCmd)
        {
            stReportUserReq.m_ddwProcessCmd = EN_REPORT_USER_OP__ALLIANCE_REPORT_GET;
        }
        else if ("report_send" == sCmd)
        {
            stReportUserReq.m_ddwProcessCmd = EN_REPORT_USER_OP__REPORT_SEND;
        }
        else if ("report_multi_send" == sCmd)
        {
            stReportUserReq.m_ddwProcessCmd = EN_REPORT_USER_OP__REPORT_MULTI_SEND;
        }
        else if ("mail_scan_for_debug" == sCmd)
        {
            stReportUserReq.m_ddwProcessCmd = EN_REPORT_USER_OP__REPORT_SCAN_FOR_DEBUG;
        }
        else
        {
            return -4;
        }
    }
    else
    {
        return -3;
    }

    Json::Value jsnParam;
    jsnParam.clear();
    if (jsnRoot["request"]["report"]["param"].isObject())
    {
        jsnParam = jsnRoot["request"]["report"]["param"];
    }
    else
    {
        return -5;
    }

    switch (stReportUserReq.m_ddwProcessCmd)
    {
    case EN_REPORT_USER_OP__REPORT_GET:
        if (!CheckReportGet(jsnParam))
        {
            dwRetCode = -6;
        }
        else
        {
            stReportUserReq.m_ddwKey = stReportUserReq.m_ddwUid;
            stReportUserReq.m_udwCurPage = jsnParam["cur_page"].asInt64();
        }
        break;
    case EN_REPORT_USER_OP__REPORT_DETAIL_GET:
        if (!CheckReportDetailGet(jsnParam))
        {
            dwRetCode = -7;
        }
        else
        {
            stReportUserReq.m_ddwKey = stReportUserReq.m_ddwUid;
            stReportUserReq.m_ucReportType = jsnParam["report_type"].asInt64();
            stReportUserReq.m_udwCurPage = jsnParam["cur_page"].asInt64();
        }
        break;
    case EN_REPORT_USER_OP__REPORT_READ:
    case EN_REPORT_USER_OP__REPROT_DEL:
        if (!CheckReportRead(jsnParam))
        {
            dwRetCode = -8;
        }
        else
        {
            stReportUserReq.m_ddwKey = stReportUserReq.m_ddwUid;

            for (TUINT32 udwIdx = 0; udwIdx < jsnParam["op_list"].size(); udwIdx++)
            {

                stReportUserReq.m_vecOpType.push_back(jsnParam["op_list"][udwIdx]["op_type"].asInt64());
                stReportUserReq.m_vecRid.push_back(jsnParam["op_list"][udwIdx]["rid"].asInt64());
            }
        }
        break;
    case EN_REPORT_USER_OP__ALLIANCE_REPORT_GET:
        if (!CheckAllianceReportGet(jsnParam))
        {
            dwRetCode = -9;
        }
        else
        {
            stReportUserReq.m_ddwKey = -stReportUserReq.m_ddwAid;
            stReportUserReq.m_ucGetType = jsnParam["get_type"].asInt64();
            stReportUserReq.m_udwCurPage = jsnParam["cur_page"].asInt64();
            stReportUserReq.m_udwPerPage = jsnParam["per_page"].asInt64();
        }
        break;
    case EN_REPORT_USER_OP__OP_REPORT_GET:
        break;
    case EN_REPORT_USER_OP__REPORT_SEND:
        if (!CheckReportSend(jsnParam))
        {
            dwRetCode = -10;
        }
        else
        {
            stReportUserReq.m_ddwKey = jsnParam["uid"].asInt64();
            stReportUserReq.m_ddwRid = jsnParam["rid"].asInt64();
            stReportUserReq.m_ucReportType = jsnParam["report_type"].asInt64();
            stReportUserReq.m_ucGetType = jsnParam["get_type"].asInt64();
            stReportUserReq.m_ucStatus = jsnParam["status"].asInt64();
        }
        break;
    case EN_REPORT_USER_OP__REPORT_MULTI_SEND:
    {
        if (!CheckReportMultiSend(jsnParam))
        {
            dwRetCode = -11;
        }
        else
        {
            SReportInfo objReport;
            for (TUINT32 udwIdx = 0; udwIdx < jsnParam["report_info"].size(); udwIdx++)
            {
                objReport.Reset();
                objReport.m_nRid = jsnParam["report_info"][udwIdx]["rid"].asInt64();
                objReport.m_nReport_type = jsnParam["report_info"][udwIdx]["report_type"].asInt64();
                objReport.m_nType = jsnParam["report_info"][udwIdx]["get_type"].asInt64();
                objReport.m_nStatus = jsnParam["report_info"][udwIdx]["status"].asInt64();

                stReportUserReq.m_vecReports.push_back(objReport);
                stReportUserReq.m_vecKeys.push_back(jsnParam["report_info"][udwIdx]["uid"].asInt64());
            }
        }
        break;
    }
    case EN_REPORT_USER_OP__REPORT_SCAN_FOR_DEBUG:
    {
        string sReportType("user");
        if (jsnParam["user_or_al"].isString())
        {
            sReportType = jsnParam["user_or_al"].asCString();
        }
        if ("user" == sReportType)
        {
            stReportUserReq.m_ddwKey = stReportUserReq.m_ddwUid;
        }
        else if ("al" == sReportType)
        {
            stReportUserReq.m_ddwKey = -stReportUserReq.m_ddwAid;
        }
        else
        {
            dwRetCode = -12;
        }
        break;
    }
    default:
        dwRetCode = -99;
        break;
    }


    return dwRetCode;
}

int UnpackUtil::GetResponseContent(const ReportUserRsp &stReportUserRsp, string &sResponseBuf)
{
    sResponseBuf.clear();
    //char szBuf[DEFAULT_PARAM_STR_LEN];
    //sprintf(szBuf,"%u,%u,%u,%lld", stReportUserRsp.m_udwReportTotalNum, 
    //    stReportUserRsp.m_udwReportUnreadNum, 
    //    stReportUserRsp.m_udwReportEntryNum,
    //    stReportUserRsp.m_ddwNewestRid);
    //sResponseBuf.append(szBuf);
    sResponseBuf.resize(sizeof(ReportUserRsp));
    memcpy((char*)sResponseBuf.c_str(), &stReportUserRsp, sizeof(ReportUserRsp));

    //TODO:to append the return entries;

    return 0;
}

int UnpackUtil::GetResponseContentInJson(const ReportUserReq &stReportUserReq, const ReportUserRsp &stReportUserRsp, string &sResponseBuf)
{
    Json::Value jsnRoot;
    jsnRoot.clear();

    jsnRoot["header"]["uid"] = stReportUserReq.m_ddwUid;
    jsnRoot["header"]["aid"] = stReportUserReq.m_ddwAid;
    jsnRoot["header"]["sid"] = stReportUserReq.m_ddwSid;
    jsnRoot["header"]["did"] = stReportUserReq.m_ddwDid;

    Json::Value jsnRsp;
    jsnRsp.clear();

    jsnRsp["report_total_num"] = stReportUserRsp.m_udwReportTotalNum;
    jsnRsp["report_unread_num"] = stReportUserRsp.m_udwReportUnreadNum;
    jsnRsp["report_newest_rid"] = stReportUserRsp.m_ddwNewestRid;

    for (TUINT32 udwIndex = 0; udwIndex < stReportUserRsp.m_udwReportEntryNum; udwIndex++)
    {
        jsnRsp["report_entry"][udwIndex]["rid"] = stReportUserRsp.m_aReportToReturn[udwIndex].ddwRid;
        jsnRsp["report_entry"][udwIndex]["unread_count"] = stReportUserRsp.m_aReportToReturn[udwIndex].dwUnread;
        jsnRsp["report_entry"][udwIndex]["report_type"] = stReportUserRsp.m_aReportToReturn[udwIndex].dwReportType;
        jsnRsp["report_entry"][udwIndex]["display_class"] = stReportUserRsp.m_aReportToReturn[udwIndex].ddwDisplayClass;
        jsnRsp["report_entry"][udwIndex]["time"] = stReportUserRsp.m_aReportToReturn[udwIndex].dwTime;
        jsnRsp["report_entry"][udwIndex]["status"] = stReportUserRsp.m_aReportToReturn[udwIndex].dwStatus;
        jsnRsp["report_entry"][udwIndex]["num"] = stReportUserRsp.m_aReportToReturn[udwIndex].dwNum;
    }

    jsnRoot["response"]["report"] = jsnRsp;

    Json::FastWriter jsnFastWriter;
    jsnFastWriter.omitEndingLineFeed();
    sResponseBuf = jsnFastWriter.write(jsnRoot);

    return 0;
}

void UnpackUtil::GetVectorFromString(const char* pszBuf, const char ch, vector<string>& vecList)
{
    vecList.clear();
    char* p = const_cast<char*>(pszBuf);
    char* q = strchr(p, ch);
    string sTmp;
    while (p && *p)
    {
        sTmp.clear();
        if (q)
        {
            sTmp.append(p, q - p);
        }
        else
        {
            sTmp.append(p);
        }
        vecList.push_back(sTmp);

        if (q)
        {
            p = q + 1;
            q = strchr(p, ch);
        }
        else
        {
            p = NULL;
        }
    };
}

bool UnpackUtil::CheckHeader(Json::Value &jsnRoot)
{
    bool bRet = jsnRoot["header"]["uid"].isInt64()
        && jsnRoot["header"]["aid"].isInt64()
        && jsnRoot["header"]["sid"].isInt64()
        && jsnRoot["header"]["did"].isInt64();

    return bRet;
}

bool UnpackUtil::CheckReportGet(Json::Value &jsnParam)
{
    bool bRet = jsnParam["cur_page"].isInt64();

    return bRet;
}

bool UnpackUtil::CheckReportDetailGet(Json::Value &jsnParam)
{
    bool bRet = jsnParam["report_type"].isInt64()
        && jsnParam["cur_page"].isInt64();

    return bRet;
}

bool UnpackUtil::CheckReportRead(Json::Value &jsnParam)
{
    bool bRet = jsnParam["op_list"].isArray();

    if (bRet)
    {
        bRet = (jsnParam["op_list"].size() > 0);
        for (unsigned int udwIdx = 0; udwIdx < jsnParam["op_list"].size(); udwIdx++)
        {
            if (!jsnParam["op_list"][udwIdx]["op_type"].isInt64() || !jsnParam["op_list"][udwIdx]["rid"].isInt64())
            {
                bRet = false;
                break;
            }
        }
    }

    return bRet;
}

bool UnpackUtil::CheckReportDel(Json::Value &jsnParam)
{
    return CheckReportRead(jsnParam);
}

bool UnpackUtil::CheckAllianceReportGet(Json::Value &jsnParam)
{
    bool bRet = jsnParam["get_type"].isInt64()
        && jsnParam["cur_page"].isInt64()
        && jsnParam["per_page"].isInt64();

    return bRet;
}

bool UnpackUtil::CheckReportSend(Json::Value &jsnParam)
{
    bool bRet = jsnParam["uid"].isInt64()
        && jsnParam["rid"].isInt64()
        && jsnParam["report_type"].isInt64()
        && jsnParam["get_type"].isInt64()
        && jsnParam["status"].isInt64();

    return bRet;
}

bool UnpackUtil::CheckReportMultiSend(Json::Value &jsnParam)
{
    bool bRet = (jsnParam["report_info"].size() > 0);
    for (unsigned int udwIdx = 0; udwIdx < jsnParam["report_info"].size(); udwIdx++)
    {
        if (!CheckReportSend(jsnParam["report_info"][udwIdx]))
        {
            bRet = false;
            break;
        }
    }

    return bRet;
}

bool UnpackUtil::CheckReportScanForDebug(Json::Value &jsnParam)
{
    return true;
}
