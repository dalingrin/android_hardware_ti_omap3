
/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */
/**
* @file SkImageDecoder_libtijpeg.cpp
*
* This file implements SkTIJPEGImageDecoder
*
*/

#include "SkImageDecoder_libtijpeg.h"

typedef struct OMX_CUSTOM_RESOLUTION 
{
	OMX_U32 nWidth;
	OMX_U32 nHeight;
} OMX_CUSTOM_RESOLUTION;


OMX_ERRORTYPE OMX_EventHandler(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData,
                                    OMX_EVENTTYPE eEvent,
                                    OMX_U32 nData1,
                                    OMX_U32 nData2,
                                    OMX_PTR pEventData)
{
    ((SkTIJPEGImageDecoder *)pAppData)->EventHandler(hComponent, eEvent, nData1, nData2, pEventData);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMX_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    SkTIJPEGImageDecoder * ImgDec = (SkTIJPEGImageDecoder *)ptr;
    ImgDec->iLastState = ImgDec->iState;
    ImgDec->iState = SkTIJPEGImageDecoder::STATE_EMPTY_BUFFER_DONE_CALLED;
    sem_post(ImgDec->semaphore) ;
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMX_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead)
{
    SkDebugf("\nOMX_FillBufferDone:: FilledLen = %ld \n", pBuffHead->nFilledLen);
    ((SkTIJPEGImageDecoder *)ptr)->FillBufferDone(pBuffHead->pBuffer, pBuffHead->nFilledLen);

    return OMX_ErrorNone;
}


SkTIJPEGImageDecoder::~SkTIJPEGImageDecoder()
{
    free(semaphore) ;
    semaphore=NULL;
}

SkTIJPEGImageDecoder::SkTIJPEGImageDecoder()
{
    pInBuffHead = NULL;
    pOutBuffHead = NULL;
    semaphore = NULL;
    semaphore = (sem_t*)malloc(sizeof(sem_t)) ;
    sem_init(semaphore, 0x00, 0x00);

}


OMX_S16 SkTIJPEGImageDecoder::Get16m(const void * Short)
{
    return(((OMX_U8 *)Short)[0] << 8) | ((OMX_U8 *)Short)[1];
}

void SkTIJPEGImageDecoder::FixFrameSize(JPEG_HEADER_INFO* JpegHeaderInfo)
{

    OMX_U32 nWidth=JpegHeaderInfo->nWidth, nHeight=JpegHeaderInfo->nHeight;

    /*round up if nWidth is not multiple of 32*/
    ( (nWidth%32 ) !=0 ) ?  nWidth=32 * (  (  nWidth/32 ) + 1 )  : nWidth;
    SkDebugf("new nWidth %d \n", nWidth);

    /*round up if nHeight is not multiple of 16*/
    ( (nHeight%16) !=0 ) ?  nHeight=16 * (  (  nHeight/16 ) + 1 )  : nHeight;
    SkDebugf("new nHeight %d \n", nHeight);

    JpegHeaderInfo->nWidth = nWidth;
    JpegHeaderInfo->nHeight = nHeight;
}


OMX_S16 SkTIJPEGImageDecoder::GetYUVformat(OMX_U8 * Data)
{
    unsigned char Nf;
    OMX_U32 j;
    OMX_U32 temp_index;
    OMX_U32 temp;
    OMX_U32 image_format;
    short H[4],V[4];

    Nf = Data[7];

    for (j = 0; j < Nf; j++)
    {
       temp_index = j * 3 + 7 + 2;
        /*---------------------------------------------------------*/
       /* H[j]: upper 4 bits of a byte, horizontal sampling fator.                  */
       /* V[j]: lower 4 bits of a byte, vertical sampling factor.                    */
       /*---------------------------------------------------------*/
         H[j] = (0x0f & (Data[temp_index] >> 4));
         V[j] = (0x0f & Data[temp_index]);
    }

    /*------------------------------------------------------------------*/
    /* Set grayscale flag, namely if image is gray then set it to 1,    */
    /* else set it to 0.                                                */
    /*------------------------------------------------------------------*/
    image_format = -1;

    if (Nf == 1){
      image_format = OMX_COLOR_FormatL8;
    }


    if (Nf == 3)
    {
       temp = (V[0]*H[0])/(V[1]*H[1]) ;

      if (temp == 4 && H[0] == 2)
        image_format = OMX_COLOR_FormatYUV420Planar;

      if (temp == 4 && H[0] == 4)
        image_format = OMX_COLOR_FormatYUV411Planar;

      if (temp == 2)
        image_format = OMX_COLOR_FormatCbYCrY; /* YUV422 interleaved, little endian */

      if (temp == 1)
        image_format = OMX_COLOR_FormatYUV444Interleaved;
    }

    return (image_format);

}


