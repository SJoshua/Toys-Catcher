/* VoiceRecognition Module @ Robot Control
 * Au: SJoshua
 */
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include <conio.h>
#include <errno.h>

#include "qisr.h"
#include "msp_cmn.h"
#include "msp_errors.h"

#define	BUFFER_SIZE 2048
#define HINTS_SIZE  100
#define GRAMID_LEN	128
#define FRAME_LEN	640 

void send(char * sendData) {
    WORD sockVersion = MAKEWORD(2,2);
    WSADATA data; 
    if (WSAStartup(sockVersion, &data)) {
        return;
    }
    SOCKET sclient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sclient == INVALID_SOCKET) {
        printf("invalid socket !");
        return;
    }
    sockaddr_in serAddr;
    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(8865);
    serAddr.sin_addr.S_un.S_addr = inet_addr("10.105.181.245"); 
    if (connect(sclient, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR) {
        printf("connect error !");
        closesocket(sclient);
        return;
    }
    send(sclient, sendData, strlen(sendData), 0);
    closesocket(sclient);
    WSACleanup();
}

int get_grammar_id(char* grammar_id, unsigned int id_len) {
	FILE*			fp				=	NULL;
	char*			grammar			=	NULL;
	unsigned int	grammar_len		=	0;
	unsigned int	read_len		=	0;
	const char*		ret_id			=	NULL;
	unsigned int	ret_id_len		=	0;
	int				ret				=	-1;	

	if (NULL == grammar_id)
		goto grammar_exit;

	fp = fopen("grammar.abnf", "rb");
	if (NULL == fp) {   
		printf("\nopen grammar file failed!\n");
		goto grammar_exit;
	}
	
	fseek(fp, 0, SEEK_END);
	grammar_len = ftell(fp); //��ȡ�﷨�ļ���С 
	fseek(fp, 0, SEEK_SET); 

	grammar = (char*)malloc(grammar_len + 1);
	if (NULL == grammar) {
		printf("\nout of memory!\n");
		goto grammar_exit;
	}

	read_len = fread((void *)grammar, 1, grammar_len, fp); //��ȡ�﷨����
	if (read_len != grammar_len) {
		printf("\nread grammar error!\n");
		goto grammar_exit;
	}
	grammar[grammar_len] = '\0';

	ret_id = MSPUploadData("usergram", grammar, grammar_len, "dtt = abnf, sub = asr", &ret); //�ϴ��﷨
	if (MSP_SUCCESS != ret) {
		printf("\nMSPUploadData failed, error code: %d.\n", ret);
		goto grammar_exit;
	}

	ret_id_len = strlen(ret_id);
	if (ret_id_len >= id_len) {
		printf("\nno enough buffer for grammar_id!\n");
		goto grammar_exit;
	}
	strncpy(grammar_id, ret_id, ret_id_len);
	printf("grammar_id: \"%s\" \n", grammar_id); //�´ο���ֱ��ʹ�ø�ID�������ظ��ϴ��﷨��

grammar_exit:
	if (NULL != fp) {
		fclose(fp);
		fp = NULL;
	}
	if (NULL!= grammar) {
		free(grammar);
		grammar = NULL;
	}
	return ret;
}

