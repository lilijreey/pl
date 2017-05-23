#include "file_mgr.h"


CFileMgr *CFileMgr::m_poFileMgr = NULL;

CFileMgr * CFileMgr::GetInstance()
{

    if (NULL == m_poFileMgr)
    {
        try
        {
            m_poFileMgr = new CFileMgr;
        }
        catch (const std::bad_alloc &memExp)
        {
            assert(m_poFileMgr);
        }
    }
    return m_poFileMgr;

}

TINT32 CFileMgr::Init(CTseLogger *poLog)
{
    m_poServLog = poLog;

    /*
    // 活跃玩家和联盟列表
    m_setActivePlayer.clear();
    m_setActiveAlliance.clear();
    */

    for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
    {
        m_fpRawData[dwIdx] = NULL;
        m_dwRawDataEmpty[dwIdx] = 1;
    }
    m_fpRawDataOffset = NULL;
    m_dwRawDataShitCount = 0;
    m_udwRawDataCurFileIndex = 0;
    m_uddwRawDataCurFileRow = 0;

   
    /*
    if (0 != ActiveDataInit(DATA_FILE__ACTIVE_PLAYER, DATA_FILE__ACTIVE_ALLIANCE))
    {
        TSE_LOG_ERROR(m_poServLog, ("CFileMgr::Init, init player and alliance file failed!"));
        return -1;
    }
    */

    if (0 != RawDataInit(DATA_FILE__RAW_DATA_PRE, DATA_FILE__RAW_DATA_OFFSET, DATA_FILE__RAW_DATA_CUR_INFO))
    {
        TSE_LOG_ERROR(m_poServLog, ("CFileMgr::Init, init raw_data raw_data_offset raw_data_cur_info file failed!"));
        return -2;
    }


    // 恢复当前的读写的位置信息
    if (0 != ReadRawDataCurInfoFile())
    {
        TSE_LOG_ERROR(m_poServLog, ("CFileMgr::PreCreateCacheIndexFromOffset, ReadRawDataCurInfoFile failed!"));
        return -3;
    }


    // 获取当前已经完成的回写情况
    m_setFileOffset.clear();
    SRawDataOffsetInfo stOffset;
    stOffset.Reset();
    TINT32 dwRetCode = ReadRawDataOffsetFile(stOffset);
    if (dwRetCode < 0)
    {
        TSE_LOG_ERROR(m_poServLog, ("CFileMgr::PreCreateCacheIndexFromOffset, ReadRawDataOffsetFile failed with [%d]!",dwRetCode));
        return -4;
    }
    else
    {
        m_stExpectFileOffset.m_udwFileIndex = stOffset.m_udwFileIndex;
        m_stExpectFileOffset.m_uddwFileRow = stOffset.m_uddwFileRow;
        if (m_stExpectFileOffset.m_uddwFileRow < DATA_FILE__RAW_DATA_MAX_ROW)
        {
            m_stExpectFileOffset.m_uddwFileRow++;
        }
        else
        {
            m_stExpectFileOffset.m_uddwFileRow = 1;
            m_stExpectFileOffset.m_udwFileIndex = (m_stExpectFileOffset.m_udwFileIndex + 1) % DATA_FILE__RAW_DATA_INDEX;
        }
    }

   

    return 0;
}

