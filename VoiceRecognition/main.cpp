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
	grammar_len = ftell(fp); //获取语法文件大小 
	fseek(fp, 0, SEEK_SET); 

	grammar = (char*)malloc(grammar_len + 1);
	if (NULL == grammar) {
		printf("\nout of memory!\n");
		goto grammar_exit;
	}

	read_len = fread((void *)grammar, 1, grammar_len, fp); //读取语法内容
	if (read_len != grammar_len) {
		printf("\nread grammar error!\n");
		goto grammar_exit;
	}
	grammar[grammar_len] = '\0';

	ret_id = MSPUploadData("usergram", grammar, grammar_len, "dtt = abnf, sub = asr", &ret); //上传语法
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
	printf("grammar_id: \"%s\" \n", grammar_id); //下次可以直接使用该ID，不必重复上传语法。

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
	HWAVEIN hWaveIn;  //输入设备
	WAVEFORMATEX waveform; //采集音频的格式，结构体
	BYTE *pBuffer1;//采集音频时的数据缓存
	WAVEHDR wHdr1; //采集音频时包含数据缓存的结构体
	FILE *pf;
	HANDLE          wait;
	waveform.wFormatTag = WAVE_FORMAT_PCM;//声音格式为PCM
	waveform.nSamplesPerSec = 16000;//采样率，16000次/秒
	waveform.wBitsPerSample = 16;//采样比特，16bits/次
	waveform.nChannels = 1;//采样声道数，2声道
	waveform.nAvgBytesPerSec = 16000;//每秒的数据率，就是每秒能采集多少字节的数据
	waveform.nBlockAlign = 2;//一个块的大小，采样bit的字节数乘以声道数
	waveform.cbSize = 0;//一般为0

	wait = CreateEvent(NULL, 0, 0, NULL);
	//使用waveInOpen函数开启音频采集
	waveInOpen(&hWaveIn, WAVE_MAPPER, &waveform, (DWORD_PTR)wait, 0L, CALLBACK_EVENT);

	//建立两个数组（这里可以建立多个数组）用来缓冲音频数据
	DWORD bufsize = 1024 * 100;//每次开辟10k的缓存存储录音数据
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
		waveInPrepareHeader(hWaveIn, &wHdr1, sizeof(WAVEHDR));//准备一个波形数据块头用于录音
		waveInAddBuffer(hWaveIn, &wHdr1, sizeof(WAVEHDR));//指定波形数据块为录音输入缓存
		waveInStart(hWaveIn);//开始录音
		Sleep(1000);//+1s
		waveInReset(hWaveIn);//停止录音
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
	char			hints[HINTS_SIZE]				= {'\0'}; //hints为结束本次会话的原因描述，由用户自定义
	unsigned int	total_len						= 0;
	int 			aud_stat 						= MSP_AUDIO_SAMPLE_CONTINUE;		//音频状态
	int 			ep_stat 						= MSP_EP_LOOKING_FOR_SPEECH;		//端点检测
	int 			rec_stat 						= MSP_REC_STATUS_SUCCESS;			//识别状态	
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
	pcm_size = ftell(f_pcm); //获取音频文件大小 
	fseek(f_pcm, 0, SEEK_SET);		

	p_pcm = (char*)malloc(pcm_size);
	if (NULL == p_pcm) {
		printf("\nout of memory!\n");
		goto asr_exit;
	}

	read_size = fread((void *)p_pcm, 1, pcm_size, f_pcm); //读取音频文件内容
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
	// todo 边录边发 
	while (1) {
		unsigned int len = 10 * FRAME_LEN; // 每次写入200ms音频(16k，16bit)：1帧音频20ms，10帧=200ms。16k采样率的16位音频，一帧的大小为640Byte。3s的场合有150帧，应该要有92K
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
		Sleep(200); //模拟人说话时间间隙，10帧的音频长度为200ms
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
		Sleep(150); //防止频繁占用CPU
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
	const char* login_params = "appid = 571ab578, work_dir = ."; //登录参数,appid与msc库绑定,请勿随意改动
	const char*	session_begin_params = "sub = asr, result_type = plain, result_encoding = gb2312";
	char*		grammar_id = NULL;

	ret = MSPLogin(NULL, NULL, login_params); //第一个参数是用户名，第二个参数是密码，均传NULL即可，第三个参数是登录参数
	if (MSP_SUCCESS != ret) {
		printf("MSPLogin failed, error code: %d.\n", ret);
		goto exit; //登录失败，退出登录
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
	//wav文件的码率是256kbps
exit:
	if (NULL != grammar_id) {
		free(grammar_id);
		grammar_id = NULL;
	}
	MSPLogout(); //退出登录

	return 0;
}
