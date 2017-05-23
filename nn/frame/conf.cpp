#include "conf.h"

CConf::CConf()
{
	// do nothing
}

CConf::~CConf()
{
	// do nothing
}

//初始化配置文件
TINT32 CConf::Init(const TCHAR *pszServFile, const TCHAR *pszModuleFile, const TCHAR *pszConfFile)
{
    CTseIniConfiger objConfig;
    bool bConf = FALSE;

	bConf = objConfig.LoadConfig(pszServFile);
	assert(bConf == TRUE);

    bConf = objConfig.GetValue("SERV_INFO", "module_ip", m_szServerIp);
    assert(bConf == true);
    bConf = objConfig.GetValue("SERV_INFO", "module_name", m_szModuleName);
    assert(bConf == true);

    bConf = objConfig.LoadConfig(pszModuleFile);
    assert(bConf == TRUE);

    bConf = objConfig.GetValue("PROJECT_INFO", "project", m_szProject);
    assert(bConf == true);
    bConf = objConfig.GetValue("PROJECT_INFO", "tbxml_project", m_szTblProject);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_query_port", m_uwServerPort);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_search_port", m_uwRegPort);
    assert(bConf == true);


    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WorkQueueSize", m_udwWorkQueSize);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WorkThreadNum", m_udwWorkThreadNum);
    assert(bConf == true);


    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WritebackQueueSize", m_udwWritebackQueSize);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WritebackQueInitSize", m_udwWritebackQueInitSize);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WritebackQueMaxSize", m_udwWritebackQueMaxSize);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WritebackThreadNum", m_udwWritebackThreadNum);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WritebackInterval", m_udwWritebackInterval);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WritebackSpeed", m_udwWritebackSpeed);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WritebackSpeedFactor", m_udwWritebackSpeedFactor);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_MaxRetryTime", m_udwMaxRetryTime);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_WritebackDelay", m_udwWritebackDelay);
    assert(bConf == true);


    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_CacheThreshold", m_udwCacheThreshold);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_CacheLimit", m_udwCacheLimit);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_CacheLRULoopNum", m_udwCacheLRULoopNum);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_IdleItemMaxSize", m_udwIdleItemMaxSize);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_IdleItemInitSize", m_udwIdleItemInitSize);
    assert(bConf == true);
    bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_IdleItemSizeFactor", m_udwIdleItemSizeFactor);
    assert(bConf == true);


	// stat
    std::string tmp = m_szProject;
    tmp += m_szModuleName;
    memcpy(m_project_name, tmp.c_str(), strlen(tmp.c_str()) + 1);
	bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_IntervalTime", m_stat_interval_time);
	assert(bConf == true);
	bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_NeedSendMessage", m_need_send_message);
	assert(bConf == true);
	bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_ErrNumThreshold", m_error_num_threshold);
	assert(bConf == true);
	bConf = objConfig.GetValue("REPORT_CENTER_INFO", "report_center_ErrRateThreashold", m_error_rate_threshold);
	assert(bConf == true);

    bConf = objConfig.LoadConfig(pszConfFile);
    assert(bConf == TRUE);

    //...do nothing

    return 0;
}

