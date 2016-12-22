/* Project Robot
 * Au: SJoshua 
 */
#include <cstdio>
#include <opencv2/opencv.hpp>

using namespace cv;
const int delay = 30;
const char captureImageName[] = "capture.jpg";

FILE *fp; 
int count = 0, cnt = 0;

/* function formatPicture():
 *   resize picture and save as cifar-10 format
 */
void formatPicture(void) {
    IplImage *pSrcImage = cvLoadImage(captureImageName, CV_LOAD_IMAGE_UNCHANGED);
    IplImage *pDstImage = NULL;
	CvSize czSize;
    czSize.width = czSize.height = 32;
    pDstImage = cvCreateImage(czSize, pSrcImage->depth, pSrcImage->nChannels);  
    cvResize(pSrcImage, pDstImage, CV_INTER_AREA);
	int b[32*32], g[32*32], r[32*32];
	fwrite(&count, sizeof(char), 1, fp);
	for (int i = 0; i < 32; i++) {
		uchar * pucPixel = (uchar*)pDstImage->imageData + i*pDstImage->widthStep;
		for (int j = 0; j < 32; j++)  {
			b[i*32+j] = pucPixel[3*j];
			g[i*32+j] = pucPixel[3*j+1];
			r[i*32+j] = pucPixel[3*j+2];
		}
	}
	for (int k = 0; k < 32*32; k++) {
		fwrite(&r[k], sizeof(char), 1, fp);
	}
	for (int k = 0; k < 32*32; k++) {
		fwrite(&g[k], sizeof(char), 1, fp);
	}
	for (int k = 0; k < 32*32; k++) {
		fwrite(&b[k], sizeof(char), 1, fp);
	}
	cvReleaseImage(&pSrcImage);
	cvReleaseImage(&pDstImage);
}

/* function capture(camera):
 *   save frame as captureImageName 
 */
void capture(int camera) { 
	cvNamedWindow("Camera");
	cvNamedWindow("extCamera");
	CvCapture *capture, *extCapture;
	printf("label: 0\n");
	capture = cvCreateCameraCapture(camera);
	extCapture = cvCreateCameraCapture(2);
	IplImage *frame, *extFrame;
	char keyCode;
   	fp = fopen("train.bin", "ab");
	while ((keyCode = cvWaitKey(delay))) {
		if (keyCode == 27) {
			break;
		} else if (keyCode >= '0' && keyCode <= '9') {
			printf("label: %d\n", count = keyCode - '0');
			cnt = 0;
		} else if (keyCode == 32) {
			printf("label: %d\n", ++count);
			cnt = 0;
		} else if (keyCode == 'c') {
    		cvReleaseCapture(&capture);
			capture = cvCreateCameraCapture((++camera) %= 2);
		} else if (keyCode == 13) {
			IplImage* outImage = cvCreateImage(cvGetSize(frame), frame->depth, frame->nChannels);
			cvCopy(frame, outImage, NULL);	
			cvSaveImage(captureImageName, outImage);
			cvReleaseImage(&outImage);
			
			printf("cnt: %d\n", ++cnt);
			formatPicture();
			
			outImage = cvCreateImage(cvGetSize(extFrame), extFrame->depth, extFrame->nChannels);
			cvCopy(extFrame, outImage, NULL);	
			cvSaveImage(captureImageName, outImage);
			cvReleaseImage(&outImage);
			
			printf("cnt: %d\n", ++cnt);
			formatPicture();
			//break;
		}
		frame = cvQueryFrame(capture);
		extFrame = cvQueryFrame(extCapture);
		if (!frame) {
			break;
		}
		cvShowImage("Camera", frame);
		cvShowImage("extCamera", extFrame);
	}
	fclose(fp);
	cvReleaseImage(&frame);
    cvReleaseCapture(&capture);
	cvReleaseImage(&extFrame);
    cvReleaseCapture(&extCapture);
	cvDestroyAllWindows();
}

/* function showPicture():
 *   print picture.
 */
void showPicture(void) {
	IplImage *srcImg = NULL;
	srcImg = cvLoadImage(captureImageName, 1);
	if(srcImg == NULL) {
		printf("failed.\n");
 	} else {
		cvShowImage("Capture", srcImg); 
		cvWaitKey(0);
		cvReleaseImage(&srcImg);
	}
}

int main(void) {
	capture(1);
	//showPicture();
	return 0;
}

