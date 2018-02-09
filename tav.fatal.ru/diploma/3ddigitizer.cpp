// 3ddigitizer.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "3ddigitizer.h"

HINSTANCE hInstance;
HWND hWnd;
HGLRC hRC;
HDC hDC;
int width=0,height=0,bpp=0,displayFreq=0;
double dT,aspectXY,fpsShowTime;

UINT64 prevTime,freq;

UINT64 GetTime()
{
	LARGE_INTEGER s;
	QueryPerformanceCounter(&s);
	return s.QuadPart;
}

double TimeDiff(UINT64 curTime,UINT64 prevTime)
{
	return double(((double)(signed __int64)(curTime-prevTime))/((double)(signed __int64)freq));
}

bool Key[256];

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
			SendMessage(hWnd,WM_CLOSE,0,0);
			break;
		case VK_SPACE:
			void OnSpace();
			OnSpace();
			break;
		default:
			Key[wParam]=true;
			break;
		}
		return 0;

	case WM_KEYUP:
		Key[wParam]=false;
		return 0;

	case WM_CLOSE:
		PostQuitMessage(1);
		break;

	default:
		return DefWindowProc(hWnd,uMsg,wParam,lParam);
	}

	return 0;
}

ubvec3 pic[100][256][256];
mat4 cam[LENGTH(pic)], camera;
vec3 pos[LENGTH(pic)];
int P = 0;

void OnSpace()
{
	cam[P] = camera;
	extern vec3 Eye;
	pos[P] = Eye;

	glFinish();
	glReadBuffer(GL_FRONT);
	glReadPixels(0,0,256,256,GL_RGB,GL_UNSIGNED_BYTE,pic[P++]);

	if (P == LENGTH(pic))
	{
		mat4 invcam = inverse(cam[0]);
		vec4 start(pos[0], 1);

		//for (int y=0; y<256; y++)
		//for (int x=0; x<256; x++)
		int x=128,y=128;
		{
			static ubvec3 color = pic[0][y][x];

			//if (color == unvec3(0)) continue;

			vec3 dir = normalize(( invcam * vec4(x/256.f*2 - 1.f, y/256.f*2 - 1.f, 1, 0) ).xyz());
#define RAY_LENGTH 1024
			vec4 end(start.xyz() + dir*RAY_LENGTH, 1);

			static HMubyte dists[RAY_LENGTH*32];//каждый элемент - кол-во расстояний, соответствующих данному элементу
			static vector<int> arr;
			static irange distsRange(0);
			memset(dists, 0, sizeof(dists));

			static int i=1;
			for (; i<LENGTH(pic); i++)
			{
				vec4 a = cam[i]*start, b = cam[i]*end;
				range r(0, RAY_LENGTH);
				if (ClipEdge(a, b, &r))
				{
					struct T {static void f(int x, int y, float d)//d - расстояние до данной точки от pos[0]
					{
						ivec3 v1=vec_cast<ivec3>(pic[i][y][x]),v2=vec_cast<ivec3>(color);
						ivec3 sub=abs(v1-v2);
						if (all(lessThan(sub, ivec3(20))))
						{
							int ind = int(d*32);
							//if (dists[ind] == 0) arr.push_back(ind);
							distsRange+=ind;
							dists[ind]++;
						}
					}};

#define CVV_TO_SCREEN(a) vec_cast<ivec2>(((a)+1.f)*128)

					Line(CVV_TO_SCREEN(a.xy()/a.w),
						 CVV_TO_SCREEN(b.xy()/b.w), T::f, r, a.w, b.w);
				}
			}

/*			struct T {static bool pred(int i1, int i2)
			{
				int d = (int)dists[i1] - (int)dists[i2];
				return d ? d < 0 : i1 < i2;
			}};
			sort(arr.begin(),arr.end(),T::pred);*/

			vector< vector<int> > V;
			vector<int> v;
			for (i = distsRange.min; i <= distsRange.max; i++)
				v.push_back(dists[i]);
			if (v.size()%2) v.push_back(0);
			V.push_back(v);

			while (V[V.size()-1].size() > 2)
			{
				vector<int> &p=V[V.size()-1],v;

				int n = p.size();
				for (int i=0; i<n; i+=2)
					v.push_back(p[i]+p[i+1]);

				if (v.size()%2) v.push_back(0);
				V.push_back(v);
			}

			//Находим наилучшее расстояние
			int pos=0;
			for (i=V.size()-1; i>=0; i--,pos*=2)
			{
				vector<int> &v=V[i];
				if (v[pos] == v[pos+1])
				{
					continue;
				}
				if (v[pos] > v[pos+1])
				{
					continue;
				}
				pos++;
			}
			pos/=2;

			float t=(pos+distsRange.min)/32.f, tt;
			segPlaneIntn(tt,start.xyz(),end.xyz(),plane(vec3(0,0,1),vec3(0)));
			tt*=RAY_LENGTH;
			i=5;
		}
	}
}

float alpha=radians(225.f),beta=radians(-45.f);
#define mouse_sensivity radians(70.0)

vec3 Eye(40);
GLuint tex;

