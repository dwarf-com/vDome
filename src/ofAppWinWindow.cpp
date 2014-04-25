#include "ofAppWinWindow.h"
#include "ofBaseApp.h"
#include "ofEvents.h"
#include "ofUtils.h"
#include "ofGraphics.h"
#include "ofAppRunner.h"
#include "ofConstants.h"
#include "ofGLProgrammableRenderer.h"
#include <windows.h>
#include <process.h>
#include <tchar.h>
#include <strsafe.h>

// DATAPATH
#include <rgb.h>
#include <rgbapi.h>
#include <rgberror.h>
#include <synchapi.h>



// OF
static int			offsetX;
static int			offsetY;
static int			windowMode;
static bool			bNewScreenMode;
static int			buttonInUse;
static bool			buttonPressed;
static bool			bEnableSetupScreen;
static bool			bDoubleBuffered; 
static int			windowW;
static int			windowH;
static int          nFramesSinceWindowResized;
static ofBaseApp *		ofAppPtr;
void ofGLReadyCallback();

// WIN
static HINSTANCE hInstance;
static const wchar_t CLASS_NAME[]  = L"Sample Window Class";
static HWND hWnd;
static HDC   hDC; 
static HGLRC hRC; 

static LONG WINAPI WndProc (HWND, UINT, WPARAM, LPARAM); 
static BOOL bSetupPixelFormat(HDC);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// THREAD
HANDLE               renderEvent;
HANDLE               renderThread = NULL;
HANDLE               gHMutex;
BOOL                 gThreadRunning = TRUE;
unsigned __stdcall WaitRender(void *data);


string getErrorString(int error);

// DATAPATH
#define NUM_BUFFERS 10



ofTexture tex[NUM_BUFFERS];




#define RGB_UNKNOWN  0
#define RGB_565      1
#define RGB_24       2
#define RGB_888      3
#define FRGB565      16
#define FRGB24       24
#define FRGB888      32

#define OneSecond 1000*1000*1000L

HRGB		hRGB; 
HRGBDLL		hRGBDLL;
SIGNALTYPE	signalType;
SIGNALTYPE  gSignalType;

unsigned long		error;
unsigned long		pNumberOfInputs;
signed long			pBIsSupported;
unsigned long		pCaptureWidth;
unsigned long		pCaptureHeight;
unsigned long		pRefreshRate;
unsigned long       gWidth = 2048;
unsigned long       gHeight = 2048;
unsigned long       gRefreshRate;
unsigned long       uInput = 0;
LPBITMAPINFO        gPBitmapInfo[NUM_BUFFERS]  = { NULL };

GPUTRANSFERDESCRIPTOR gpuTransfer;
GRAPHICSHARDWARE     gHardware;

/* possible values: FRGB565, FRGB24, FRGB888 */
unsigned int         gCaptureFormat = FRGB24; 
unsigned int         gColourFormat;
unsigned int         gByteFormat;
unsigned int         gRgbFormat;
unsigned int         gFormatSize;
unsigned int         gBufferIndex;
unsigned int         *gDataBuffer;
GLuint               gOGLBuffer[NUM_BUFFERS];
GLuint               gOGLTexture[NUM_BUFFERS];
BOOL                 gBChainBuffer = TRUE;
int                  first_capture = 0;

// color masks
static struct 
{
   COLORREF Mask[4];
}  ColourMasks[] =
{
   { 0x00000000, 0x00000000, 0x00000000, 0,},  /* Unknown */
   { 0x0000f800, 0x000007e0, 0x0000001f, 0,},  /* RGB565 */
   { 0x00ff0000, 0x0000ff00, 0x000000ff, 0,},  /* RGB24 */
   { 0x00ff0000, 0x0000ff00, 0x000000ff, 0,},  /* RGB888 */
};

// bitmap data
void
CreateBitmapInformation (
   BITMAPINFO  *pBitmapInfo,
   int         width,
   int         height,
   int         bitCount )
{
   pBitmapInfo->bmiHeader.biWidth          = width;
   pBitmapInfo->bmiHeader.biHeight         = -height;
   pBitmapInfo->bmiHeader.biBitCount       = bitCount;
   pBitmapInfo->bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
   pBitmapInfo->bmiHeader.biPlanes         = 1;
   pBitmapInfo->bmiHeader.biSizeImage      = 0;
   pBitmapInfo->bmiHeader.biXPelsPerMeter  = 3000;
   pBitmapInfo->bmiHeader.biYPelsPerMeter  = 3000;
   pBitmapInfo->bmiHeader.biClrUsed        = 0;
   pBitmapInfo->bmiHeader.biClrImportant   = 0;
   pBitmapInfo->bmiHeader.biSizeImage      = width * height * bitCount / 8 ;

   switch ( bitCount )
   {
      case 16:
      {
         memcpy ( &pBitmapInfo->bmiColors, &ColourMasks[RGB_565], 
               sizeof(ColourMasks[RGB_565]) );
         gFormatSize = 2;
         gColourFormat = GL_RGB;
         gByteFormat = GL_UNSIGNED_SHORT_5_6_5;
         gRgbFormat = RGB_PIXELFORMAT_565;

         pBitmapInfo->bmiHeader.biCompression    = BI_BITFIELDS;
         break;
      }

      case 24:
      {
         memcpy ( &pBitmapInfo->bmiColors, &ColourMasks[RGB_24], 
               sizeof(ColourMasks[RGB_24]) );
         gFormatSize = 3;
         gColourFormat = GL_BGR_EXT;
         gByteFormat = GL_UNSIGNED_BYTE;
         gRgbFormat = RGB_PIXELFORMAT_RGB24;

         pBitmapInfo->bmiHeader.biCompression    = BI_RGB;
         break;
      }

      case 32:
      {
         memcpy ( &pBitmapInfo->bmiColors, &ColourMasks[RGB_888], 
               sizeof(ColourMasks[RGB_888]) );
         gFormatSize = 4;
         gColourFormat = GL_BGRA_EXT;
         gByteFormat = GL_UNSIGNED_BYTE;
         gRgbFormat = RGB_PIXELFORMAT_888;

         pBitmapInfo->bmiHeader.biCompression    = BI_BITFIELDS;
         break;
      }

      default:
      {
         memcpy ( &pBitmapInfo->bmiColors, &ColourMasks[RGB_UNKNOWN], 
               sizeof(ColourMasks[RGB_UNKNOWN]) );
         gFormatSize = 0;

         pBitmapInfo->bmiHeader.biCompression    = BI_BITFIELDS;
         break;
      }
   }

   if (gFormatSize == 0)
        exit(0);
}
int currentBuffer = 0;
// next buffer
unsigned int 
findNextBuffer(
   unsigned int cb)
{
   unsigned int i;

   for (i=0; i<2; i++)
   {
      cb++;
      if (cb>=NUM_BUFFERS)
      {
         cb=0;
      }
   }

   currentBuffer = cb;
   return cb;
}