OMX_S32 SkTIJPEGImageDecoder::ParseJpegHeader (SkStream* stream, JPEG_HEADER_INFO* JpgHdrInfo)
{
    OMX_U8 a = 0;
    OMX_S32 lSize = 0;
    lSize = stream->getLength();
    SkDebugf("File size is %d\n", lSize);
    stream->rewind();
    JpgHdrInfo->nProgressive = 0; /*Default value is non progressive*/


    a = stream->readU8();
    if ( a != 0xff || stream->readU8() != M_SOI )  {
        return 0;
    }
    for ( ;; )  {
        OMX_U32 itemlen = 0;
        OMX_U32 marker = 0;
        OMX_U32 ll = 0,lh = 0, got = 0;
        OMX_U8 *Data = NULL;

        for ( a=0;a<15 /* 7 originally */;a++ ) {
            marker = stream->readU8();

            SkDebugf("MARKER IS %x\n",marker);

            if ( marker != 0xff )   {
                break;
            }
            if ( a >= 14 /* 6 originally */)   {
                SkDebugf("too many padding bytes\n");
                if ( Data != NULL ) {
                    free(Data);
                    Data=NULL;
                }
                return 0;
            }
        }
        if ( marker == 0xff )   {
            /* 0xff is legal padding, but if we get that many, something's wrong.*/
            SkDebugf("too many padding bytes!");
        }

        /* Read the length of the section.*/
        lh = stream->readU8();
        ll = stream->readU8();

        itemlen = (lh << 8) | ll;

        if ( itemlen < 2 )  {
            SkDebugf("invalid marker");
        }

        Data = (OMX_U8 *)malloc(itemlen);
        if ( Data == NULL ) {
            SkDebugf("Could not allocate memory");
            break;
        }

        /* Store first two pre-read bytes. */
        Data[0] = (OMX_U8)lh;
        Data[1] = (OMX_U8)ll;

        got = stream->read(Data+2, itemlen-2); /* Read the whole section.*/

        if ( got != itemlen-2 ) {
            SkDebugf("Premature end of file?");
        }

        SkDebugf("Jpeg section marker 0x%02x size %d\n",marker, itemlen);
        switch ( marker )   {

        case M_SOS:
            if ( Data != NULL ) {
                free(Data);
                Data=NULL;
            }

            return lSize;

        case M_EOI:
            SkDebugf("No image in jpeg!\n");
            if ( Data != NULL ) {
                free(Data);
                Data=NULL;
            }
            return 0;

        case M_COM: /* Comment section  */

            break;

        case M_JFIF:

            break;

        case M_EXIF:

            break;

        case M_SOF2:
            SkDebugf("nProgressive IMAGE!\n");
            JpgHdrInfo->nProgressive=1;

        case M_SOF0:
        case M_SOF1:
        case M_SOF3:
        case M_SOF5:
        case M_SOF6:
        case M_SOF7:
        case M_SOF9:
        case M_SOF10:
        case M_SOF11:
        case M_SOF13:
        case M_SOF14:
        case M_SOF15:

            JpgHdrInfo->nHeight = Get16m(Data+3);
            JpgHdrInfo->nWidth = Get16m(Data+5);
            JpgHdrInfo->nFormat = GetYUVformat(Data);
            switch (JpgHdrInfo->nFormat) {
            case OMX_COLOR_FormatYUV420Planar:
                SkDebugf("APP:: Image chroma format is OMX_COLOR_FormatYUV420Planar\n");
                break;
            case OMX_COLOR_FormatYUV411Planar:
                SkDebugf("APP:: Image chroma format is OMX_COLOR_FormatYUV411Planar\n");
                break;
            case OMX_COLOR_FormatCbYCrY:
                SkDebugf("APP:: Image chroma format is OMX_COLOR_FormatYUV422Interleaved\n");
                break;
            case OMX_COLOR_FormatYUV444Interleaved:
                 SkDebugf("APP:: Image chroma format is OMX_COLOR_FormatYUV444Interleaved\n");
                 break;
            case OMX_COLOR_FormatL8:
                SkDebugf("APP:: Image chroma format is OMX_COLOR_FormatL8 \n");
                break;
            default:
                 SkDebugf("APP:: Cannot find Image chroma format \n");
                 JpgHdrInfo->nFormat = OMX_COLOR_FormatUnused;
                 break;
            }
            SkDebugf("APP:: Image Width x Height = %u * %u\n", Get16m(Data+5), Get16m(Data+3)  );
            /*
            SkDebugf("JPEG image is %uw * %uh,\n", Get16m(Data+3), Get16m(Data+5)  );

            if ( *(Data+9)==0x41 )  {
                SkDebugf("THIS IS A YUV 411 ENCODED IMAGE \n" );
                JpgHdrInfo->format= 1;
            }
            */

            if ( Data != NULL ) {
                free(Data);
                Data=NULL;
            }
            break;
        default:
            /* Skip any other sections.*/
            break;
        }

        if ( Data != NULL ) {
            free(Data);
            Data=NULL;
        }
    }


    return 0;
}