void record(void) {
	HWAVEIN hWaveIn;  //�����豸
	WAVEFORMATEX waveform; //�ɼ���Ƶ�ĸ�ʽ���ṹ��
	BYTE *pBuffer1;//�ɼ���Ƶʱ�����ݻ���
	WAVEHDR wHdr1; //�ɼ���Ƶʱ�������ݻ���Ľṹ��
	FILE *pf;
	HANDLE          wait;
	waveform.wFormatTag = WAVE_FORMAT_PCM;//������ʽΪPCM
	waveform.nSamplesPerSec = 16000;//�����ʣ�16000��/��
	waveform.wBitsPerSample = 16;//�������أ�16bits/��
	waveform.nChannels = 1;//������������2����
	waveform.nAvgBytesPerSec = 16000;//ÿ��������ʣ�����ÿ���ܲɼ������ֽڵ�����
	waveform.nBlockAlign = 2;//һ����Ĵ�С������bit���ֽ�������������
	waveform.cbSize = 0;//һ��Ϊ0

	wait = CreateEvent(NULL, 0, 0, NULL);
	//ʹ��waveInOpen����������Ƶ�ɼ�
	waveInOpen(&hWaveIn, WAVE_MAPPER, &waveform, (DWORD_PTR)wait, 0L, CALLBACK_EVENT);

	//�����������飨������Խ���������飩����������Ƶ����
	DWORD bufsize = 1024 * 100;//ÿ�ο���10k�Ļ���洢¼������
	printf("Press any key to start...\n");
	getch();
	pf = fopen("voice.wav", "wb");
	while (!kbhit()) {
		pBuffer1 = new BYTE[bufsize];
		wHdr1.lpData = (LPSTR)pBuffer1;
		wHdr1.dwBufferLength = bufsize;
		wHdr1.dwBytesRecorded = 0;
		wHdr1.dwUser = 0;
		wHdr1.dwFlags = 0;
		wHdr1.dwLoops = 1;
		waveInPrepareHeader(hWaveIn, &wHdr1, sizeof(WAVEHDR));//׼��һ���������ݿ�ͷ����¼��
		waveInAddBuffer(hWaveIn, &wHdr1, sizeof(WAVEHDR));//ָ���������ݿ�Ϊ¼�����뻺��
		waveInStart(hWaveIn);//��ʼ¼��
		Sleep(1000);//+1s
		waveInReset(hWaveIn);//ֹͣ¼��
		fwrite(pBuffer1, 1, wHdr1.dwBytesRecorded, pf);
		delete pBuffer1;
		printf(">");
	}
	fclose(pf);

	waveInClose(hWaveIn);
} 

