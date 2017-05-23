#pragma once
#include "aws_table_report_user_cache.h"

struct SReportUserCacheProxy
{
    SReportInfo* m_pTmp[100];


    TVOID Reset()
    {
        memset(this, 0, sizeof(*this));
    }

    SReportUserCacheProxy()
    {
        Reset();
    }

    TVOID Init(TbReport_user_cache *poReportUserCache)
    {
        if (poReportUserCache == NULL)
        {
            return;
        }

        m_pTmp[0] = &poReportUserCache->m_bIdx0[0];
        m_pTmp[1] = &poReportUserCache->m_bIdx1[0];
        m_pTmp[2] = &poReportUserCache->m_bIdx2[0];
        m_pTmp[3] = &poReportUserCache->m_bIdx3[0];
        m_pTmp[4] = &poReportUserCache->m_bIdx4[0];
        m_pTmp[5] = &poReportUserCache->m_bIdx5[0];
        m_pTmp[6] = &poReportUserCache->m_bIdx6[0];
        m_pTmp[7] = &poReportUserCache->m_bIdx7[0];
        m_pTmp[8] = &poReportUserCache->m_bIdx8[0];
        m_pTmp[9] = &poReportUserCache->m_bIdx9[0];
        m_pTmp[10] = &poReportUserCache->m_bIdx10[0];
        m_pTmp[11] = &poReportUserCache->m_bIdx11[0];
        m_pTmp[12] = &poReportUserCache->m_bIdx12[0];
        m_pTmp[13] = &poReportUserCache->m_bIdx13[0];
        m_pTmp[14] = &poReportUserCache->m_bIdx14[0];
        m_pTmp[15] = &poReportUserCache->m_bIdx15[0];
        m_pTmp[16] = &poReportUserCache->m_bIdx16[0];
        m_pTmp[17] = &poReportUserCache->m_bIdx17[0];
        m_pTmp[18] = &poReportUserCache->m_bIdx18[0];
        m_pTmp[19] = &poReportUserCache->m_bIdx19[0];
        m_pTmp[20] = &poReportUserCache->m_bIdx20[0];
        m_pTmp[21] = &poReportUserCache->m_bIdx21[0];
        m_pTmp[22] = &poReportUserCache->m_bIdx22[0];
        m_pTmp[23] = &poReportUserCache->m_bIdx23[0];
        m_pTmp[24] = &poReportUserCache->m_bIdx24[0];
        m_pTmp[25] = &poReportUserCache->m_bIdx25[0];
        m_pTmp[26] = &poReportUserCache->m_bIdx26[0];
        m_pTmp[27] = &poReportUserCache->m_bIdx27[0];
        m_pTmp[28] = &poReportUserCache->m_bIdx28[0];
        m_pTmp[29] = &poReportUserCache->m_bIdx29[0];
        m_pTmp[30] = &poReportUserCache->m_bIdx30[0];
        m_pTmp[31] = &poReportUserCache->m_bIdx31[0];
        m_pTmp[32] = &poReportUserCache->m_bIdx32[0];
        m_pTmp[33] = &poReportUserCache->m_bIdx33[0];
        m_pTmp[34] = &poReportUserCache->m_bIdx34[0];
        m_pTmp[35] = &poReportUserCache->m_bIdx35[0];
        m_pTmp[36] = &poReportUserCache->m_bIdx36[0];
        m_pTmp[37] = &poReportUserCache->m_bIdx37[0];
        m_pTmp[38] = &poReportUserCache->m_bIdx38[0];
        m_pTmp[39] = &poReportUserCache->m_bIdx39[0];
        m_pTmp[40] = &poReportUserCache->m_bIdx40[0];
        m_pTmp[41] = &poReportUserCache->m_bIdx41[0];
        m_pTmp[42] = &poReportUserCache->m_bIdx42[0];
        m_pTmp[43] = &poReportUserCache->m_bIdx43[0];
        m_pTmp[44] = &poReportUserCache->m_bIdx44[0];
        m_pTmp[45] = &poReportUserCache->m_bIdx45[0];
        m_pTmp[46] = &poReportUserCache->m_bIdx46[0];
        m_pTmp[47] = &poReportUserCache->m_bIdx47[0];
        m_pTmp[48] = &poReportUserCache->m_bIdx48[0];
        m_pTmp[49] = &poReportUserCache->m_bIdx49[0];
    }


    SReportInfo* GetReportByIndex(TUINT32 udwIdx)
    {
        return m_pTmp[udwIdx];
    }

    TUINT32 GetReportFieldByIndex(TUINT32 udwIdx)
    {
        return TbREPORT_USER_CACHE_FIELD_IDX0 + udwIdx;
    }

};