TINT32 CFileMgr::RawDataInit(const string &strRawDataFile, const string &strRawDataOffsetFile, const string &strRawDataCurInfo)
{

    // 加载raw_data_offset文件
    m_fpRawDataOffset = fopen(strRawDataOffsetFile.c_str(), "at+");
    if (NULL == m_fpRawDataOffset)
    {
        return -1;
    }
    fclose(m_fpRawDataOffset);
    m_fpRawDataOffset = fopen(strRawDataOffsetFile.c_str(), "rt+");
    if (NULL == m_fpRawDataOffset)
    {
        return -1;
    }

    // 加载raw_data_cur_info文件
    m_fpRawDataCurInfo = fopen(strRawDataCurInfo.c_str(), "at+");
    if (NULL == m_fpRawDataCurInfo)
    {
        return -1;
    }
    fclose(m_fpRawDataCurInfo);
    m_fpRawDataCurInfo = fopen(strRawDataCurInfo.c_str(), "rt+");
    if (NULL == m_fpRawDataCurInfo)
    {
        return -1;
    }


    // 加载raw_data文件
    ostringstream ossRawDataFileName;
    for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
    {
        ossRawDataFileName.str("");
        ossRawDataFileName << strRawDataFile << "." << dwIdx;
        m_fpRawData[dwIdx] = fopen(ossRawDataFileName.str().c_str(), "at+");
        if (NULL == m_fpRawData[dwIdx])
        {
            return -2;
        }
        fclose(m_fpRawData[dwIdx]);
        m_fpRawData[dwIdx] = fopen(ossRawDataFileName.str().c_str(), "rt+");
        if (NULL == m_fpRawData[dwIdx])
        {
            return -2;
        }

    }


    return 0;

}



TINT32 CFileMgr::shiftFiles()
{
    /*
    SRawDataOffsetInfo stRawDataOffsetInfo;
    TUINT32 udwSize = 0;

    udwSize = ftell(m_fpRawData[m_dwRawDataCurFileIndex]);
    if (udwSize >= DATA_FILE__RAW_DATA_MAX_SIZE)
    {
        m_dwRawDataCurFileIndex = (m_dwRawDataCurFileIndex + 1) % DATA_FILE__RAW_DATA_INDEX;
        m_ddwRawDataCurFileRow = 0;

        stRawDataOffsetInfo.Reset();
        ReadRawDataOffsetFile(stRawDataOffsetInfo);
        ClearOldRawDataFile(stRawDataOffsetInfo);
        return 1;

    }
    */

    SRawDataOffsetInfo stRawDataOffsetInfo;
    m_udwRawDataCurFileIndex = (m_udwRawDataCurFileIndex + 1) % DATA_FILE__RAW_DATA_INDEX;
    m_uddwRawDataCurFileRow = 0;
    stRawDataOffsetInfo.Reset();
    ReadRawDataOffsetFile(stRawDataOffsetInfo);
    ClearOldRawDataFile(stRawDataOffsetInfo);

    return 0;
}



/*
TINT32 CFileMgr::UpdateRawDataOffset(const SRawDataOffsetInfo &stRawDataOffsetInfo)
{
    if (0 != WriteRawDataOffsetFile(stRawDataOffsetInfo))
    {
        return -1;
    }
    return 0;
}

TINT32 CFileMgr::UpdateRawData(const string &strUpdateInfo, SRawDataOffsetInfo &stCurRawDataOffsetInfo)
{
    if (0 != WriteRawDataFile(strUpdateInfo, stCurRawDataOffsetInfo))
    {
        return -1;
    }
    return 0;
}

*/

TINT32 CFileMgr::GetRawdataInfoFromString(const string &strRawdata, SRawDataInfo &stRawDataInfo)
{
    // todo::转换为raw_data结构
    stRawDataInfo.Reset();
    stRawDataInfo.m_strRawData = strRawdata;
    return 0;
}

TINT32 CFileMgr::GetRawdataOffsetFromString(const string &strRawdataOffset, SRawDataOffsetInfo &stRawDataOffsetInfo)
{
    // todo::转换为raw_data_offset结构
    return 0;
}


TINT32 CFileMgr::GetRawdataCurInfoFromString(const string &strRawdataCurInfo, SRawDataCurInfo &stRawDataCurInfo)
{
    // todo::转换为raw_data_offset结构
    return 0;
}