// capture callback
void RGBCBKAPI FrameCapturedFn(HWND hWnd, HRGB hRGB, LPBITMAPINFOHEADER pBitmapInfo, void *pBitmapBits, ULONG_PTR userData) {

   unsigned int i, next_buf;
   unsigned long  error;

   for ( i = 0; i < NUM_BUFFERS; i++ )
   {  

      if ( (pBitmapBits == (void *)gDataBuffer[i]) && pBitmapInfo 
               && gBChainBuffer )
      {
         /* Aquire Mutex. */
         WaitForSingleObject ( gHMutex, INFINITE );

         gBufferIndex = i;

         if (first_capture==0){
            first_capture=1;
         }

         next_buf = findNextBuffer(i);

         if (gHardware == GPU_NVIDIA)
         {
            RGBDirectGPUNVIDIAOp( hRGB, i, NVIDIA_GPU_COPY);
         }

         /* Release Mutex. */
         ReleaseMutex ( gHMutex );

         /* Sisgnal event to perform render operation on the main thread */
         SetEvent(renderEvent);

         /* Pass previous buffer back into the driver */
         error = RGBChainOutputBufferEx ( hRGB, gPBitmapInfo[next_buf], 
            (void *)gDataBuffer[next_buf], RGB_BUFFERTYPE_DIRECTGMA);

         break;
      }
   }

	//std::cout << "captured" << endl;
}





static void DatapathInit1(){
	hRGBDLL = 0;
	pNumberOfInputs = 0;
	first_capture = 0;

	int error = 0;
	error = RGBLoad(&hRGBDLL);
	//std::cout << getErrorString(error) << endl;

	error = RGBOpenInput(0, &hRGB);	
	//std::cout << getErrorString(error) << endl;

	error = RGBGetInputSignalType(uInput,&gSignalType,&gWidth,&gHeight,&gRefreshRate);
	//std::cout << getErrorString(error) << endl;

	int j;
    for (j=0; j<NUM_BUFFERS; j++)
    {
        gPBitmapInfo[j] = (LPBITMAPINFO) malloc ( 
        sizeof(BITMAPINFOHEADER) + ( 3 * sizeof(DWORD) ) );
        CreateBitmapInformation ( gPBitmapInfo[j], gWidth,
        gHeight, gCaptureFormat );
    }

	// create window
}






void 
LoadGLTextures() 
{
   unsigned int i;

   /* Generate texture name for gOGLTexture. */
   glGenTextures(NUM_BUFFERS,gOGLTexture);

   for (i=0; i<NUM_BUFFERS; i++)
   {
      /* Bind gOGLTexture to our texture container. */
      glBindTexture(GL_TEXTURE_2D, gOGLTexture[i]);

      glTexImage2D(GL_TEXTURE_2D, 0, 3, gWidth, gHeight, 0, gColourFormat,
         gByteFormat, NULL);

      /* Linear filtering. */
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); 
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

      /* Unbind gOGLTexture from OpenGL texture container. */
      glBindTexture(GL_TEXTURE_2D, 0);
   }

   
	for (i = 0; i <NUM_BUFFERS; i++) {
		tex[i].setUseExternalTextureID(gOGLTexture[i]);
		tex[i].texData.width = 2048;
		tex[i].texData.height = 2048;
		tex[i].texData.tex_w = 2048;
		tex[i].texData.tex_h = 2048;
		tex[i].texData.textureTarget = GL_TEXTURE_2D;
	}
}


int
DrawGLScene(
   GLvoid )
{


   /* Activate Sync to VBlank to avoid tearing. */
   //wglSwapIntervalEXT(1);





	  //  ReleaseMutex ( gHMutex );


   return TRUE;
}




static void DatapathInit2(){
	   
	/* OpenGL pixel format. */
   GLuint      PixelFormat;

	LoadGLTextures();

	glGenBuffers(NUM_BUFFERS, &gOGLBuffer[0]);

    /* Fill in structure to define parameters for the
    AMD DirectGMA functionality. */
    gpuTransfer.Size = sizeof(GPUTRANSFERDESCRIPTOR);
    gpuTransfer.Buffer = &gDataBuffer;
	gpuTransfer.Width = gWidth;
    gpuTransfer.Height = gHeight;
    gpuTransfer.OglByteFormat = gByteFormat;
    gpuTransfer.OglColourFormat = gColourFormat;
    gpuTransfer.FormatSize = gFormatSize;
    gpuTransfer.OglObject = &gOGLBuffer[0];
    gpuTransfer.NumBuffers = NUM_BUFFERS;

    /* Try to initialize capture driver to 
    AMD GPU communication. */
	gHardware = GPU_NVIDIA;
    gpuTransfer.GpuBrand = gHardware;
    error = RGBDirectGPUInit( hRGB, &gpuTransfer );

    if ( error )
    {
    /* Try to initialize capture driver to 
        NVidia GPU communication. */

    }
    if (error != 0)
    {
		MessageBox(NULL,TEXT("Couldn't start GPU communication module."),TEXT("ERROR"),
		 MB_OK|MB_ICONEXCLAMATION);
		RGBCloseInput ( hRGB );
		exit(0);
    }


    /* The size of the buffer allocated is stored in
    the field bufferSize. The size of the buffer is assigned to the
    bitmap header. */
    for (int j=0; j<NUM_BUFFERS; j++)
    {
		gPBitmapInfo[j]->bmiHeader.biSizeImage = gpuTransfer.BufferSize;
    }
}