void Loop()
{
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	GLfloat k=cos(beta);
	vec3 Look=vec3(cos(alpha)*k,sin(alpha)*k,sin(beta));
	glLoadMatrixf(camera=perspectiveMat<float>(radians(50.),aspectXY,5.,150.)*lookAtMat(Eye,Eye+Look,vec3(0,0,1)));

	glBindTexture(GL_TEXTURE_2D,tex);
	glEnable(GL_TEXTURE_2D);
	TexRecti(-10,-10,10,10);

	if (P > 0 && false)
	{
		mat4 invcam = inverse(cam[0]);
		int x=128,y=128;
		vec4 a(pos[0],1), b = vec4(pos[0]+100.f*normalize(( invcam * vec4(x/256.f*2 - 1.f, y/256.f*2 - 1.f, 1, 0) ).xyz()),1);
		/*glBegin(GL_LINES);
			glColor3f(0,1,0);
			glVertex4fv(a);
			glColor3f(0,0,1);
			glVertex4fv(b);
		glEnd();*/

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(0,256,0,256);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glDisable(GL_TEXTURE_2D);

		range r(0,1);
		a=camera*a;
		b=camera*b;
		if (ClipEdge(a,b,&r))
		{
			a.xyz()/=a.w; b.xyz()/=b.w;
			struct T
			{
				static void f(int x,int y,float k)
				{
					glColor3fv(lerp(vec3(0,1,0),vec3(0,0,1),k));
					glRectf(x,y+3,x+1,y+4);
				}
			};
			glColor3f(1,0,0);
			Line(ivec2(vec_cast<ivec4>((a+1.f)*128)),
				 ivec2(vec_cast<ivec4>((b+1.f)*128)), T::f, r, a.w, b.w);
			glColor3f(1,1,1);
		}
	}

	static bool pt=true;
	if (pt)
	{
	glLoadIdentity();
	glColor3f(.5,0,0);
	glBegin(GL_POINTS);
		glVertex2f(0,0);
	glEnd();
	glColor3f(1,1,1);
	if (Key[VK_RETURN]) pt=false;
	}

	glFlush();
	SwapBuffers(hDC);

	static float curShowedFPS=0;
	//Обновляем FPS и dT
	UINT64 curTime=GetTime();
	double dT=TimeDiff(curTime,prevTime);
	prevTime=curTime;
	fpsShowTime+=dT;
	static int totframes=0;
	totframes++;
	if (fpsShowTime>.5)
	{
		curShowedFPS=totframes/fpsShowTime;
		fpsShowTime=0;
		totframes=0;
	}
	if (dT>.1) dT=.1;

	const GLfloat speed=100.0f;//Скорость движения камеры
	if (Key[VK_UP] || (Key['W'] && !Key[VK_CONTROL])) Eye+=Look*(speed*dT);
	else if (Key[VK_DOWN] || Key['S']) Eye-=Look*(speed*dT);
	if (Key[VK_RIGHT] || Key['D']) {Eye.x+=sinf(alpha)*speed*dT;Eye.y-=cos(alpha)*speed*dT;}
	if (Key[VK_LEFT] || Key['A']) {Eye.x-=sinf(alpha)*speed*dT;Eye.y+=cos(alpha)*speed*dT;}

	POINT p;
	RECT rect;
	GetCursorPos(&p);
	GetWindowRect(hWnd,&rect);
	float cX=((GLfloat)(p.x-rect.left)/width)*2.0f-1.0f;
	float cY=((GLfloat)(rect.bottom-p.y)/height)*2.0f-1.0f;

	alpha=wrap(alpha-cX*mouse_sensivity,radians(360.0));
	beta+=cY*mouse_sensivity;
	SetCursorPos(width/2,height/2);
	if (beta>radians(89.0)) beta=radians(89.0);
	if (beta<-radians(89.0)) beta=-radians(89.0);/**/
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	HGLRC hRC;

	//Регистрируем класс окна
	WNDCLASS wc = {0};
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;//LoadIcon (NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "3d";
	RegisterClass(&wc);

	width=256;//GetSystemMetrics(SM_CXSCREEN);
	height=256;//GetSystemMetrics(SM_CYSCREEN);

	hWnd=CreateWindow(wc.lpszClassName, wc.lpszClassName,
		WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUPWINDOW | WS_VISIBLE,
		0, 0, width, height, NULL, NULL, hInstance, NULL);
	hDC = GetDC (hWnd);
	//Установка формата пикселя для OpenGL
	PIXELFORMATDESCRIPTOR pfd={0};
	int iFormat;
	pfd.nSize = sizeof (pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	iFormat = ChoosePixelFormat(hDC, &pfd);
	SetPixelFormat(hDC, iFormat, &pfd);

	//Создание контекста воспроизведения
	hRC = wglCreateContext(hDC);
	wglMakeCurrent(hDC, hRC);

	aspectXY=(GLdouble)width/height;

	ShowCursor(FALSE);
	SetCursorPos(width/2,height/2);

	tex = LoadJpg("tex.jpg");

	//Инициализируем таймер
	LARGE_INTEGER s;
	//получаем частоту
	QueryPerformanceFrequency(&s);
	//сохраняем её
	freq=s.QuadPart;

	prevTime=GetTime();

	MSG msg;
	while (true)
	{
		//Проверка наличия сообщения в очереди сообщений
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;
			else
			{
				//Передача сообщения в функцию обработки сообщений WndProc
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else Loop();//if (IsActive) Loop(); else WaitMessage();
	}

	//Завершение OpenGL
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	ReleaseDC(hWnd, hDC);

	//Уничтожаем окно
	DestroyWindow(hWnd);

	return (int) msg.wParam;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