TINT32 CFileMgr::WriteRawDataFile(const string &strRawDataString, SRawDataOffsetInfo &stCurRawDataOffsetInfo)
{
    TINT32 dwRet = 0;
    TCHAR szLine[512];
    SRawDataOffsetInfo stRawDataOffsetInfo;
    SRawDataCurInfo stRawDataCurInfo;
    /*
    TSE_LOG_DEBUG(m_poServLog, ("CFileMgr::WriteRawDataFile, strRawDataString=%s", \
    strRawDataString.c_str()));
    */
    m_oRawData.lock();

    /*
    if (m_dwRawDataShitCount >= DATA_FILE__RAW_DATA_SHIFT_COUNT)
    {
        TINT32 dwRetShift = 0;
        dwRetShift = shiftFiles();
        TSE_LOG_DEBUG(m_poServLog, ("CFileMgr::WriteRawDataFile, shiftFiles [curindex=%d] [dwRetShift=%d]", \
            m_dwRawDataCurFileIndex, \
            dwRetShift));
        m_dwRawDataShitCount = 0;
    }
    */


    stRawDataOffsetInfo.Reset();
    ReadRawDataOffsetFile(stRawDataOffsetInfo);
    if (m_uddwRawDataCurFileRow == DATA_FILE__RAW_DATA_MAX_ROW
        && stRawDataOffsetInfo.m_udwFileIndex == (((m_udwRawDataCurFileIndex + 1) % DATA_FILE__RAW_DATA_INDEX) + 1) % DATA_FILE__RAW_DATA_INDEX)
    {
        dwRet = -1;
        TSE_LOG_ERROR(m_poServLog, ("CFileMgr::WriteRawDataFile, disk can not save new request [curindex=%d] [currow=%d] [offsetindex=%d] [offsetrow=%d]", \
            m_udwRawDataCurFileIndex, \
            m_uddwRawDataCurFileRow, \
            stRawDataOffsetInfo.m_udwFileIndex, \
            stRawDataOffsetInfo.m_uddwFileRow));

        stCurRawDataOffsetInfo.Reset();
        stCurRawDataOffsetInfo.m_udwFileIndex = m_udwRawDataCurFileIndex;
        stCurRawDataOffsetInfo.m_uddwFileRow = m_uddwRawDataCurFileRow;


        m_oRawData.unlock();
        return dwRet;
    }
    if (m_uddwRawDataCurFileRow == DATA_FILE__RAW_DATA_MAX_ROW)
    {
        shiftFiles();
    }
    

    /*
    stRawDataOffsetInfo.Reset();
    ReadRawDataOffsetFile(stRawDataOffsetInfo);
    TINT32 dwAllFileFull = 1;
    for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
    {
        dwAllFileFull += m_dwRawDataEmpty[dwIdx];
    }
    TINT32 dwCurRow = m_ddwRawDataCurFileRow;
    if (0 == dwCurRow)
    {
        dwCurRow = 1;
    }
    if (m_dwRawDataCurFileIndex == stRawDataOffsetInfo.m_dwFileIndex
        && dwCurRow <= stRawDataOffsetInfo.m_ddwFileRow
        && 1 == dwAllFileFull)
    {
        dwRet = -2;
    }
    if (-2 == dwRet)
    {
        TSE_LOG_ERROR(m_poServLog, ("CFileMgr::WriteRawDataFile, disk can not save new request [curindex=%d] [currow=%d] [offsetindex=%d] [offsetrow=%d]", \
            m_dwRawDataCurFileIndex, \
            m_ddwRawDataCurFileRow, \
            stRawDataOffsetInfo.m_dwFileIndex, \
            stRawDataOffsetInfo.m_ddwFileRow));

        stCurRawDataOffsetInfo.Reset();
        stCurRawDataOffsetInfo.m_dwFileIndex = m_dwRawDataCurFileIndex;
        stCurRawDataOffsetInfo.m_ddwFileRow = m_ddwRawDataCurFileRow;


        m_oRawData.unlock();
        return dwRet;
    }
    */


    fseek(m_fpRawData[m_udwRawDataCurFileIndex], 0, SEEK_END);

    fputs(strRawDataString.c_str(), m_fpRawData[m_udwRawDataCurFileIndex]);

    if ('\n' != szLine[strlen(szLine) - 1])
    {
        fputs("\n", m_fpRawData[m_udwRawDataCurFileIndex]);
    }
    fflush(m_fpRawData[m_udwRawDataCurFileIndex]);
    if (1 == m_dwRawDataEmpty[m_udwRawDataCurFileIndex])
    {
        m_dwRawDataEmpty[m_udwRawDataCurFileIndex] = 0;
    }
    //++m_dwRawDataShitCount;
    ++m_uddwRawDataCurFileRow;


    
    stRawDataCurInfo.Reset();
    stRawDataCurInfo.m_dwRawDataShiftCount = m_dwRawDataShitCount;
    stRawDataCurInfo.m_udwRawDataCurFileIndex = m_udwRawDataCurFileIndex;
    stRawDataCurInfo.m_uddwRawDataCurFileRow = m_uddwRawDataCurFileRow;
    for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
    {
        stRawDataCurInfo.m_dwRawDataEmpty[dwIdx] = m_dwRawDataEmpty[dwIdx];
    }

    WriteRawDataCurInfoFile(stRawDataCurInfo);

    stCurRawDataOffsetInfo.Reset();
    stCurRawDataOffsetInfo.m_udwFileIndex = m_udwRawDataCurFileIndex;
    stCurRawDataOffsetInfo.m_uddwFileRow = m_uddwRawDataCurFileRow;

    m_oRawData.unlock();
    return 0;
}