void SkTIJPEGImageDecoder::FillBufferDone(OMX_U8* pBuffer, OMX_U32 nFilledLen)
{

    OMX_U32 i;
    OMX_U8 temp;
    
    if (OutPortDef.format.image.eColorFormat == OMX_COLOR_Format32bitARGB8888)
    {
        for(i = 0; i < nFilledLen; i+=4)
        {
            temp = pBuffer[i];
            pBuffer[i] = pBuffer[i+2];
            pBuffer[i+2] = temp;
        }
    }

    memcpy(bitmap->getPixels(), pBuffer, nFilledLen);
    iLastState = iState;
    iState = STATE_FILL_BUFFER_DONE_CALLED;

}


void SkTIJPEGImageDecoder::EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData)
{

    SkDebugf("\nEventHandler:: eEvent = %x, nData1 = %x, nData2 = %x\n\n", eEvent, (unsigned int)nData1, (unsigned int)nData2);

    switch ( eEvent ) {

        case OMX_EventCmdComplete:
            /* Do not move the common stmts in these conditionals outside. */
            /* We do not want to apply them in cases when these conditions are not met. */
            if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateIdle))
            {
                SkDebugf ("Component State Changed To OMX_StateIdle\n");
                iLastState = iState;
                iState = STATE_IDLE;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateExecuting))
            {
                SkDebugf ("Component State Changed To OMX_StateExecuting\n");
                iLastState = iState;
                iState = STATE_EXECUTING;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateLoaded))
            {
                SkDebugf ("Component State Changed To OMX_StateLoaded\n");
                iLastState = iState;
                iState = STATE_LOADED;
                sem_post(semaphore) ;
            }
            break;

        case OMX_EventError:
            SkDebugf ("\n\n\nOMX Component  reported an Error!!!!\n\n\n");
            iLastState = iState;
            iState = STATE_ERROR;
            OMX_SendCommand(hComponent, OMX_CommandStateSet, OMX_StateInvalid, NULL);
            sem_post(semaphore) ;
            break;

        default:
            break;
    }

}