GLsync Fence;

void ofAppWinWindow::DatapathBind() {


   /* Enables depth testing. */
   //glEnable(GL_DEPTH_TEST);

   /* Enable smooth shading. */
  // glShadeModel(GL_SMOOTH);

   /* Set rasterization mode. */
   //glPolygonMode(GL_FRONT, GL_FILL);

   /* Set type of perspective calculations. */
 //  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);


  if (first_capture==1)
   {

	 /* Enable texturing. */
	   glEnable(GL_TEXTURE_2D);


	  //WaitForSingleObject ( gHMutex, INFINITE );

      if (gHardware == GPU_NVIDIA)
      {
         RGBDirectGPUNVIDIAOp( hRGB, gBufferIndex, NVIDIA_GPU_WAIT);
      }

      /* Clear screen and depth buffer. */
     // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      /* Reset the current matrix. */
      //glLoadIdentity();

      //glBindTexture(GL_TEXTURE_2D, gOGLTexture[gBufferIndex]);

	  tex[gBufferIndex].bind();

      //This two lines copy to the GPU
      //glBindBuffer(GL_PIXEL_UNPACK_BUFFER,gOGLBuffer[gBufferIndex]);   	
      //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gWidth, gHeight, gColourFormat,
     //    gByteFormat, NULL);

      /* Insert Sync object to check for completion. */
      Fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
  }
}


void ofAppWinWindow::DatapathUnbind() {
	if (first_capture==1){
		if (glIsSync(Fence))
		{
			glClientWaitSync(Fence, GL_SYNC_FLUSH_COMMANDS_BIT, OneSecond);
			glDeleteSync(Fence);
		}

		if (gHardware == GPU_NVIDIA)
		{
			RGBDirectGPUNVIDIAOp( hRGB, gBufferIndex, NVIDIA_GPU_END);
		}

		tex[gBufferIndex].unbind();
		//glBindTexture(GL_TEXTURE_2D, 0);
	}
}


static void DatapathInit3(){

	// create thread 


	/* Maximise the capture rate. */
    RGBSetFrameDropping ( hRGB, 0 );

	/* Set Capture format. */
    error = RGBSetPixelFormat ( hRGB, PIXELFORMAT(gRgbFormat) );
	//std::cout << getErrorString(error) << endl;

	error = RGBSetFrameCapturedFn(hRGB, FrameCapturedFn, 0);
	//std::cout << getErrorString(error) << endl;

	 error = RGBSetOutputSize (hRGB, gWidth, gHeight);
	//std::cout << getErrorString(error) << endl;

	int j;

    /* Pass buffers to Vision Capture driver.*/
    for (j =0; j < NUM_BUFFERS; j++)
    {
		error = RGBChainOutputBufferEx ( hRGB, gPBitmapInfo[j], 
		 (void *)gDataBuffer[j], RGB_BUFFERTYPE_DIRECTGMA);
    }

    error = RGBUseOutputBuffers ( hRGB, TRUE );
	//std::cout << getErrorString(error) << endl;

	if ( error == 0 ){
		gBChainBuffer = TRUE;
    }

	error = RGBStartCapture(hRGB);
	//std::cout << getErrorString(error) << endl;
}




//----------------------------------------------------------
// Constructor
//----------------------------------------------------------
ofAppWinWindow::ofAppWinWindow(){
	windowMode			= OF_WINDOW;
	bNewScreenMode		= true;
	buttonInUse			= 0;
	buttonPressed		= false;
	bEnableSetupScreen	= true;
}

//----------------------------------------------------------
// Create window
//----------------------------------------------------------
void ofAppWinWindow::setupOpenGL(int w, int h, int screenMode){

	// create window class
	WNDCLASS wc = { };
	hInstance = GetModuleHandle(NULL);

    wc.style         = 0; 
    wc.lpfnWndProc   = (WNDPROC)WndProc; 
    wc.cbClsExtra    = 0; 
    wc.cbWndExtra    = 0; 
    wc.hInstance     = hInstance; 
    wc.hIcon         = LoadIcon (hInstance, CLASS_NAME); 
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW); 
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); 
    wc.lpszMenuName  = CLASS_NAME; 
    wc.lpszClassName = CLASS_NAME; 

	RegisterClass(&wc);

		
	// datapath
	DatapathInit1();


    // create window
	hWnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"My Window",    // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
		0, 0, w+16, h+38,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
        );

    if (hWnd == NULL)
		std:: cout << "could not create handle" << endl;


	// show window
    ShowWindow(hWnd, SW_SHOWNORMAL);


	// update window
	UpdateWindow(hWnd);

	// get difference between window and drawing area
	RECT rc = { 0, 0, w, h } ;
	GetClientRect(hWnd, &rc);

	// set screen size vars
	offsetX = w - rc.right;
	offsetY = h - rc.bottom;

	windowW = w;
	windowH = h;
}