TINT32 CFileMgr::ReadRawDataFile(const SRawDataOffsetInfo &stRawDataOffsetInfo, vector<SRawDataInfo> &vecRawDataInfo)
{
    vector<TUINT32> vecInitFileOrder;
    SRawDataInfo stRawDataInfo;
    TCHAR szLine[512]; 
    TUINT64 uddwCurRow = 0;

    m_oRawData.lock();

    if (stRawDataOffsetInfo.m_udwFileIndex == m_udwRawDataCurFileIndex
        && 0 == m_udwRawDataCurFileIndex
        && m_uddwRawDataCurFileRow == stRawDataOffsetInfo.m_uddwFileRow
        && 0 == m_uddwRawDataCurFileRow)
    {
        m_oRawData.unlock();
        return 0;
    }

    // todo
    /*
    if (stRawDataOffsetInfo.m_dwFileIndex == m_dwRawDataCurFileIndex
        && m_ddwRawDataCurFileRow >= stRawDataOffsetInfo.m_ddwFileRow)
    {
        m_oRawData.unlock();
        return -3;
    }
    */

    if (stRawDataOffsetInfo.m_udwFileIndex == m_udwRawDataCurFileIndex)
    {
        if (m_uddwRawDataCurFileRow > stRawDataOffsetInfo.m_uddwFileRow)
        {
            vecInitFileOrder.push_back(m_udwRawDataCurFileIndex);
        }
        else if (m_uddwRawDataCurFileRow < stRawDataOffsetInfo.m_uddwFileRow)
        {
            for (TUINT32 udwIdx = stRawDataOffsetInfo.m_udwFileIndex; udwIdx < DATA_FILE__RAW_DATA_INDEX; ++udwIdx)
            {
                vecInitFileOrder.push_back(udwIdx);
            }
            for (TUINT32 udwIdx = 0; udwIdx <= m_udwRawDataCurFileIndex; ++udwIdx)
            {
                vecInitFileOrder.push_back(udwIdx);
            }
        }
        else
        {
            // do noting
        }
    }
    else if (stRawDataOffsetInfo.m_udwFileIndex < m_udwRawDataCurFileIndex)
    {
        for (TUINT32 udwIdx = stRawDataOffsetInfo.m_udwFileIndex; udwIdx <= m_udwRawDataCurFileIndex; ++udwIdx)
        {
            vecInitFileOrder.push_back(udwIdx);
        }
    }
    else
    {
        for (TUINT32 udwIdx = stRawDataOffsetInfo.m_udwFileIndex; udwIdx < DATA_FILE__RAW_DATA_INDEX; ++udwIdx)
        {
            vecInitFileOrder.push_back(udwIdx);
        }
        for (TUINT32 udwIdx = 0; udwIdx <= m_udwRawDataCurFileIndex; ++udwIdx)
        {
            vecInitFileOrder.push_back(udwIdx);
        }
    }

    for (TUINT32 udwIdx = 0; udwIdx < vecInitFileOrder.size(); ++udwIdx)
    {
        stRawDataInfo.Reset();

        while (fgets(szLine, 512, m_fpRawData[vecInitFileOrder[udwIdx]]))
        {
            for (TUINT32 udwIdy = 0; udwIdy < 512 && szLine[udwIdy] != '\0';udwIdy++)
            {
                if (szLine[udwIdy] == '\n')
                {
                    szLine[udwIdy] = '\0';
                    break;
                }
            }
            ++uddwCurRow;

            if (1 == vecInitFileOrder.size())
            {
                if (stRawDataOffsetInfo.m_uddwFileRow <= uddwCurRow)
                {
                    GetRawdataInfoFromString(szLine, stRawDataInfo);
                    stRawDataInfo.m_udwFileIndex = vecInitFileOrder[udwIdx];
                    stRawDataInfo.m_uddwFileRow = uddwCurRow;
                    vecRawDataInfo.push_back(stRawDataInfo);
                }
            }
            else
            {
                if (0 == udwIdx
                    && stRawDataOffsetInfo.m_uddwFileRow <= uddwCurRow)
                {
                    GetRawdataInfoFromString(szLine, stRawDataInfo);
                    stRawDataInfo.m_udwFileIndex = vecInitFileOrder[udwIdx];
                    stRawDataInfo.m_uddwFileRow = uddwCurRow;
                    vecRawDataInfo.push_back(stRawDataInfo);
                }
                else if (vecInitFileOrder.size() - 1 == udwIdx
                    && m_uddwRawDataCurFileRow >= uddwCurRow)
                {
                    GetRawdataInfoFromString(szLine, stRawDataInfo);
                    stRawDataInfo.m_udwFileIndex = vecInitFileOrder[udwIdx];
                    stRawDataInfo.m_uddwFileRow = uddwCurRow;
                    vecRawDataInfo.push_back(stRawDataInfo);
                }
                else
                {
                    GetRawdataInfoFromString(szLine, stRawDataInfo);
                    stRawDataInfo.m_udwFileIndex = vecInitFileOrder[udwIdx];
                    stRawDataInfo.m_uddwFileRow = uddwCurRow;
                    vecRawDataInfo.push_back(stRawDataInfo);
                }
            }
        }
        uddwCurRow = 0;
    }

    m_oRawData.unlock();
    return 0;
}