OMX_S32 SkTIJPEGImageDecoder::fill_data(OMX_BUFFERHEADERTYPE *pBuf, SkStream* stream, OMX_S32 bufferSize)
{
    pBuf->nFilledLen = stream->read(pBuf->pBuffer, bufferSize);
    SkDebugf ("filldata: nAllocLen = %d, nFilledLen =%ld\n", bufferSize, pBuf->nFilledLen);
    return pBuf->nFilledLen;
}


bool SkTIJPEGImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, SkBitmap::Config prefConfig, Mode mode)
{

    int nRetval;
    int nIndex1;
    int nIndex2;
    int scaleFactor;
    int bitsPerPixel;
    OMX_S32 inputFileSize;
    OMX_S32 nCompId = 100;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    JPEG_HEADER_INFO JpegHeaderInfo;
    OMX_PORT_PARAM_TYPE PortType;
    OMX_CUSTOM_RESOLUTION MaxResolution;
    OMX_CONFIG_SCALEFACTORTYPE ScaleFactor;
    OMX_INDEXTYPE nCustomIndex = OMX_IndexMax;
    char strTIJpegDec[] = "OMX.TI.JPEG.Decoder";
    char strColorFormat[] = "OMX.TI.JPEG.decode.Config.OutputColorFormat";
    char strProgressive[] = "OMX.TI.JPEG.decode.Config.ProgressiveFactor";
    char strMaxResolution[] = "OMX.TI.JPEG.decode.Param.SetMaxResolution";

    OMX_CALLBACKTYPE JPEGCallBack ={OMX_EventHandler, OMX_EmptyBufferDone, OMX_FillBufferDone};

    SkDebugf("\nEntered Image Decoder.\n");

    bitmap = bm;
    inStream = stream;
    SkBitmap::Config config = prefConfig;
    scaleFactor = this->getSampleSize();


    SkDebugf("prefConfig = %d\n", prefConfig);
    SkDebugf("mode = %d\n", mode);
    SkDebugf("scaleFactor = %d\n", scaleFactor);

    printf("prefConfig = %d\n", prefConfig);
    printf("mode = %d\n", mode);
    printf("scaleFactor = %d\n", scaleFactor);
	if (scaleFactor == 3)
		scaleFactor = 2;
	else if ((scaleFactor > 4) && (scaleFactor < 8))
		scaleFactor = 4;
		else if (scaleFactor > 8)
			scaleFactor = 8;
		
	SkDebugf ("Modified scaleFactor = %d\n", scaleFactor);

    inputFileSize = ParseJpegHeader(stream , &JpegHeaderInfo);

    if (inputFileSize == 0) {
        SkDebugf("The file size is 0. Maybe the format of the file is not correct\n");
        goto EXIT;
    }

    // if no user preference, see what the device recommends
    if (config == SkBitmap::kNo_Config)
        config = SkImageDecoder::GetDeviceConfig();

    bm->setConfig(config, JpegHeaderInfo.nWidth/scaleFactor, JpegHeaderInfo.nHeight/scaleFactor);
    bm->setIsOpaque(true);
    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        return true;
    }

    FixFrameSize(&JpegHeaderInfo);

    eError = TIOMX_Init();
    if ( eError != OMX_ErrorNone ) {
        SkDebugf("%d :: Error returned by OMX_Init()\n",__LINE__);
        goto EXIT;
    }

    /* Load the JPEGDecoder Component */

    SkDebugf("Calling OMX_GetHandle\n");
    eError = TIOMX_GetHandle(&pOMXHandle, strTIJpegDec, (void *)this, &JPEGCallBack);
    if ( (eError != OMX_ErrorNone) ||  (pOMXHandle == NULL) ) {
        SkDebugf ("Error in Get Handle function\n");
        goto EXIT;
    }

    iLastState = STATE_LOADED;
    iState = STATE_LOADED;

    eError = OMX_GetParameter(pOMXHandle, OMX_IndexParamImageInit, &PortType);
    if ( eError != OMX_ErrorNone ) {
        goto EXIT;
    }

    nIndex1 = PortType.nStartPortNumber;
    nIndex2 = nIndex1 + 1;

    /**********************************************************************/
    /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (INPUT) */
    /**********************************************************************/

    InPortDef.nPortIndex = PortType.nStartPortNumber;
    eError = OMX_GetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &InPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    InPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    InPortDef.nVersion.s.nVersionMajor = 0x1;
    InPortDef.nVersion.s.nVersionMinor = 0x0;
    InPortDef.nVersion.s.nRevision = 0x0;
    InPortDef.nVersion.s.nStep = 0x0;
    InPortDef.eDir = OMX_DirInput;
    InPortDef.nBufferCountActual =1;
    InPortDef.nBufferCountMin = 1;
    InPortDef.bEnabled = OMX_TRUE;
    InPortDef.bPopulated = OMX_FALSE;
    InPortDef.eDomain = OMX_PortDomainImage;
    InPortDef.format.image.pNativeRender = NULL;
    InPortDef.format.image.nFrameWidth = JpegHeaderInfo.nWidth;
    InPortDef.format.image.nFrameHeight = JpegHeaderInfo.nHeight;
    InPortDef.format.image.nStride = -1;
    InPortDef.format.image.nSliceHeight = -1;
    InPortDef.format.image.bFlagErrorConcealment = OMX_FALSE;
    InPortDef.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    InPortDef.nBufferSize = inputFileSize;

    if (InPortDef.eDir == nIndex1 ) {
        InPortDef.nPortIndex = nIndex1;
    }
    else {
        InPortDef.nPortIndex = nIndex2;
    }

    if (JpegHeaderInfo.nFormat == OMX_COLOR_FormatYCbYCr ||
        JpegHeaderInfo.nFormat == OMX_COLOR_FormatYUV444Interleaved ||
        JpegHeaderInfo.nFormat == OMX_COLOR_FormatUnused) {
        InPortDef.format.image.eColorFormat = OMX_COLOR_FormatCbYCrY;
    }
    else {
        InPortDef.format.image.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    }

    SkDebugf("Calling OMX_SetParameter for Input Port\n");
    eError = OMX_SetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &InPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    /***********************************************************************/
    /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (OUTPUT) */
    /***********************************************************************/

    OutPortDef.nPortIndex = PortType.nStartPortNumber;
    eError = OMX_GetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &OutPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    OutPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    OutPortDef.nVersion.s.nVersionMajor = 0x1;
    OutPortDef.nVersion.s.nVersionMinor = 0x0;
    OutPortDef.nVersion.s.nRevision = 0x0;
    OutPortDef.nVersion.s.nStep = 0x0;
    OutPortDef.eDir = OMX_DirOutput;
    OutPortDef.nBufferCountActual = 1;
    OutPortDef.nBufferCountMin = 1;
    OutPortDef.bEnabled = OMX_TRUE;
    OutPortDef.bPopulated = OMX_FALSE;
    OutPortDef.eDomain = OMX_PortDomainImage;
    OutPortDef.format.image.pNativeRender = NULL;
    OutPortDef.format.image.nFrameWidth = JpegHeaderInfo.nWidth;
    OutPortDef.format.image.nFrameHeight = JpegHeaderInfo.nHeight;
    OutPortDef.format.image.nStride = -1;
    OutPortDef.format.image.nSliceHeight = -1;
    OutPortDef.format.image.bFlagErrorConcealment = OMX_FALSE;
    OutPortDef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;

    if (OutPortDef.eDir == nIndex1 ) {
        OutPortDef.nPortIndex = nIndex1;
    }
    else {
        OutPortDef.nPortIndex = nIndex2;
    }

    if (config == SkBitmap::kA8_Config) { /* 8-bits per pixel, with only alpha specified (0 is transparent, 0xFF is opaque) */
        OutPortDef.format.image.eColorFormat = OMX_COLOR_FormatL8;
        SkDebugf("Color format is OMX_COLOR_FormatL8\n");
        bitsPerPixel = 1;
    }
    else if (config == SkBitmap::kRGB_565_Config) { /* 16-bits per pixel*/
        OutPortDef.format.image.eColorFormat = OMX_COLOR_Format16bitRGB565;
        SkDebugf("Color format is OMX_COLOR_Format16bitRGB565\n");
        bitsPerPixel = 2;
    }
    else if (config == SkBitmap::kARGB_8888_Config) { /* 32-bits per pixel */
        OutPortDef.format.image.eColorFormat = OMX_COLOR_Format32bitARGB8888;
        SkDebugf("Color format is OMX_COLOR_Format32bitARGB8888\n");
        bitsPerPixel = 4;
    }
    else { /*Set DEFAULT color format*/
        OutPortDef.format.image.eColorFormat = OMX_COLOR_FormatCbYCrY;
        SkDebugf("Color format is OMX_COLOR_FormatCbYCrY\n");
        bitsPerPixel = 2;
    }

    OutPortDef.nBufferSize = JpegHeaderInfo.nWidth * JpegHeaderInfo.nHeight * bitsPerPixel / scaleFactor;

    SkDebugf("APP:: Output buffer size is %ld\n", OutPortDef.nBufferSize);

    eError = OMX_SetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &OutPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (scaleFactor == 2){
        ScaleFactor.xWidth = 50;
        ScaleFactor.xHeight = 50;
    }
    else if (scaleFactor == 4){
        ScaleFactor.xWidth = 25;
        ScaleFactor.xHeight = 25;
    }
    else if (scaleFactor == 8){
        ScaleFactor.xWidth = 12;
        ScaleFactor.xHeight = 12;
    }
    else{
        ScaleFactor.xWidth = 100;
        ScaleFactor.xHeight = 100;
    }

    eError = OMX_SetConfig (pOMXHandle, OMX_IndexConfigCommonScale, &ScaleFactor);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    eError = OMX_GetExtensionIndex(pOMXHandle, strProgressive, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }
    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &(JpegHeaderInfo.nProgressive));
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    eError = OMX_GetExtensionIndex(pOMXHandle, strColorFormat, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &(OutPortDef.format.image.eColorFormat));
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    /* Set max width & height value*/
    eError = OMX_GetExtensionIndex(pOMXHandle, strMaxResolution, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    MaxResolution.nWidth = JpegHeaderInfo.nWidth;
    MaxResolution.nHeight = JpegHeaderInfo.nHeight;

    eError = OMX_SetParameter (pOMXHandle, nCustomIndex, &MaxResolution);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    if (!bm->allocPixels()) {
        SkDebugf("xxxxxxxxxxxxxxxxxxxx allocPixels failed\n");
        goto EXIT;
    }

    SkDebugf("xxxxxxxxxxxxxxxxxxxx allocPixels:inputPort: %d bytes \n", InPortDef.nBufferSize);
    eError = OMX_AllocateBuffer(pOMXHandle, &pInBuffHead,  InPortDef.nPortIndex,  (void *)&nCompId, InPortDef.nBufferSize);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    /*### For now I am going to use AllocateBuffer. Later on, we must change this to UseBuffer and pass bm as the bufer. */
    /*### OMX will allocate the buffer and in the FillBufferDone function, we will memcpy the buffer to bm.                      */
    eError = OMX_AllocateBuffer(pOMXHandle, &pOutBuffHead,  OutPortDef.nPortIndex,  (void *)&nCompId, OutPortDef.nBufferSize);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    eError = OMX_SendCommand(pOMXHandle, OMX_CommandStateSet, OMX_StateIdle ,NULL);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("Error from SendCommand-Idle(Init) State function\n");
        goto EXIT;
    }

    Run();
    return true;