//----------------------------------------------------------
// Setup pixel format
//----------------------------------------------------------
static BOOL bSetupPixelFormat(HDC hDC) { 
    PIXELFORMATDESCRIPTOR pfd, *ppfd; 
    int pixelformat; 
 
    ppfd = &pfd; 
 
    ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR); 
    ppfd->nVersion = 1; 
    ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |  
                        PFD_DOUBLEBUFFER; 
    ppfd->dwLayerMask = PFD_MAIN_PLANE; 
    ppfd->iPixelType = PFD_TYPE_RGBA; 
    ppfd->cColorBits = 16; 
    ppfd->cDepthBits = 16; 
    ppfd->cAccumBits = 16; 
    ppfd->cStencilBits = 16; 
 
    if ( (pixelformat = ChoosePixelFormat(hDC, ppfd)) == 0 ) { 
		MessageBox(NULL, TEXT("ChoosePixelFormat failed"),  TEXT("Error"), MB_OK); 
        return FALSE; 
    } 
 
    if (SetPixelFormat(hDC, pixelformat, ppfd) == FALSE) { 
        MessageBox(NULL, TEXT("SetPixelFormat failed"), TEXT("Error"), MB_OK); 
        return FALSE; 
    } 
 
    return TRUE; 
} 

//----------------------------------------------------------
// Setup gl rendering context, called from winProc 
//----------------------------------------------------------
void wSetupContext(HWND &hwnd){

	RECT rect;	
    hDC = GetDC(hwnd); 

    if (!bSetupPixelFormat(hDC)) 
        PostQuitMessage (0); 
    hRC = wglCreateContext(hDC); 
    wglMakeCurrent(hDC, hRC); 
    GetClientRect(hwnd, &rect);
	ofGLReadyCallback();

	// datapath
	DatapathInit2();
}

//----------------------------------------------------------
// Rendering loop
//----------------------------------------------------------
void ofAppWinWindow::runAppViaInfiniteLoop(ofBaseApp * appPtr){
	ofAppPtr = appPtr;


	// setup
	ofNotifySetup();
	

	// thread -> place after setup, else shader init will fail
	wglMakeCurrent(NULL,NULL);
    renderEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    renderThread = (HANDLE)_beginthreadex(NULL,0,&WaitRender,
		(void*)&gThreadRunning,0,NULL);

	gHMutex = CreateMutexW( NULL, FALSE, NULL );


	// datapath
	DatapathInit3();

	// event -> update -> draw loop
	MSG msg;
    //while (1) { 
        while ( GetMessage ( &msg, NULL, 0,0 )) { 
			if (msg.message == WM_TIMER) { 
				msg.hwnd = hWnd; 
			} 
            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
        } 
		//ofNotifyUpdate();
		//display();	
  //  } 
}

//----------------------------------------------------------
// Main draw function
//----------------------------------------------------------
void ofAppWinWindow::display(void){


	 WaitForSingleObject ( gHMutex, INFINITE );

	ofPtr<ofGLProgrammableRenderer> renderer = ofGetGLProgrammableRenderer();
	if(renderer){
		renderer->startRender();
	}
	
	// set viewport, clear the screen
	ofViewport();
	
	float * bgPtr = ofBgColorPtr();
	ofClear(bgPtr[0]*255,bgPtr[1]*255,bgPtr[2]*255, bgPtr[3]*255);

	// screen adjustments
	if (bEnableSetupScreen)
		ofSetupScreen();

	// call draw()
	ofNotifyDraw();

	//DrawGLScene();

	// swap
	SwapBuffers(hDC);

	if(renderer){
		renderer->finishRender();
	}
	ReleaseMutex ( gHMutex );

}

static bool shiftK = false;
static bool altK = false;
static bool ctrlK = false;

static int char2OFKey(TCHAR ch) {
	int key = ch;
	
	// letters
	 if (ch >= 65 && ch <= 90) {
		if (!shiftK)
			key = ch + 32;
	}
	else {
		switch(ch) {
			//numbers
			case 48: if (shiftK) key = 41; break;
			case 49: if (shiftK) key = 33; break;
			case 50: if (shiftK) key = 64; break;
			case 51: if (shiftK) key = 35; break;
			case 52: if (shiftK) key = 36; break;
			case 53: if (shiftK) key = 37; break;
			case 54: if (shiftK) key = 94; break;
			case 55: if (shiftK) key = 38; break;
			case 56: if (shiftK) key = 42; break;
			case 57: if (shiftK) key = 40; break;

			default:
				key = -1;
		}
	}

	return key;
}


static int wParam2OfKey(WPARAM wParam, LPARAM lParam) {

	int key = -1;
	TCHAR ch = TCHAR(wParam);

	switch(wParam) {
		case VK_F1:			key = OF_KEY_F1;		break;
		case VK_F2:			key = OF_KEY_F2;		break;
		case VK_F3:			key = OF_KEY_F3;		break;
		case VK_F4:			key = OF_KEY_F4;		break;
		case VK_F5:			key = OF_KEY_F5;		break;
		case VK_F6:			key = OF_KEY_F6;		break;
		case VK_F7:			key = OF_KEY_F7;		break;
		case VK_F8:			key = OF_KEY_F8;		break;
		case VK_F9:			key = OF_KEY_F9;		break;
		case VK_F10:		key = OF_KEY_F10;		break;
		case VK_F11:		key = OF_KEY_F11;		break;
		case VK_F12:		key = OF_KEY_F12;		break;
		
		case VK_LEFT:		key = OF_KEY_LEFT;		break;
		case VK_RIGHT:		key = OF_KEY_RIGHT;		break;
		case VK_UP:			key = OF_KEY_UP;		break;
		case VK_DOWN:		key = OF_KEY_DOWN;		break;

		case VK_DELETE:		key = OF_KEY_DEL;		break;
		case VK_RETURN:		key = OF_KEY_RETURN;	break;
		case VK_ESCAPE:		key = OF_KEY_ESC;		break;
		case VK_TAB:		key = OF_KEY_TAB;		break;
		case VK_HOME:		key = OF_KEY_HOME;		break;
		case VK_END:		key = OF_KEY_END;		break;
		case VK_INSERT:		key = OF_KEY_INSERT;	break;
		
		case VK_CONTROL:  
			if( lParam & (1 << 24) )  
				key = OF_KEY_RIGHT_CONTROL;
			else
				key = OF_KEY_LEFT_CONTROL;
			break;
		
		case VK_SHIFT: 
			if( (((unsigned short)GetKeyState( VK_RSHIFT )) >> 15) != 1 )  
				key = OF_KEY_RIGHT_SHIFT;
			if( (((unsigned short)GetKeyState( VK_LSHIFT )) >> 15) != 1 ) 
				key = OF_KEY_LEFT_SHIFT;
			break;

		default:
			if (altK || ctrlK)
				key = char2OFKey(ch);
			break;
	}

	return key;
}