TINT32 CFileMgr::WriteRawDataOffsetFile(const SRawDataOffsetInfo &stRawDataOffsetInfo)
{
    TINT32 dwRet = 0;
    TCHAR szLine[512];
    //snprintf(szLine, 512, "%u\t%lu", stRawDataOffsetInfo.m_udwFileIndex, stRawDataOffsetInfo.m_uddwFileRow);
    TUINT32 udwRealFileIndex;
    TUINT64 udwRealFileRow;
    TBOOL bFound = FALSE;

    TSE_LOG_DEBUG(m_poServLog, ("CFileMgr::WriteRawDataOffsetFile, expected[%u,%lu],now[%u,%lu]", 
        m_stExpectFileOffset.m_udwFileIndex,m_stExpectFileOffset.m_uddwFileRow,stRawDataOffsetInfo.m_udwFileIndex,stRawDataOffsetInfo.m_uddwFileRow));
    set<SRawDataOffsetInfo>::iterator it;

    m_oRawDataOffset.lock();

    m_setFileOffset.insert(stRawDataOffsetInfo);

    for (it = m_setFileOffset.find(m_stExpectFileOffset); it != m_setFileOffset.end(); it = m_setFileOffset.find(m_stExpectFileOffset))
    {
        bFound = TRUE;
        m_setFileOffset.erase(it);
        udwRealFileIndex = m_stExpectFileOffset.m_udwFileIndex;
        udwRealFileRow = m_stExpectFileOffset.m_uddwFileRow;

        if (m_stExpectFileOffset.m_uddwFileRow < DATA_FILE__RAW_DATA_MAX_ROW)
        {
            m_stExpectFileOffset.m_uddwFileRow++;
        }
        else
        {
            m_stExpectFileOffset.m_uddwFileRow = 1;
            m_stExpectFileOffset.m_udwFileIndex = (m_stExpectFileOffset.m_udwFileIndex + 1) % DATA_FILE__RAW_DATA_INDEX;
        }
    }

    if (bFound)
    {
        snprintf(szLine, 512, "%u\t%lu", udwRealFileIndex, udwRealFileRow);

        dwRet = ftruncate(fileno(m_fpRawDataOffset), 0);
        if (-1 == dwRet)
        {
            TSE_LOG_ERROR(m_poServLog, ("CFileMgr::WriteRawDataOffsetFile, ftruncate failed! Error[%s]", strerror(errno)));
        }

        rewind(m_fpRawDataOffset);
        fputs(szLine, m_fpRawDataOffset);

        fflush(m_fpRawDataOffset);
    }
    
    m_oRawDataOffset.unlock();

    return 0;
}

