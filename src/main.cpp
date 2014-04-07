#include "ofMain.h"
#include "vdome.h"
#include "ofGlProgrammableRenderer.h"
#include "ofAppGLFWWindow.h"
#include "ofAppWinWindow.h"

int main( ){
	ofAppWinWindow window;
	//ofAppGLFWWindow window;
	ofSetCurrentRenderer(ofGLProgrammableRenderer::TYPE);
	ofSetupOpenGL(&window, 1024,768, OF_WINDOW);
	ofRunApp(new vdome());
}
