#pragma once
#include "base/log/wtselogger.h"
#include "session.h"
#include "conf.h"

using namespace wtse::log;

/*************************************************************************************
Session ����
1.����ģʽ
2.��ҵ��so��ʼ��ҵ��
3.��ҵ��so���غͳ�ʼ��Session,����װ��SessionWrapper
4.�ṩȡSessionWrapper���ͻ���SessionWrapper�Ĺ���
5.�ṩ����ʱ�滻ҵ��so�Ĺ���
**************************************************************************************/
class CWritebackSessionMgr
{
private:
    static CWritebackSessionMgr                * m_poCSessionMgr;
    CWritebackSessionMgr();
public:
    ~CWritebackSessionMgr();

    /*
    *    ��ȡSessionMgr����
    *    \return
    *        S_OK �ɹ���S_FAIL ʧ��
    */
    static CWritebackSessionMgr    * Instance();

    /*
    *    ��ʼ��
    *    \return
    *        S_OK �ɹ���S_FAIL ʧ��
    */
    HRESULT    Init(CConf * poConf, CTseLogger *poLog);

    /*
    *    ����ʼ�����ͷ���Դ
    *    \return
    *        S_OK �ɹ���S_FAIL ʧ��
    */
    HRESULT    Uninit();

    /*
    *    ȡSessionWrapper�����޵ȴ�
    *    \return
    *        S_OK �ɹ���S_FAIL ʧ��
    */
    HRESULT    WaitTillSession(CWritebackSession **ppoSessionWrapper);

    /*
    *    ȡSessionWrapper���г�ʱ����
    *    \return
    *        S_OK �ɹ���S_FAIL ʧ��
    */
    HRESULT    WaitTimeSession(CWritebackSession **ppoSessionWrapper);

    /*
    *    ����SessionWrapper
    *    \return
    *        S_OK �ɹ���S_FAIL ʧ��
    */
    HRESULT    ReleaseSesion(CWritebackSession *poSessionWrapper);

private:

    CWritebackQueue                 *m_poMemQue;                        // �������
    CTseLogger                      *m_poLog;                            // ��־����

    TUINT32                         m_udwSessionNum;                    // Session�ܸ���
    CWritebackSession               *m_apoSessionWrapprList;            // ��װ���ڴ��б�
    TBOOL volatile                  m_bReady;                            // �Ƿ�����ṩ�������
};