TINT32 CFileMgr::ReadRawDataOffsetFile(SRawDataOffsetInfo &stRawDataOffsetInfo)
{
    TINT32 dwRet = 0;
    TUINT32 udwRowCount = 0;
    TCHAR *pCur = NULL;
    TCHAR szLine[512];
    TUINT32 udwFileIndex = 0;
    TUINT64 uddwFileRow = 0;

    m_oRawDataOffset.lock();
    rewind(m_fpRawDataOffset);
    while (fgets(szLine, 512, m_fpRawDataOffset))
    {
        //pCur = strtok(szLine, "\t\r\n");
        udwFileIndex = strtoul(szLine, NULL, 10);


        pCur = strchr(szLine, '\t');
        if (pCur == NULL)
        {
            dwRet = -1;
        }
        else
        {
            pCur++;
            uddwFileRow = strtoul(pCur, NULL, 10);

            if (0 >= uddwFileRow)
            {
                dwRet = -1;
            }
        }
        ++udwRowCount;
    }

    if (-1 == dwRet)
    {
        // do nothing
    }
    else if (1 < udwRowCount)
    {
        dwRet = - 2;
    }
    else
    {
        stRawDataOffsetInfo.Reset();
        stRawDataOffsetInfo.m_udwFileIndex = udwFileIndex;
        stRawDataOffsetInfo.m_uddwFileRow = uddwFileRow;
    }



    m_oRawDataOffset.unlock();

    return dwRet;
}




TINT32 CFileMgr::WriteRawDataCurInfoFile(SRawDataCurInfo &stRawDataCurInfo)
{
    TINT32 dwRet = 0;
    TCHAR szLine[512];
    TCHAR *pCur = szLine;
    TUINT32 udwCurLen = 0;
    udwCurLen = snprintf(pCur, 512, "%u\t%lu\t%d", \
                         stRawDataCurInfo.m_udwRawDataCurFileIndex, \
                         stRawDataCurInfo.m_uddwRawDataCurFileRow, \
                         stRawDataCurInfo.m_dwRawDataShiftCount);
    pCur += udwCurLen;

    for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
    {
        udwCurLen = snprintf(pCur, 512 - (pCur - szLine), "\t%d", stRawDataCurInfo.m_dwRawDataEmpty[dwIdx]);
        pCur += udwCurLen;
    }


    m_oRawDataCurInfo.lock();

    dwRet = ftruncate(fileno(m_fpRawDataCurInfo), 0);
    if (-1 == dwRet)
    {
        TSE_LOG_ERROR(m_poServLog, ("CFileMgr::WriteRawDataCurInfoFile, ftruncate failed! Error[%s]", strerror(errno)));
    }
    rewind(m_fpRawDataCurInfo);
    fputs(szLine, m_fpRawDataCurInfo);

    /*
    if ('\0' != szLine[strlen(szLine) - 1])
    {
        fputs("\0", m_fpRawDataCurInfo);
    }
    */

    fflush(m_fpRawDataCurInfo);
    m_oRawDataCurInfo.unlock();

    return 0;
}