EXIT:

    return false;
}

void SkTIJPEGImageDecoder::PrintState()
{
    SkDebugf("\niLastState = %x\n", iLastState);
    switch(iState)
    {
        case STATE_LOADED:
            SkDebugf("\nCurrent State is STATE_LOADED.\n");
            break;

        case STATE_IDLE:
            SkDebugf("\nCurrent State is STATE_IDLE.\n");
            break;

        case STATE_EXECUTING:
            SkDebugf("\nCurrent State is STATE_EXECUTING.\n");
            break;

        case STATE_EMPTY_BUFFER_DONE_CALLED:
            SkDebugf("\nCurrent State is STATE_EMPTY_BUFFER_DONE_CALLED.\n");
            break;

        case STATE_FILL_BUFFER_DONE_CALLED:
            SkDebugf("\nCurrent State is STATE_FILL_BUFFER_DONE_CALLED.\n");
            break;

        case STATE_ERROR:
            SkDebugf("\nCurrent State is STATE_ERROR.\n");
            break;

        case STATE_EXIT:
            SkDebugf("\nCurrent State is STATE_EXIT.\n");
            break;

        default:
            SkDebugf("\nCurrent State is Invalid.\n");
            break;

    }
}

void SkTIJPEGImageDecoder::Run()
{
    int nRead;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

while(1){
    sem_wait(semaphore) ;

    PrintState();
    switch(iState)
    {
        case STATE_IDLE:
            if (iLastState == STATE_LOADED)
            {
                eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateExecuting, NULL);
                if ( eError != OMX_ErrorNone ) {
                    SkDebugf("eError from SendCommand-Executing State function\n");
                    iState = STATE_ERROR;
                    break;
                }
            }
            else if (iLastState == STATE_EMPTY_BUFFER_DONE_CALLED)
            {
                    eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("Error from SendCommand-Idle State function\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_SendCommand(pOMXHandle, OMX_CommandPortDisable, 0x0, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("APP:: Error from SendCommand-PortDisable function. Input port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_SendCommand(pOMXHandle, OMX_CommandPortDisable, 0x1, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("APP:: Error from SendCommand-PortDisable function. Output port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    /* Free buffers */
                    eError = OMX_FreeBuffer(pOMXHandle, InPortDef.nPortIndex, pInBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("APP:: Error from OMX_FreeBuffer. Input port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_FreeBuffer(pOMXHandle, OutPortDef.nPortIndex, pOutBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("APP:: Error from OMX_FreeBuffer. Output port.\n");
                        iState = STATE_ERROR;
                        break;
                    }
            }
            else
            {
                iState = STATE_ERROR;
            }
            break;

        case STATE_EXECUTING:

            inStream->rewind();
            nRead = fill_data(pInBuffHead, inStream, pInBuffHead->nAllocLen);
            pInBuffHead->nFilledLen = nRead;
            pInBuffHead->nFlags = OMX_BUFFERFLAG_EOS;
            OMX_EmptyThisBuffer(pOMXHandle, pInBuffHead);
            OMX_FillThisBuffer(pOMXHandle, pOutBuffHead);
            break;

        case STATE_EMPTY_BUFFER_DONE_CALLED:

            eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
            if ( eError != OMX_ErrorNone ) {
                SkDebugf("Error from SendCommand-Idle(nStop) State function\n");
                iState = STATE_ERROR;
            }
            break;

        case STATE_LOADED:
        case STATE_ERROR:
            /*### Assume that the inStream will be closed by some upper layer */
            /*### Assume that the Bitmap object/file need not be closed. */
            /*### More needs to be done here */
            /*### Do different things based on iLastState */

            if (pOMXHandle) {
                eError = TIOMX_FreeHandle(pOMXHandle);
                if ( (eError != OMX_ErrorNone) )    {
                    SkDebugf("Error in Free Handle function\n");
                }
            }

            eError = TIOMX_Deinit();
            if ( eError != OMX_ErrorNone ) {
                SkDebugf("Error returned by TIOMX_Deinit()\n");
            }

            iState = STATE_EXIT;

            break;

        default:
            break;
    }

    if (iState == STATE_ERROR) sem_post(semaphore) ;
    if (iState == STATE_EXIT) break;
    }
}
