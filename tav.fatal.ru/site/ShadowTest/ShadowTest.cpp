/**************************
 * Includes
 *
 **************************/

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <gl/glext.h>
#include <mmsystem.h>
#include <stdio.h>
#include <math.h>

/**************************
 * Function Declarations
 *
 **************************/


LRESULT CALLBACK WndProc (HWND hWnd, UINT message,WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        return 0;
    case WM_CLOSE:
        PostQuitMessage (0);
        return 0;

    case WM_DESTROY:
        return 0;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            return 0;
        }
        return 0;

    default:
        return DefWindowProc (hWnd, message, wParam, lParam);
    }
}

HDC hDC;
HWND hWnd;
DWORD prevTime=0,fpsShowTime=0;

GLUquadricObj* qobj;
GLuint wirebox,wiretex,shadowtex[1];

#define LENGTH(arr) (sizeof(arr)/sizeof(arr[0]))

void DrawScene(int off=0,bool ending=true)
{
    glColor4ub(255,255,255,1+off);
    gluDisk(qobj,0,200,25,1);

    glColor4ub(0,255,0,2+off);
    glPushMatrix();
        glTranslatef(-5,-5,11);
        gluSphere(qobj,5,40,40);
    glPopMatrix();

    glColor4ub(0,0,255,3+off);
    glPushMatrix();
        glTranslatef(15,0,20);
        gluSphere(qobj,10,40,40);
    glPopMatrix();

    glColor4ub(255,255,255,4+off);
    glPushMatrix();
        gluCylinder(qobj,20,0,5,25,2);
    glPopMatrix();

    glColor4ub(0,255,255,5+off);
    glPushMatrix();
        glTranslatef(-20,-20,4);
        gluCylinder(qobj,15,15,6,25,2);
        glTranslatef(0,0,6);
        gluDisk(qobj,0,15,25,2);
    glPopMatrix();

    if (!ending) return;

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    glColor4ub(255,0,0,6+off);
    glPushMatrix();
        glTranslatef(0,20,14);
        glCallList(wirebox);
    glPopMatrix();

    glColor4ub(255,255,0,7+off);
    glPushMatrix();
        glTranslatef(-10,-10,30);
        glCallList(wirebox);
    glPopMatrix();
}

#define RENDER_TARGET_SIZE 512

void MainLoop()
{
    static float dT;
    static float theta = 0.0f;

    double a=theta*3.14159/180;
    GLfloat pos[4]={cos(a*10),-sin(a*10),1,0};

    //рендерим тени в текстуру
    glScissor(0,0,RENDER_TARGET_SIZE,RENDER_TARGET_SIZE);
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0,0,RENDER_TARGET_SIZE,RENDER_TARGET_SIZE);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glOrtho(-50,50,-50,50,0,1000);
    gluLookAt(pos[0]*50,pos[1]*50,pos[2]*50,0,0,0,0,0,1);
    GLfloat mat[4][4];
    glGetFloatv(GL_MODELVIEW_MATRIX,mat[0]);
    for (int i=0;i<4;i++)
        for (int j=0;j<i;j++)
        {
            GLfloat t=mat[i][j];
            mat[i][j]=mat[j][i];
            mat[j][i]=t;
        }

    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    DrawScene();
    glBindTexture(GL_TEXTURE_2D,shadowtex[0]);
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,RENDER_TARGET_SIZE,RENDER_TARGET_SIZE);
glPopAttrib();

    //рисуем сцену
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(50,1,1,10000);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(100*cos(a),100*sin(a),50,0,0,0,0,0,1);

    //формируем буфер глубины
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    DrawScene();
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

    glLightfv(GL_LIGHT0,GL_POSITION,pos);

    glEnable(GL_LIGHTING);
    glColorMaterial(GL_FRONT_AND_BACK,GL_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    glTexGenfv(GL_S,GL_EYE_PLANE,mat[0]);
    glTexGenfv(GL_T,GL_EYE_PLANE,mat[1]);
    //glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);
    //glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE_ARB);
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB_ARB,GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB_ARB,GL_PRIMARY_COLOR_ARB);
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA_ARB,GL_SUBTRACT_ARB);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA_ARB,GL_PRIMARY_COLOR_ARB);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_ALPHA_ARB,GL_TEXTURE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_EQUAL,1.f/255);

    glEnable(GL_TEXTURE_2D);
    glTexCoord4f(0,0,0,1);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(.5,.5,1);
    glTranslatef(1,1,0);
    glMatrixMode(GL_MODELVIEW);

    for (int i=-1;i<=1;i+=2)
    for (int j=-1;j<=1;j+=2)
    {
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
            glTranslatef((float)i/RENDER_TARGET_SIZE,(float)j/RENDER_TARGET_SIZE,0);
        glMatrixMode(GL_MODELVIEW);

        DrawScene(1,i==1 && j==1);

        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

//    DrawScene(1);
    glDisable(GL_ALPHA_TEST);
/*glBegin(GL_QUADS);
           // glTexCoord2f(0,0);
            glVertex3f(-100,-100,0);
           // glTexCoord2f(1,0);
            glVertex3f( 100,-100,0);
           // glTexCoord2f(1,1);
            glVertex3f( 100, 100,0);
           // glTexCoord2f(0,1);
            glVertex3f(-100, 100,0);
            glEnd();
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
*/
    DWORD t=timeGetTime();
    dT=int(t-prevTime)/1000.f;
    if (t!=prevTime)
    {
        fpsShowTime+=int(t-prevTime);
        if (fpsShowTime>=500)
        {
            fpsShowTime=0;
            char cap[100];
            sprintf(cap,"ShadowTest (FPS: %i)",1000/int(t-prevTime));
            SetWindowText(hWnd,cap);
        }
    }
    prevTime=t;

    glFinish();
    SwapBuffers(hDC);
    theta+=15*dT;
}