TINT32 CFileMgr::ReadRawDataCurInfoFile()
{
    TINT32 dwRet = 0;
    TUINT32 udwRowCount = 0;
    TCHAR *pCur = NULL;
    TCHAR szLine[512];
    TUINT32 udwFileIndex = 0;
    TUINT64 uddwFileRow = 0;
    TINT32 dwShiftCount = 0;
    TINT32 dwRawDataEmpty[DATA_FILE__RAW_DATA_INDEX];
    for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
    {
        dwRawDataEmpty[dwIdx] = 1;
    }
    

    m_oRawDataCurInfo.lock();
    rewind(m_fpRawDataCurInfo);
    while (fgets(szLine, 512, m_fpRawDataCurInfo))
    {
        //pCur = strtok(szLine, "\t\r\n");
        udwFileIndex = strtoul(szLine, NULL, 10);

        pCur = strchr(szLine, '\t');
        pCur++;
        uddwFileRow = strtoul(pCur, NULL, 10);

        pCur = strchr(pCur, '\t');
        pCur++;
        dwShiftCount = atoi(pCur);


        for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
        {
            pCur = strchr(pCur, '\t');
            pCur++;
            dwRawDataEmpty[dwIdx] = atoi(pCur);
        }


        if (0 >= uddwFileRow)
        {
            dwRet = -1;
        }
        ++udwRowCount;
    }

    if (-1 == dwRet)
    {
        // do nothing
    }
    else if (1 < udwRowCount)
    {
        dwRet = -2;
    }
    else
    {
        m_oRawData.lock();
        m_udwRawDataCurFileIndex = udwFileIndex;
        m_uddwRawDataCurFileRow = uddwFileRow;
        m_dwRawDataShitCount = dwShiftCount;
        for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
        {
            m_dwRawDataEmpty[dwIdx] = dwRawDataEmpty[dwIdx];
        }
        m_oRawData.unlock();
    }


    m_oRawDataCurInfo.unlock();

    return dwRet;


}



TINT32 CFileMgr::ClearOldRawDataFile(SRawDataOffsetInfo &stCurRawDataOffsetInfo)
{
    ostringstream ossRawDataFileName;
    if (stCurRawDataOffsetInfo.m_udwFileIndex == m_udwRawDataCurFileIndex)
    {
        // 在同一个文件里增长的
        return 1;
    }


    if (0 == stCurRawDataOffsetInfo.m_udwFileIndex)
    {
        if (m_udwRawDataCurFileIndex != DATA_FILE__RAW_DATA_INDEX - 1
            && 0 == m_dwRawDataEmpty[DATA_FILE__RAW_DATA_INDEX - 1])
        {
            fclose(m_fpRawData[DATA_FILE__RAW_DATA_INDEX - 1]);
            ossRawDataFileName.str("");
            ossRawDataFileName << DATA_FILE__RAW_DATA_PRE << "." << DATA_FILE__RAW_DATA_INDEX - 1;
            remove(ossRawDataFileName.str().c_str());
            m_fpRawData[DATA_FILE__RAW_DATA_INDEX - 1] = fopen(ossRawDataFileName.str().c_str(), "wt+");
            if (NULL == m_fpRawData[DATA_FILE__RAW_DATA_INDEX - 1])
            {
                return -1;
            }
        }
    }
    else if (stCurRawDataOffsetInfo.m_udwFileIndex - 1 != m_udwRawDataCurFileIndex)
    {
        if (0 == m_dwRawDataEmpty[stCurRawDataOffsetInfo.m_udwFileIndex - 1])
        {
            fclose(m_fpRawData[stCurRawDataOffsetInfo.m_udwFileIndex - 1]);
            ossRawDataFileName.str("");
            ossRawDataFileName << DATA_FILE__RAW_DATA_PRE << "." << stCurRawDataOffsetInfo.m_udwFileIndex - 1;
            remove(ossRawDataFileName.str().c_str());
            m_fpRawData[stCurRawDataOffsetInfo.m_udwFileIndex - 1] = fopen(ossRawDataFileName.str().c_str(), "wt+");
            if (NULL == m_fpRawData[stCurRawDataOffsetInfo.m_udwFileIndex - 1])
            {
                return -2;
            }
        }
    }
    else
    {
         // do nothing
    }
    return 0;
}

