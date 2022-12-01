/*

 */

#ifndef TOOL_H_
#define TOOL_H_
//�ڴ˴���������ͷ�ļ�
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif
#define YUV420	0
#define YUV422 	YUV420 	+ 1
#define YUV444 	YUV422 	+ 1
/*************************************************************
Function:       ReadBmpFile
Description:    ��ȡbmpͼ���ڴ�
Input:          pFilePath��bmp����·��
                pData��rgbͼ�������ڴ�ָ�룬rgbͼ����������˳��Ϊbgrbgr...bgr��rgbֵ����Ϊ8bit
				width��ͼ���
				height��ͼ���
Output:         ��
*************************************************************/
extern void ReadBmpFile(char *pFilePath, unsigned char *pData, int & width, int & height);
/*************************************************************
Function:       SaveBmpFile
Description:    ��rgbͼ�����ݱ���Ϊbmp
Input:          pFilePath��bmp����·��
                pData��rgbͼ�������ڴ�ָ�룬rgbͼ����������˳��Ϊbgrbgr...bgr��rgbֵ����Ϊ8bit
				width��ͼ���
				height��ͼ���
Output:         ��
*************************************************************/
extern void SaveBmpFile(char *pFilePath, unsigned char *pData, int width, int height);
/*************************************************************
Function:       SaveRaw
Description:    ����rawͼ
Input:          pSavePath��raw����·��
                pData��raw����
				width��rawͼ���
				height��rawͼ���
Output:         ��
*************************************************************/
extern void SaveRaw(char *pSavePath, short *pRawData, int width, int height);

extern void SaveRaw32bit(char *pSavePath, long *pRawData, int width, int height);

/*************************************************************
Function:       SaveBmpFile2
Description:    ��������λ�����8bit��bmpͼ��
Input:          pFilePath��bmp����·��
				width��ͼ���
				height��ͼ���
				bitValue��ͼ������λ��
				pRGBData��rgbͼ�������ڴ�ָ�룬rgbͼ����������˳��Ϊbgrbgr...bgr
Output:         ��
*************************************************************/
extern void SaveBmpFile2(char *pFilePath, int width, int height, int bitValue, short *pRGBData);

/*************************************************************
Function:       SaveYUVData
Description:    ����8bit YUVͼ
Input:          pSavePath������·��
                pData��yuv���ݣ�8bit������˳��yyy...yyyuuu...uuuvvv...vvv
				width��ͼ���
				height��ͼ���
Output:         ��
*************************************************************/
extern void SaveYUVData(char *pSavePath, unsigned char *pData, int width, int height);



/*************************************************************
Function:       SaveYUVData2
Description:    ��������λ�����8bit��YUVͼ
Input:          pSavePath������·��
                pData��yuv���ݣ�����λ�����8bit������˳��yyy...yyyuuu...uuuvvv...vvv
				width��ͼ���
				height��ͼ���
Output:         ��
*************************************************************/
extern void SaveYUVData2(char *pSavePath, short *pData, int width, int height, int bitValue);
/*************************************************************
Function:       SaveYUVData1
Description:    ����8bit YUV420ͼ
Input:          pSavePath������·��
                pData��yuv���ݣ�8bit������˳��yyy...yyyuuu...uuuvvv...vvv
				width��ͼ���
				height��ͼ���
Output:         ��
*************************************************************/
extern void SaveYUVData1(char *pSavePath, unsigned char *pData, int width, int height, int fmt);
/*************************************************************
Function:       ReadYUVData1
Description:    ��ȡ8bit YUV420ͼ
Input:          pReadPath������·��
                pData��yuv���ݣ�8bit������˳��yyy...yyyuuu...uuuvvv...vvv
				width��ͼ���
				height��ͼ���
Output:         ��
*************************************************************/
extern void ReadYUVData1(char *pReadPath, unsigned char *pData, int width, int height, int fmt);
/*************************************************************
Function:     Yuvfmtconv
Description:    yuv fmt conversion.444 420 422 to 444 420 422
Input:   	pDatain ���뻺��
		pDataout �������
		width ��
		height ��
		fmt_in �����ʽ
		fmt_out �����ʽ
Output:	      ��
*************************************************************/
extern void Yuvfmtconv(void *pDatain, void *pDataout, int width, int height, int fmt_in, int fmt_out, int size);
/*************************************************************
Function:     Yuvbitstochar
Description:    save yuv to 8 bitdepth
Input:   	pDatain ���뻺��
		pDataout �������
		size yuv����
		height ����λ��
Output:	      ��
*************************************************************/
extern void Yuvbitstochar(short *pDatain, unsigned char *pDataout, int size,  int bitdepth);

/*************************************************************
Function:       SaveCfaBmp
Description:    ��raw�����cfaͼ��
Input:          pRawData�������rawͼ��
                width��rawͼ��
				height��rawͼ�ߣ�
				bayerPattern��bayer pattern��ʽ��ȡֵ��Χ[0��3]��
				bitValue��raw����λ��
Output:         ��
*************************************************************/
extern void SaveCfaBmp(char *pFilePath, short *pRawData, int width, int height, int bayerPattern, int bitValue);

#ifdef __cplusplus
}
#endif

#endif  // TOOL_H_