//----------------------------------------------------------
// WinProc callback
//----------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	RECT rect; 
    PAINTSTRUCT  ps; 
	POINT p;
	TCHAR ch;
	int key = -1;

	switch(msg){

    case WM_CREATE: 
		wSetupContext(hwnd);
		ofNotifyWindowEntry(0);
        break; 

    case WM_SIZE: 
		GetWindowRect(hwnd, &rect);
		ofNotifyWindowResized(rect.right, rect.bottom);
		GetClientRect(hwnd, &rect);
		windowW = rect.right;
		windowH = rect.bottom;
        break; 

	case WM_LBUTTONDOWN:
		buttonPressed = true;
		buttonInUse = OF_MOUSE_BUTTON_LEFT;
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		ofNotifyMousePressed(p.x, p.y, buttonInUse);
		break;

	case WM_RBUTTONDOWN:
		buttonPressed = true;
		buttonInUse = OF_MOUSE_BUTTON_RIGHT;
		POINT p;
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		ofNotifyMousePressed(p.x, p.y, buttonInUse);
		break;

	case WM_MBUTTONDOWN:
		buttonPressed = true;
		buttonInUse = OF_MOUSE_BUTTON_MIDDLE;
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		ofNotifyMousePressed(p.x, p.y, buttonInUse);
		break;

	case WM_LBUTTONUP:
		buttonPressed = false;
		buttonInUse = OF_MOUSE_BUTTON_LEFT;
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		ofNotifyMouseReleased(p.x, p.y, buttonInUse);
		break;

	case WM_RBUTTONUP:
		buttonPressed = false;
		buttonInUse = OF_MOUSE_BUTTON_RIGHT;
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		ofNotifyMouseReleased(p.x, p.y, buttonInUse);
		break;

	case WM_MBUTTONUP:
		buttonPressed = false;
		buttonInUse = OF_MOUSE_BUTTON_MIDDLE;
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		ofNotifyMouseReleased(p.x, p.y, buttonInUse);
		break;

	case WM_MOUSEMOVE:
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		ofNotifyMouseMoved(p.x, p.y);
		if (buttonPressed)
			ofNotifyMouseDragged(p.x, p.y, buttonInUse);
		break;

	case WM_CHAR: 
		ofNotifyKeyPressed((TCHAR) wParam);
		break;

	case WM_KEYDOWN: 
		if (wParam == VK_SHIFT)
			shiftK = true;
		else if (wParam == VK_CONTROL)
			ctrlK = false;
		key = wParam2OfKey(wParam, lParam);
		if (key > -1)
			ofNotifyKeyPressed(key);
		break;

	case WM_SYSKEYDOWN:
		// alt  
        if( wParam == VK_MENU ) {  
			altK = true;          
			if( lParam & (1 << 24) )
				ofNotifyKeyPressed(OF_KEY_RIGHT_ALT);
            else 
				ofNotifyKeyPressed(OF_KEY_LEFT_ALT);
        } 
		else {
			key = wParam2OfKey(wParam, lParam);
		}
		break;

	case WM_KEYUP: 
		if (wParam == VK_SHIFT)
			shiftK = true;
		else if (wParam == VK_CONTROL)
			ctrlK = false;
		key = wParam2OfKey(wParam, lParam);
		if (key > -1)
			ofNotifyKeyReleased(key);
		break;

	case WM_SYSKEYUP:
		// alt  
        if( wParam == VK_MENU ) {  
			altK = false;          
			if( lParam & (1 << 24) )
				ofNotifyKeyReleased(OF_KEY_RIGHT_ALT);
            else 
				ofNotifyKeyReleased(OF_KEY_LEFT_ALT);
        } 
		else {
			key = wParam2OfKey(wParam, lParam);
		}
		break;

    case WM_PAINT: 
		BeginPaint(hWnd, &ps); 
		EndPaint(hWnd, &ps); 
        break; 
	  
	  case WM_DROPFILES:
         break;
		

      case WM_CLOSE:
        OF_EXIT_APP(0);
		if (hRC) 
			wglDeleteContext(hRC); 
		if (hDC) 
            ReleaseDC(hWnd, hDC); 
        hRC = 0; 
        hDC = 0; 
        DestroyWindow (hWnd); 
      break;

      case WM_DESTROY: 
         OF_EXIT_APP(0);
		if (hRC) 
			wglDeleteContext(hRC); 
		if (hDC) 
            ReleaseDC(hWnd, hDC); 
        PostQuitMessage (0);
         break;

	  default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
      break;
    }

    return 0;
}



