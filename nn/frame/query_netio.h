#ifndef _QUERY_NET_IO_H_
#define _QUERY_NET_IO_H_

#include "my_include.h"
#include "session.h"
#include "conf.h"

/*************************************************************************************
    ǰ����������IO
    1.������������
    2.��ȡSessionWrapper�����빤������
**************************************************************************************/
class CQueryNetIO : public ITasksGroupCallBack
{
public :
    CQueryNetIO();
    ~CQueryNetIO();

    /*
     *    ��ʼ������
     */
    TINT32 Init(CConf *poConf, CTseLogger *poLog, CWorkQueue *poWorkQueue, CWritebackQueue *m_poDelayTaskQueue);

    /*
     *    ����ʼ������
     */
    TINT32 Uninit();

public :

    /*
     *    ����IO�߳���������
     */
    static TVOID * RoutineNetIO(TVOID *pParam);

    /*
     *    ��ʼ�������
     */
    TVOID StartNetServ();

    /*
     *    ֹͣ�������
     */
    TVOID StopNetServ();

    
    /*
     *    ������Ϣ�ص�
     */
    virtual TVOID OnTasksFinishedCallBack(LTasksGroup *pstTasksGrp);

    /*
     *    ������Ϣ����
     */
    virtual TVOID OnUserRequest(LongConnHandle stHandle, const TUCHAR *pszData, TUINT32 udwDataLen, TINT32 &dwWillResponse);

     /*
     *  �رճ����ӣ�����ʱ����
     *    ��Ҫ������������ṩ�������񣬵�����֧�ָ��£�
     *   ��������������ʱ��
     */
    TVOID StopLongConn();

     /*
         *   ����ʱ����
     *  ���³�ʼ��������,���ô˺���ʱ����Ҫ�ؿ�һ���̣߳�����RoutineNetIO
     */  
    TINT32 RestartLongConn();

private :
    /*
     *    ��������
     */
    TINT32 OnRequest(LongConnHandle stHandle, TUINT32 udwSeqNum, 
                            const TUCHAR *pszData, TUINT32 udwDataLen, TUINT16 uwServType);

private :
    /*
     *    ��������socket
     */
    SOCKET CreateListenSocket(TCHAR* pszListenHost,    TUINT16 uwPort);

    /*
     *    �رռ���socket
     */
    TINT32 CloseListenSocket();

    /*
     *    ��ȡIP���˿�
     */
    TINT32 GetIp2PortByHandle(LongConnHandle stHandle, TUINT16 *puwPort, TCHAR **ppszIp);

    /*
     *    �����η��ش���
     */
    TINT32 SendBackErr(LongConnHandle stHandle, TUINT32 udwServType, TUINT32 udwSeq);

public :
    ILongConn *m_pLongConn;  //������
    CTseLogger *m_poLog;  // Log

    CWorkQueue *m_poTaskQueue;
    CWritebackQueue *m_poDelayTaskQueue;

    TINT32 m_hListenSock;  // ����socket

    CBaseProtocolUnpack *m_poUnpack;  // �����
    CBaseProtocolPack *m_poPack;  // �����

    TBOOL m_bServNew;  // �Ƿ�����µķ���
    TBOOL m_bRelease;  //���Ƿ����ͷų�������Դ
    static TBOOL m_bRunLongConn;  //�Ƿ�ص�������

    TUINT32 m_udwSeqno;


};

#endif ///< _KEY_VALUE_QUERY_NETIO_HH