int WINAPI WinMain (HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         LPSTR lpCmdLine,
         int iCmdShow)
{
    HGLRC hRC;
    WNDCLASS wc;
    MSG msg;

    /* register window class */
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "ShadowTest";
    RegisterClass (&wc);

    /* create main window */
    hWnd = CreateWindow (
      "ShadowTest", "ShadowTest",
      WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE,
      0, 0, 600, 600,
      NULL, NULL, hInstance, NULL);

    /* enable OpenGL for the window */
    PIXELFORMATDESCRIPTOR pfd;
    int iFormat;

    /* get the device context (DC) */
    hDC = GetDC (hWnd);

    /* set the pixel format for the DC */
    ZeroMemory (&pfd, sizeof (pfd));
    pfd.nSize = sizeof (pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    iFormat = ChoosePixelFormat (hDC, &pfd);
    SetPixelFormat (hDC, iFormat, &pfd);

    /* create and enable the render context (RC) */
    hRC = wglCreateContext( hDC );
    wglMakeCurrent( hDC, hRC );

    //Initialization
    glClearColor (0.0f, 0.0f, 0.0f, 0.0f);

    qobj=gluNewQuadric();
    //gluQuadricNormals(qobj,GLU_SMOOTH);
    //gluQuadricTexture(qobj,GL_TRUE);

    glEnable(GL_LIGHT0);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);

    //gen wire box
    wirebox=glGenLists(1);
    glGenTextures(1,&wiretex);
    glBindTexture(GL_TEXTURE_2D,wiretex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    GLubyte pixels[16*16]={
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255, 0 , 0 , 0 , 0 , 0 , 0 ,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    };
    glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,16,16,0,GL_ALPHA,GL_UNSIGNED_BYTE,pixels);
    glNewList(wirebox,GL_COMPILE);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D,wiretex);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_NOTEQUAL,0.0f);
        glBegin(GL_QUADS);
            glNormal3f(1,0,0);//yz
            glTexCoord2f(0,0);
            glVertex3f(10,-10,-10);
            glTexCoord2f(1,0);
            glVertex3f(10, 10,-10);
            glTexCoord2f(1,1);
            glVertex3f(10, 10, 10);
            glTexCoord2f(0,1);
            glVertex3f(10,-10, 10);

            glNormal3f(0,1,0);//zx
            glTexCoord2f(0,0);
            glVertex3f(-10,10,-10);
            glTexCoord2f(1,0);
            glVertex3f(-10,10, 10);
            glTexCoord2f(1,1);
            glVertex3f(10, 10, 10);
            glTexCoord2f(0,1);
            glVertex3f(10, 10,-10);

            glNormal3f(0,0,1);//xy
            glTexCoord2f(0,0);
            glVertex3f(-10,-10,10);
            glTexCoord2f(1,0);
            glVertex3f( 10,-10,10);
            glTexCoord2f(1,1);
            glVertex3f( 10, 10,10);
            glTexCoord2f(0,1);
            glVertex3f(-10, 10,10);

            glNormal3f(-1,0,0);//zy
            glTexCoord2f(0,0);
            glVertex3f(-10,-10,-10);
            glTexCoord2f(1,0);
            glVertex3f(-10,-10, 10);
            glTexCoord2f(1,1);
            glVertex3f(-10, 10, 10);
            glTexCoord2f(0,1);
            glVertex3f(-10, 10,-10);

            glNormal3f(0,-1,0);//xz
            glTexCoord2f(0,0);
            glVertex3f(-10,-10,-10);
            glTexCoord2f(1,0);
            glVertex3f( 10,-10,-10);
            glTexCoord2f(1,1);
            glVertex3f(10, -10, 10);
            glTexCoord2f(0,1);
            glVertex3f(-10,-10, 10);

            glNormal3f(0,0,-1);//yx
            glTexCoord2f(0,0);
            glVertex3f(-10,-10,-10);
            glTexCoord2f(1,0);
            glVertex3f(-10, 10,-10);
            glTexCoord2f(1,1);
            glVertex3f( 10, 10,-10);
            glTexCoord2f(0,1);
            glVertex3f(10,-10, -10);
        glEnd();
        glPopAttrib();
    glEndList();

    //gen shadow tex
    glGenTextures(LENGTH(shadowtex),shadowtex);
    glBindTexture(GL_TEXTURE_2D,shadowtex[0]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    //Вместо GL_RGBA можно использовать GL_ALPHA, но так работает быстрее
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,RENDER_TARGET_SIZE,RENDER_TARGET_SIZE,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);

    timeBeginPeriod(1);

    prevTime=timeGetTime();
    /* program main loop */
    while (true)
    {
        /* check for messages */
        if (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
        {
            /* handle or dispatch messages */
            if (msg.message == WM_QUIT)
                break;
            else
            {
                TranslateMessage (&msg);
                DispatchMessage (&msg);
            }
        }
        else
            MainLoop();
    }

    gluDeleteQuadric(qobj);
    glDeleteLists(wirebox,1);
    glDeleteTextures(1,&wiretex);
    glDeleteTextures(LENGTH(shadowtex),shadowtex);
    /* shutdown OpenGL */
    wglMakeCurrent (NULL, NULL);
    wglDeleteContext (hRC);
    ReleaseDC (hWnd, hDC);

    /* destroy the window explicitly */
    DestroyWindow (hWnd);

    return msg.wParam;
}
