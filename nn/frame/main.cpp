#include "global_serv.h"
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

// 0: ��ʼ״̬
// -1: stop
int g_flag = 0;

void sys_sig_kill(int signal)
{
    g_flag = -1;
}

void signal_kill(int signal)
{
    g_flag = -1;
    printf("recv kill signal[%d]\n", signal);
}

// �����û��ź�
TVOID InitSignal(){
    struct sigaction sa;
    sigset_t sset;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sys_sig_kill;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    sigemptyset(&sset);
    sigaddset(&sset, SIGSEGV);
    sigaddset(&sset, SIGBUS);
    sigaddset(&sset, SIGABRT);
    sigaddset(&sset, SIGILL);
    sigaddset(&sset, SIGCHLD);
    sigaddset(&sset, SIGFPE);
    sigprocmask(SIG_UNBLOCK, &sset, &sset);

    signal(SIGUSR1, signal_kill);
}


int main(int argc, char** argv)
{
    // ��ʼ���ź���
    g_flag = 0;
    InitSignal();

    int ret_flag = 0;

    // ��ʼ��
    ret_flag = CGlobalServ::Init();
    assert(ret_flag == 0);

    // �����߳�
    ret_flag = CGlobalServ::Start();
    assert(ret_flag == 0);

    // �����ź����
    while (1)
    {
        // �յ��ź�����,����һֱ�ȴ�
        if (g_flag == -1)
        {
            TSE_LOG_INFO(CGlobalServ::m_poServLog, ("Receive stop signal!"));
            TSE_LOG_INFO(CGlobalServ::m_poServLog, ("Stop new req, wait to exit ..."));

            //�յ��ź�֮��ֹͣ����, ��ʱ���˳�
            CGlobalServ::StopProcess();
            sleep(2);

            TSE_LOG_INFO(CGlobalServ::m_poServLog, ("Stop success!"));
            break;
        }

        /*
        if (access(MAP_SVR_LIST_UPDATE_FLAG, F_OK) == 0)
        {
            TSE_LOG_INFO(CGlobalServ::m_poServLog, ("Update map svr list!"));
            CMapCenter::GetInstance()->Update();
            remove(MAP_SVR_LIST_UPDATE_FLAG);
        }

        if (access(OUTPUT_MAP_LIST_FLAG, F_OK) == 0)
        {
            TSE_LOG_INFO(CGlobalServ::m_poServLog, ("OutputMapList in %s!", OUTPUT_MAP_LIST_FILE));
            CMapCenter::GetInstance()->OutputMapList();
            remove(OUTPUT_MAP_LIST_FLAG);
        }

        if (access(DISTRIBUTION_UPDATE_FLAG, F_OK) == 0)
        {
            TSE_LOG_INFO(CGlobalServ::m_poServLog, ("update distribution plan"));
            CMapUserDistribution::Update();
            remove(DISTRIBUTION_UPDATE_FLAG);
        }
        */

        // CMapCenter::GetInstance()->UpdateResWild();

        sleep(1);
    }

    return 0;
}