// thread
unsigned __stdcall WaitRender(void *data){

   BOOL *running;

   running = (BOOL*)data;

   if(!wglMakeCurrent(hDC,hRC)){
      MessageBox(NULL,TEXT("Can't Activate shared GL Rendering Context."),
         TEXT("ERROR"),MB_OK|MB_ICONEXCLAMATION);
   }

   while (*running){
      if (WaitForSingleObject(renderEvent,1) == WAIT_OBJECT_0) {
		ofAppWinWindow * window = (ofAppWinWindow*)ofGetWindowPtr();
		ofNotifyUpdate();
		window->display();
        //SwapBuffers(hDC);
      }
   }

   wglMakeCurrent(NULL,NULL);

   return 0;
}




//------------------------------------------------------------
void ofAppWinWindow::setWindowTitle(string title){
	std::wstring stemp = std::wstring(title.begin(), title.end());
	LPCWSTR sw = stemp.c_str();
	SetWindowTextW(hWnd, sw);
}

//------------------------------------------------------------
ofPoint ofAppWinWindow::getWindowSize(){
	RECT rect;
	GetClientRect(hWnd, &rect);
	return ofPoint(rect.right-rect.left,rect.bottom-rect.top,0);
}

//------------------------------------------------------------
ofPoint ofAppWinWindow::getWindowPosition(){
	RECT rect;
	GetWindowRect(hWnd, &rect);
	return  ofPoint(rect.left,rect.top,0);
}

//------------------------------------------------------------
int ofAppWinWindow::getWidth(){
	RECT rect;
	GetClientRect(hWnd, &rect);
	return rect.right-rect.left;
}

//------------------------------------------------------------
int ofAppWinWindow::getHeight(){
	RECT rect;
	GetClientRect(hWnd, &rect);
	return rect.bottom-rect.top;
}

//------------------------------------------------------------
void ofAppWinWindow::setWindowPosition(int x, int y){
	RECT rect;
	GetWindowRect(hWnd, &rect);
	SetWindowPos(hWnd, NULL, x, y, rect.right-rect.left, rect.bottom-rect.top, NULL);
}

//------------------------------------------------------------
void ofAppWinWindow::setWindowShape(int w, int h){
	RECT rect;
	GetWindowRect(hWnd, &rect);
	SetWindowPos(hWnd, NULL, rect.left, rect.top, w, h, NULL);
}

//------------------------------------------------------------
void ofAppWinWindow::hideCursor(){
	ShowCursor(false);
}

//------------------------------------------------------------
void ofAppWinWindow::showCursor(){
	ShowCursor(true);
}

//------------------------------------------------------------
void ofAppWinWindow::hideBorder(){
	SetWindowLong(hWnd, GWL_STYLE, 0);
	ShowWindow(hWnd, SW_SHOW);
	//setWindowShape(getWidth(), getHeight());
}

//------------------------------------------------------------
void ofAppWinWindow::showBorder(){
	SetWindowLong(hWnd, WS_OVERLAPPEDWINDOW, 0);
	ShowWindow(hWnd, SW_NORMAL);
}

//------------------------------------------------------------
void ofAppWinWindow::keepWindowOnTop(bool val){
	RECT rect;
	GetWindowRect(hWnd, &rect);

	if (val)
		SetWindowPos(hWnd, HWND_TOPMOST, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, NULL);
	else 
		SetWindowPos(hWnd, NULL, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, NULL);
}

//------------------------------------------------------------
int ofAppWinWindow::getWindowMode(){
	return windowMode;
}

//------------------------------------------------------------
void ofAppWinWindow::toggleFullscreen(){
	if( windowMode == OF_GAME_MODE)return;

	if( windowMode == OF_WINDOW ){
		windowMode = OF_FULLSCREEN;
	}else{
		windowMode = OF_WINDOW;
	}

	bNewScreenMode = true;
}

//------------------------------------------------------------
void ofAppWinWindow::setFullscreen(bool fullscreen){
    if( windowMode == OF_GAME_MODE)return;

    if(fullscreen && windowMode != OF_FULLSCREEN){
        bNewScreenMode  = true;
        windowMode      = OF_FULLSCREEN;
    }else if(!fullscreen && windowMode != OF_WINDOW) {
        bNewScreenMode  = true;
        windowMode      = OF_WINDOW;
    }
}

//------------------------------------------------------------
void ofAppWinWindow::enableSetupScreen(){
	bEnableSetupScreen = true;
}

//------------------------------------------------------------
void ofAppWinWindow::disableSetupScreen(){
	bEnableSetupScreen = false;
}

//------------------------------------------------------------
void ofAppWinWindow::setVerticalSync(bool bSync){
	if (bSync) {
		if (WGL_EXT_swap_control) {
			wglSwapIntervalEXT (1);
		}
	} else {
		if (WGL_EXT_swap_control) {
			wglSwapIntervalEXT (0);
		}
	}
}

