void run_asr(const char* audio_file, const char* params, char* grammar_id) {
	const char*		session_id						= NULL;
	char			rec_result[BUFFER_SIZE]		 	= {'\0'};	
	char			hints[HINTS_SIZE]				= {'\0'}; //hintsΪ�������λỰ��ԭ�����������û��Զ���
	unsigned int	total_len						= 0;
	int 			aud_stat 						= MSP_AUDIO_SAMPLE_CONTINUE;		//��Ƶ״̬
	int 			ep_stat 						= MSP_EP_LOOKING_FOR_SPEECH;		//�˵���
	int 			rec_stat 						= MSP_REC_STATUS_SUCCESS;			//ʶ��״̬	
	int 			errcode 						= MSP_SUCCESS;

	FILE*			f_pcm 							= NULL;
	char*			p_pcm 							= NULL;
	long 			pcm_count 						= 0;
	long 			pcm_size 						= 0;
	long			read_size						= 0;

	if (NULL == audio_file)
		goto asr_exit;

	f_pcm = fopen(audio_file, "rb");
	if (NULL == f_pcm) {
		printf("\nopen [%s] failed!\n", audio_file);
		goto asr_exit;
	}
	
	fseek(f_pcm, 0, SEEK_END);
	pcm_size = ftell(f_pcm); //��ȡ��Ƶ�ļ���С 
	fseek(f_pcm, 0, SEEK_SET);		

	p_pcm = (char*)malloc(pcm_size);
	if (NULL == p_pcm) {
		printf("\nout of memory!\n");
		goto asr_exit;
	}

	read_size = fread((void *)p_pcm, 1, pcm_size, f_pcm); //��ȡ��Ƶ�ļ�����
	if (read_size != pcm_size) {
		printf("\nread [%s] failed!\n", audio_file);
		goto asr_exit;
	}
	
	printf("\nConnecting to server ...\n");
	session_id = QISRSessionBegin(grammar_id, params, &errcode);
	if (MSP_SUCCESS != errcode) {
		printf("\nQISRSessionBegin failed, error code:%d\n", errcode);
		goto asr_exit;
	}
	// todo ��¼�߷� 
	while (1) {
		unsigned int len = 10 * FRAME_LEN; // ÿ��д��200ms��Ƶ(16k��16bit)��1֡��Ƶ20ms��10֡=200ms��16k�����ʵ�16λ��Ƶ��һ֡�Ĵ�СΪ640Byte��3s�ĳ�����150֡��Ӧ��Ҫ��92K
		int ret = 0;

		if ((unsigned int)pcm_size < 2 * len) 
			len = pcm_size;
		if (len <= 0)
			break;
		
		aud_stat = MSP_AUDIO_SAMPLE_CONTINUE;
		if (0 == pcm_count)
			aud_stat = MSP_AUDIO_SAMPLE_FIRST;
		
		printf(">");
		ret = QISRAudioWrite(session_id, (const void *)&p_pcm[pcm_count], len, aud_stat, &ep_stat, &rec_stat);
		if (MSP_SUCCESS != ret)	{
			printf("\nQISRAudioWrite failed, error code:%d\n",ret);
			goto asr_exit;
		}
			
		pcm_count += (long)len;
		pcm_size  -= (long)len;
		
		if (MSP_EP_AFTER_SPEECH == ep_stat)
			break;
		Sleep(200); //ģ����˵��ʱ���϶��10֡����Ƶ����Ϊ200ms
	}
	errcode = QISRAudioWrite(session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &ep_stat, &rec_stat);
	if (MSP_SUCCESS != errcode) {
		printf("\nQISRAudioWrite failed, error code:%d\n",errcode);
		goto asr_exit;	
	}

	while (MSP_REC_STATUS_COMPLETE != rec_stat) {
		const char *rslt = QISRGetResult(session_id, &rec_stat, 0, &errcode); 
		if (MSP_SUCCESS != errcode) {
			printf("\nQISRGetResult failed, error code: %d\n", errcode);
			goto asr_exit;
		}
		if (NULL != rslt) {
			unsigned int rslt_len = strlen(rslt);
			total_len += rslt_len;
			if (total_len >= BUFFER_SIZE) {
				printf("\nno enough buffer for rec_result !\n");
				goto asr_exit;
			}
			strncat(rec_result, rslt, rslt_len);
		}
		Sleep(150); //��ֹƵ��ռ��CPU
	}
	printf("\nFinished.\nResult:\n%s", rec_result);
	send(rec_result);
asr_exit:
	if (NULL != f_pcm) {
		fclose(f_pcm);
		f_pcm = NULL;
	}
	if (NULL != p_pcm) {	
		free(p_pcm);
		p_pcm = NULL;
	}

	QISRSessionEnd(session_id, hints);
}

int main(void) {
	int			ret = MSP_SUCCESS;
	const char* login_params = "appid = 571ab578, work_dir = ."; //��¼����,appid��msc���,��������Ķ�
	const char*	session_begin_params = "sub = asr, result_type = plain, result_encoding = gb2312";
	char*		grammar_id = NULL;

	ret = MSPLogin(NULL, NULL, login_params); //��һ���������û������ڶ������������룬����NULL���ɣ������������ǵ�¼����
	if (MSP_SUCCESS != ret) {
		printf("MSPLogin failed, error code: %d.\n", ret);
		goto exit; //��¼ʧ�ܣ��˳���¼
	}

	printf("Automatic Speech Recognition\n");

	grammar_id = (char*)malloc(GRAMID_LEN);
	if (NULL == grammar_id) {
		printf("out of memory !\n");
		goto exit;
	}
	memset(grammar_id, 0, GRAMID_LEN);

	printf("Uploading grammar config ...\n");
	ret = get_grammar_id(grammar_id, GRAMID_LEN);
	if (MSP_SUCCESS != ret)
		goto exit;
	printf("Successed.\n");

	while (1) {
		record();
		run_asr("voice.wav", session_begin_params, grammar_id);
		//printf("Press any key to continue ... [ESC to exit]\n");
		//_getch();
		//int key = _getch();
		//if(key == 27)
		//	break;
	}
	//wav�ļ���������256kbps
exit:
	if (NULL != grammar_id) {
		free(grammar_id);
		grammar_id = NULL;
	}
	MSPLogout(); //�˳���¼

	return 0;
}