string getErrorString(int error) {
	switch(unsigned long(error)) {
		case RGBERROR_NO_ERROR: return "RGBERROR_NO_ERROR";
		case RGBERROR_DRIVER_NOT_FOUND: return "RGBERROR_DRIVER_NOT_FOUND";
		case RGBERROR_UNABLE_TO_LOAD_DRIVER: return "RGBERROR_UNABLE_TO_LOAD_DRIVER";
		case RGBERROR_HARDWARE_NOT_FOUND: return "RGBERROR_HARDWARE_NOT_FOUND";
		case RGBERROR_INVALID_INDEX: return "RGBERROR_INVALID_INDEX";               
		case RGBERROR_DEVICE_IN_USE: return "RGBERROR_DEVICE_IN_USE";                  
		case RGBERROR_INVALID_HRGBCAPTURE: return "RGBERROR_INVALID_HRGBCAPTURE";           
		case RGBERROR_INVALID_POINTER: return "RGBERROR_INVALID_POINTER";              
		case RGBERROR_INVALID_SIZE: return "RGBERROR_INVALID_SIZE";         
		case RGBERROR_INVALID_FLAGS: return "RGBERROR_INVALID_FLAGS";           
		case RGBERROR_INVALID_DEVICE: return "RGBERROR_INVALID_DEVICE";
		case RGBERROR_INVALID_INPUT: return "RGBERROR_INVALID_INPUT";         
		case RGBERROR_INVALID_FORMAT: return "RGBERROR_INVALID_FORMAT";                 
		case RGBERROR_INVALID_VDIF_CLOCKS: return "RGBERROR_INVALID_VDIF_CLOCKS";            
		case RGBERROR_INVALID_PHASE: return "RGBERROR_INVALID_PHASE";                 
		case RGBERROR_INVALID_BRIGHTNESS: return "RGBERROR_INVALID_BRIGHTNESS";            
		case RGBERROR_INVALID_CONTRAST: return "RGBERROR_INVALID_CONTRAST";            
		case RGBERROR_INVALID_BLACKLEVEL: return "RGBERROR_INVALID_BLACKLEVEL";            
		case RGBERROR_INVALID_SAMPLERATE: return "RGBERROR_INVALID_SAMPLERATE";              
		case RGBERROR_INVALID_PIXEL_FORMAT: return "RGBERROR_INVALID_PIXEL_FORMAT";          
		case RGBERROR_INVALID_HWND: return "RGBERROR_INVALID_HWND";                    
		case RGBERROR_INSUFFICIENT_RESOURCES: return "RGBERROR_INSUFFICIENT_RESOURCES";     
		case RGBERROR_INSUFFICIENT_BUFFERS: return "RGBERROR_INSUFFICIENT_BUFFERS";           
		case RGBERROR_INSUFFICIENT_MEMORY: return "RGBERROR_INSUFFICIENT_MEMORY";             
		case RGBERROR_SIGNAL_NOT_DETECTED: return "RGBERROR_SIGNAL_NOT_DETECTED";           
		case RGBERROR_INVALID_SYNCEDGE: return "RGBERROR_INVALID_SYNCEDGE";                
		case RGBERROR_OLD_FIRMWARE: return "RGBERROR_OLD_FIRMWARE";                
		case RGBERROR_HWND_AND_FRAMECAPTUREDFN: return "RGBERROR_HWND_AND_FRAMECAPTUREDFN";    
		case RGBERROR_HSCALED_OUT_OF_RANGE: return "RGBERROR_HSCALED_OUT_OF_RANGE";    
		case RGBERROR_VSCALED_OUT_OF_RANGE: return "RGBERROR_VSCALED_OUT_OF_RANGE";   
		case RGBERROR_SCALING_NOT_SUPPORTED: return "RGBERROR_SCALING_NOT_SUPPORTED";  
		case RGBERROR_BUFFER_TOO_SMALL: return "RGBERROR_BUFFER_TOO_SMALL";     
		case RGBERROR_HSCALE_NOT_WORD_DIVISIBLE: return "RGBERROR_HSCALE_NOT_WORD_DIVISIBLE"; 
		case RGBERROR_HSCALE_NOT_DWORD_DIVISIBLE: return "RGBERROR_HSCALE_NOT_DWORD_DIVISIBLE";
		case RGBERROR_HORADDRTIME_NOT_WORD_DIVISIBLE: return "RGBERROR_HORADDRTIME_NOT_WORD_DIVISIBLE";
		case RGBERROR_HORADDRTIME_NOT_DWORD_DIVISIBLE: return "RGBERROR_HORADDRTIME_NOT_DWORD_DIVISIBLE";
		case RGBERROR_VERSION_MISMATCH: return "RGBERROR_VERSION_MISMATCH";           
		case RGBERROR_ACC_REALLOCATE_BUFFERS: return "RGBERROR_ACC_REALLOCATE_BUFFERS";   
		case RGBERROR_BUFFER_NOT_VALID: return "RGBERROR_BUFFER_NOT_VALID";        
		case RGBERROR_BUFFERS_STILL_ALLOCATED: return "RGBERROR_BUFFERS_STILL_ALLOCATED";   
		case RGBERROR_NO_NOTIFICATION_SET: return "RGBERROR_NO_NOTIFICATION_SET";    
		case RGBERROR_CAPTURE_DISABLED: return "RGBERROR_CAPTURE_DISABLED";     
		case RGBERROR_INVALID_PIXELFORMAT: return "RGBERROR_INVALID_PIXELFORMAT";  
		case RGBERROR_ILLEGAL_CALL: return "RGBERROR_ILLEGAL_CALL";    
		case RGBERROR_CAPTURE_OUTSTANDING: return "RGBERROR_CAPTURE_OUTSTANDING";  
		case RGBERROR_MODE_NOT_FOUND: return "RGBERROR_MODE_NOT_FOUND";        
		case RGBERROR_CANNOT_DETECT: return "RGBERROR_CANNOT_DETECT";              
		case RGBERROR_NO_MODE_DATABASE: return "RGBERROR_NO_MODE_DATABASE";         
		case RGBERROR_CANT_DELETE_MODE: return "RGBERROR_CANT_DELETE_MODE";    
		case RGBERROR_MUTEX_FAILURE: return "RGBERROR_MUTEX_FAILURE";         
		case RGBERROR_THREAD_FAILURE: return "RGBERROR_THREAD_FAILURE";       
		case RGBERROR_NO_COMPLETION: return "RGBERROR_NO_COMPLETION";         
		case RGBERROR_INSUFFICIENT_RESOURCES_HALLOC: return "RGBERROR_INSUFFICIENT_RESOURCES_HALLOC";
		case RGBERROR_INSUFFICIENT_RESOURCES_RGBLIST: return "RGBERROR_INSUFFICIENT_RESOURCES_RGBLIST";
		case RGBERROR_DEVICE_NOT_READY: return "RGBERROR_DEVICE_NOT_READY";                
		case RGBERROR_HORADDRTIME_NOT_QWORD_DIVISIBLE: return "RGBERROR_HORADDRTIME_NOT_QWORD_DIVISIBLE"; 
		case RGBERROR_HSCALE_NOT_QWORD_DIVISIBLE: return "RGBERROR_HSCALE_NOT_QWORD_DIVISIBLE";     
		//case RGBERROR_INVALID_AOI: return "RGBERROR_INVALID_AOI";                     
		case RGBCAPTURE_AOI_NOT_QWORD_ALIGNED: return "RGBCAPTURE_AOI_NOT_QWORD_ALIGNED";        
		case RGBCAPTURE_AOI_NOT_DWORD_ALIGNED: return "RGBCAPTURE_AOI_NOT_DWORD_ALIGNED";         
		case RGBERROR_INVALID_HTIMINGS: return "RGBERROR_INVALID_HTIMINGS";                
		case RGBERROR_INVALID_PITCH: return "RGBERROR_INVALID_PITCH";                   
		case RGBERROR_INVALID_PIXELCOUNT: return "RGBERROR_INVALID_PIXELCOUNT";            
		case RGBERROR_FLASH_ONLY: return "RGBERROR_FLASH_ONLY";                      
		case RGBERROR_CPU_CODE_LOAD_FAILED: return "RGBERROR_CPU_CODE_LOAD_FAILED";            
		case RGBERROR_IRQ_LINE_FAILED: return "RGBERROR_IRQ_LINE_FAILED";                
		case RGBERROR_CPU_CODE_LOAD_ERROR: return "RGBERROR_CPU_CODE_LOAD_ERROR";           
		case RGBERROR_CPU_STARTUP_UNSIGNALLED: return "RGBERROR_CPU_STARTUP_UNSIGNALLED";        
		case RGBERROR_HW_INTEGRITY_VIOLATION: return "RGBERROR_HW_INTEGRITY_VIOLATION";         
		case RGBERROR_MEMORY_TEST_FAILED: return "RGBERROR_MEMORY_TEST_FAILED";             
		case RGBERROR_PCI_BUS_FAILURE: return "RGBERROR_PCI_BUS_FAILURE";              
		case RGBERROR_CPU_NO_HANDSHAKE: return "RGBERROR_CPU_NO_HANDSHAKE";           
		case RGBERROR_INVALID_VTIMINGS: return "RGBERROR_INVALID_VTIMINGS";         
		case RGBERROR_INVALID_ENVIRONMENT: return "RGBERROR_INVALID_ENVIRONMENT";   
		case RGBERROR_FILE_NOT_FOUND: return "RGBERROR_FILE_NOT_FOUND";     
		case RGBERROR_INVALID_GAIN: return "RGBERROR_INVALID_GAIN";          
		case RGBERROR_INVALID_OFFSET: return "RGBERROR_INVALID_OFFSET";        
		case RGBERROR_CANT_ADJUST_DVI: return "RGBERROR_CANT_ADJUST_DVI";         
		case RGBERROR_INCOMPATIBLE_INTERFACE: return "RGBERROR_INCOMPATIBLE_INTERFACE";    
		case RGBERROR_FLASH_INPROGRESS: return "RGBERROR_FLASH_INPROGRESS";        
		case RGBERROR_INVALID_SATURATION: return "RGBERROR_INVALID_SATURATION";       
		case RGBERROR_INVALID_HUE: return "RGBERROR_INVALID_HUE";           
		case RGBERROR_INVALID_VIDEOSTANDARD: return "RGBERROR_INVALID_VIDEOSTANDARD";    
		case RGBERROR_INVALID_DEINTERLACE: return "RGBERROR_INVALID_DEINTERLACE";     
		case RGBERROR_ROTATION_NOT_SUPPORTED: return "RGBERROR_ROTATION_NOT_SUPPORTED";    
		case RGBERROR_INVALID_ROTATION_ANGLE: return "RGBERROR_INVALID_ROTATION_ANGLE"; 
		case RGBERROR_EDID_NOT_SUPPORTED: return "RGBERROR_EDID_NOT_SUPPORTED";
		case RGBERROR_INCOMPATIBLE_HARDWARE: return "RGBERROR_INCOMPATIBLE_HARDWARE";     
		case RGBERROR_INVALID_EDID: return "RGBERROR_INVALID_EDID";        
		case RGBERROR_EDID_VERIFY: return "RGBERROR_EDID_VERIFY";           
		case RGBERROR_EDID_I2C_STUCK: return "RGBERROR_EDID_I2C_STUCK";        
		case RGBERROR_EDID_I2C_NOACK: return "RGBERROR_EDID_I2C_NOACK";       
		case RGBERROR_INVALID_EQUALISATION: return "RGBERROR_INVALID_EQUALISATION";
		case RGBERROR_INTERNAL_DEVICE_ERROR: return "RGBERROR_INTERNAL_DEVICE_ERROR";       
		case RGBERROR_INVALID_HWINDOW: return "RGBERROR_INVALID_HWINDOW";               
		case RGBERROR_NOACCESS: return "RGBERROR_NOACCESS";                    
		case RGBERROR_NOT_INITIALISED: return "RGBERROR_NOT_INITIALISED";        
		case RGBERROR_INVALID_HANDLE: return "RGBERROR_INVALID_HANDLE";              
		case RGBERROR_NO_AUDIO_AVAILABLE: return "RGBERROR_NO_AUDIO_AVAILABLE";        
		case RGBERROR_ALREADY_ACTIVE: return "RGBERROR_ALREADY_ACTIVE";           
		case RGBERROR_DIFFERENT_GPUDESCRIPTOR: return "RGBERROR_DIFFERENT_GPUDESCRIPTOR";
		case RGBERROR_INVALID_COLOURDOMAIN: return "RGBERROR_INVALID_COLOURDOMAIN"; 
		default: return ofToString(error);
	}
}