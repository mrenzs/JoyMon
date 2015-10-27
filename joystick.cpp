/*
 * Copyright 2002-2011 Giles Malet.
 * 
 * This file is part of JoyMon.
 * 
 * JoyMon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * JoyMon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with JoyMon.  If not, see <http://www.gnu.org/licenses/>.
 */

//----------------------------------------------------------------------------
// File: Joystick.cpp
//
// This file is derived from the Microsoft DirectX samples.
// The original contained this copyright notice:
//
// Copyright (c) 1998-2001 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#define DIRECTINPUT_VERSION 0x0800
#define _WIN32_WINNT 0x0500

#pragma warning( disable : 4995 ) // disable deprecated warning 
#pragma warning( disable : 4996 ) // disable deprecated warning 

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <basetsd.h>
#include <dinput.h>
#include <errno.h>
#include <io.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "resource.h"


//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
INT_PTR CALLBACK ConfigDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK    EnumObjectsCallback( const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext );
BOOL CALLBACK    EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance, VOID* pContext );
HRESULT InitDirectInput( HWND hDlg );
VOID    FreeDirectInput();
HRESULT UpdateInputState( HWND hDlg );
VOID    OnPaint( HWND hDlg );
HRESULT PollJoystick( DIJOYSTATE& js );
BOOL	CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam);
void	CheckJoystickButton( HWND hDlg );
bool	StartWriting( void );
bool	WriteToFile( void );
void	StopWriting( void );
bool	LoadConfig( void );
bool	SaveConfig( void );

//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

LPDIRECTINPUT8       g_pDI              = NULL;         
LPDIRECTINPUTDEVICE8 g_pJoystick        = NULL;     

bool g_bWriting = false, g_bWriteError = false;
HINSTANCE g_hInst;
FILE * fp = NULL;
DWORD g_timerstart;
char g_MsgText[512];

// The two buttons we monitor
static bool g_JoystickButton = false, g_Button2 = false;

static const char g_Version[] = "Version: " __DATE__ ", "  __TIME__;
static const char Title[] = "Joystick Monitor";

static struct {
	bool ShowAxes, ShowFilename, OutputFileBanner, OriginLowerLeft, DrawOctants, RememberWindow, SoundFeedback, SuppressX, SuppressY;
	long EllipseSize, XYMinMax, JoystickButton, Button2, WPosnX, WPosnY, WSizeX, WSizeY, GridCount, TickCount;
	double TicksPerSec;
	char FilePattern[MAX_PATH];	// Where to put output data. Will add 3 digit extension.
	char BannerComment[1024], LabelPosX[128], LabelPosY[128], LabelNegX[128], LabelNegY[128],
		LabelTopLeft[128], LabelTopRight[128], LabelBottomLeft[128], LabelBottomRight[128];
} g_Config;

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int APIENTRY WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    InitCommonControls();

    // Display the main dialog box.
	g_hInst = hInst; // needed to create config dialog.
	
#if 1
	DialogBox( hInst, MAKEINTRESOURCE(IDD_JOYST_IMM), NULL, MainDlgProc );
#else    
	HWND hDlg;
	MSG msg;
	BOOL ret;

	hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_JOYST_IMM), 0, MainDlgProc, 0);
	ShowWindow(hDlg, nCmdShow);

	while((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
		if(ret == -1)
			return -1;

		if(!IsDialogMessage(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
#endif

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: LoadConfig()
// Desc: Load the config from the registry.
//-----------------------------------------------------------------------------
bool LoadConfig( void )
{
	long lResult;
	HKEY hRegKey;
	unsigned long dwType, reglen;
	unsigned char regvalue[MAX_PATH];

	// Set some hopefully reasonable defaults
	g_Config.ShowAxes = false;
	g_Config.ShowFilename = true;
	g_Config.OutputFileBanner = true;
	g_Config.OriginLowerLeft = false;
	g_Config.DrawOctants = false;
	g_Config.RememberWindow = true;
	g_Config.EllipseSize = 2;
	g_Config.GridCount = 0;
	g_Config.TickCount = 0;
	g_Config.XYMinMax = 1000;
	g_Config.TicksPerSec = 2.0;
	g_Config.JoystickButton = 7;
	g_Config.Button2 = 1;
	g_Config.SoundFeedback = true;
	g_Config.SuppressX = false;
	g_Config.SuppressY = false;
	g_Config.WPosnX = 0;
	g_Config.WPosnY = 0;
	g_Config.WSizeX = 273;
	g_Config.WSizeY = 329;
	strncpy(g_Config.FilePattern, "c:\\Study 1\\Male41.", sizeof g_Config.FilePattern);
	strncpy(g_Config.BannerComment, "time,x-axis,y-axis,report status set to firing button 1 "
		"(when pressed writes 1 to file; otherwise writes 0); "
		"Double-click button 7 to stop; Adds increment on end of file to prevent inadvertent overwriting of file; "
		"To view file contents change extension to .csv or .txt and open into Excel or text editor.",
		sizeof g_Config.BannerComment);
	strncpy(g_Config.LabelPosX, "Friendly", sizeof g_Config.LabelPosX);
	strncpy(g_Config.LabelNegX, "Unfriendly", sizeof g_Config.LabelNegX);
	strncpy(g_Config.LabelPosY, "Dominant", sizeof g_Config.LabelPosY);
	strncpy(g_Config.LabelNegY, "Submissive", sizeof g_Config.LabelNegY);
	strncpy(g_Config.LabelTopLeft, "", sizeof g_Config.LabelTopLeft);
	strncpy(g_Config.LabelTopRight, "", sizeof g_Config.LabelTopRight);
	strncpy(g_Config.LabelBottomRight, "", sizeof g_Config.LabelBottomRight);
	strncpy(g_Config.LabelBottomLeft, "", sizeof g_Config.LabelBottomLeft);

	// Try read the per-user key. If this fails, there might be an older version of the config
	// (from a version of this program prior to Sept 2014) under LOCAL_MACHINE, which we try to read
	// instead. All writes now go to the per-user key.
	// This is all to avoid requiring admin rights to run.
	if ( (lResult = RegOpenKeyEx(
			HKEY_CURRENT_USER,
			"SOFTWARE\\JoystickMonitor",
			0,
			KEY_QUERY_VALUE,
			&hRegKey)) != 0 ) {
				if ( (lResult = RegOpenKeyEx(
						HKEY_LOCAL_MACHINE,
						"SOFTWARE\\JoystickMonitor",
						0,
						KEY_QUERY_VALUE,
						&hRegKey)) != 0 ) {
							SetLastError( lResult );
							return false;
			}
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"ShowAxes",
		0,
		&dwType,
		(LPBYTE)regvalue,
		&reglen)) == 0 ) {
			g_Config.ShowAxes = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"ShowFilename",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.ShowFilename = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"OutputFileBanner",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.OutputFileBanner = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"OriginLowerLeft",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.OriginLowerLeft = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"DrawOctants",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.DrawOctants = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"RememberWindow",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.RememberWindow = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"EllipseSize",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.EllipseSize = *((unsigned long*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"XYMinMax",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.XYMinMax = *((unsigned long*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"GridCount",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.GridCount = *((unsigned long*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"TickCount",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.TickCount = *((unsigned long*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"TicksPerSec",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
				// Convert old `long' format to new `float'. It's actually stored
				// as a double now so we can distinguish on type.
			if (dwType != REG_QWORD)
				g_Config.TicksPerSec = (double)*((unsigned long*)regvalue);
			else
				g_Config.TicksPerSec = *((double*)regvalue);
	}


	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"JoystickButton",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.JoystickButton = *((unsigned long*)regvalue);
			DIJOYSTATE js;
			if ( g_Config.JoystickButton < 1 || g_Config.JoystickButton > sizeof js.rgbButtons / sizeof js.rgbButtons[0] )
				g_Config.JoystickButton = 7;
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"Button2",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.Button2 = *((unsigned long*)regvalue);
			DIJOYSTATE js;
			if ( g_Config.JoystickButton < 1 || g_Config.JoystickButton > sizeof js.rgbButtons / sizeof js.rgbButtons[0] )
				g_Config.JoystickButton = 0;
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"SoundFeedback",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.SoundFeedback = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"SuppressX",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.SuppressX = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"SuppressY",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.SuppressY = *((bool*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"WindowPositionX",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.WPosnX = *((unsigned long*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"WindowPositionY",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.WPosnY = *((unsigned long*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"WindowSizeX",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.WSizeX = *((unsigned long*)regvalue);
	}

	reglen = sizeof regvalue;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"WindowSizeY",
		0,
		&dwType,
		regvalue,
		&reglen)) == 0 ) {
			g_Config.WSizeY = *((unsigned long*)regvalue);
	}

	reglen = sizeof g_Config.FilePattern;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"FilePattern",
		0,
		&dwType,
		(unsigned char*)&g_Config.FilePattern,
		&reglen)) == 0 ) {
			g_Config.FilePattern[ min( reglen, sizeof g_Config.FilePattern -1) ] = 0;
	}

	reglen = sizeof g_Config.BannerComment;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"BannerComment",
		0,
		&dwType,
		(unsigned char*)&g_Config.BannerComment,
		&reglen)) == 0 ) {
			g_Config.BannerComment[ min(reglen, sizeof g_Config.BannerComment -1) ] = 0;
	}

	reglen = sizeof g_Config.LabelPosX;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"LabelPosX",
		0,
		&dwType,
		(unsigned char*)&g_Config.LabelPosX,
		&reglen)) == 0 ) {
			g_Config.LabelPosX[ min(reglen, sizeof g_Config.LabelPosX -1) ] = 0;
	}

	reglen = sizeof g_Config.LabelNegX;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"LabelNegX",
		0,
		&dwType,
		(unsigned char*)&g_Config.LabelNegX,
		&reglen)) == 0 ) {
			g_Config.LabelNegX[ min(reglen, sizeof g_Config.LabelNegX -1) ] = 0;
	}

	reglen = sizeof g_Config.LabelPosY;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"LabelPosY",
		0,
		&dwType,
		(unsigned char*)&g_Config.LabelPosY,
		&reglen)) == 0 ) {
			g_Config.LabelPosY[ min(reglen, sizeof g_Config.LabelPosY -1) ] = 0;
	}

	reglen = sizeof g_Config.LabelNegY;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"LabelNegY",
		0,
		&dwType,
		(unsigned char*)&g_Config.LabelNegY,
		&reglen)) == 0 ) {
			g_Config.LabelNegY[ min(reglen, sizeof g_Config.LabelNegY -1) ] = 0;
	}

	reglen = sizeof g_Config.LabelTopLeft;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"LabelTopLeft",
		0,
		&dwType,
		(unsigned char*)&g_Config.LabelTopLeft,
		&reglen)) == 0 ) {
			g_Config.LabelTopLeft[ min(reglen, sizeof g_Config.LabelTopLeft -1) ] = 0;
	}

	reglen = sizeof g_Config.LabelTopRight;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"LabelTopRight",
		0,
		&dwType,
		(unsigned char*)&g_Config.LabelTopRight,
		&reglen)) == 0 ) {
			g_Config.LabelTopRight[ min(reglen, sizeof g_Config.LabelTopRight -1) ] = 0;
	}

	reglen = sizeof g_Config.LabelBottomRight;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"LabelBottomRight",
		0,
		&dwType,
		(unsigned char*)&g_Config.LabelBottomRight,
		&reglen)) == 0 ) {
			g_Config.LabelBottomRight[ min(reglen, sizeof g_Config.LabelBottomRight -1) ] = 0;
	}

	reglen = sizeof g_Config.LabelBottomLeft;
	if ( (lResult = RegQueryValueEx(
		hRegKey,
		"LabelBottomLeft",
		0,
		&dwType,
		(unsigned char*)&g_Config.LabelBottomLeft,
		&reglen)) == 0 ) {
			g_Config.LabelBottomLeft[ min(reglen, sizeof g_Config.LabelBottomLeft -1) ] = 0;
	}

	RegCloseKey( hRegKey );
	return true;
}

//-----------------------------------------------------------------------------
// Name: SaveConfig()
// Desc: Write the config to the registry.
//-----------------------------------------------------------------------------
bool SaveConfig( void )
{
	long lResult;
	HKEY hRegKey;
	unsigned long regvalue;

	if ( (lResult = RegCreateKeyEx(
			HKEY_CURRENT_USER,
			"SOFTWARE\\JoystickMonitor",
			0, "", 0,
			KEY_WRITE,
			NULL,
			&hRegKey,
			NULL)) != 0 ) {
				SetLastError( lResult );
				return false;
	}

	regvalue = g_Config.ShowAxes ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"ShowAxes",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	regvalue = g_Config.ShowFilename ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"ShowFileName",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	regvalue = g_Config.OutputFileBanner ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"OutputFileBanner",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	regvalue = g_Config.OriginLowerLeft ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"OriginLowerLeft",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	regvalue = g_Config.DrawOctants ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"DrawOctants",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	regvalue = g_Config.RememberWindow ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"RememberWindow",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"EllipseSize",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.EllipseSize,
			sizeof g_Config.EllipseSize)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"XYMinMax",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.XYMinMax,
			sizeof g_Config.XYMinMax)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"GridCount",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.GridCount,
			sizeof g_Config.GridCount)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"TickCount",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.TickCount,
			sizeof g_Config.TickCount)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"TicksPerSec",
			0,
			REG_QWORD, // note
			(unsigned char*)&g_Config.TicksPerSec,
			sizeof g_Config.TicksPerSec)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"JoystickButton",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.JoystickButton,
			sizeof g_Config.JoystickButton)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"Button2",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.Button2,
			sizeof g_Config.Button2)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	regvalue = g_Config.SoundFeedback ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"SoundFeedback",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	regvalue = g_Config.SuppressX ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"SuppressX",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	regvalue = g_Config.SuppressY ? 1 : 0;
	if ( (lResult = RegSetValueEx(
			hRegKey,
			"SuppressY",
			0,
			REG_DWORD,
			(unsigned char*)&regvalue,
			sizeof regvalue)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"WindowPositionX",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.WPosnX,
			sizeof g_Config.WPosnX)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"WindowPositionY",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.WPosnY,
			sizeof g_Config.WPosnY)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"WindowSizeX",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.WSizeX,
			sizeof g_Config.WSizeX)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"WindowSizeY",
			0,
			REG_DWORD,
			(unsigned char*)&g_Config.WSizeY,
			sizeof g_Config.WSizeY)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"FilePattern",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.FilePattern,
			strlen(g_Config.FilePattern)+1)) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"BannerComment",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.BannerComment,
			strlen(g_Config.BannerComment)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"LabelPosX",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.LabelPosX,
			strlen(g_Config.LabelPosX)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"LabelNegX",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.LabelNegX,
			strlen(g_Config.LabelNegX)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"LabelPosY",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.LabelPosY,
			strlen(g_Config.LabelPosY)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"LabelNegY",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.LabelNegY,
			strlen(g_Config.LabelNegY)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"LabelTopLeft",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.LabelTopLeft,
			strlen(g_Config.LabelTopLeft)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"LabelTopRight",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.LabelTopRight,
			strlen(g_Config.LabelTopRight)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"LabelBottomRight",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.LabelBottomRight,
			strlen(g_Config.LabelBottomRight)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	if ( (lResult = RegSetValueEx(
			hRegKey,
			"LabelBottomLeft",
			0,
			REG_SZ,
			(unsigned char*)&g_Config.LabelBottomLeft,
			strlen(g_Config.LabelBottomLeft)+1) ) != 0 ) {
				SetLastError( lResult );
				RegCloseKey( hRegKey );
				return false;
	};

	RegCloseKey( hRegKey );
	return true;
}

//-----------------------------------------------------------------------------
// Name: MainDialogProc
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg ) 
    {
        case WM_INITDIALOG:

			if ( !LoadConfig() ) {
				MessageBox( NULL, TEXT("Problems reading the config from the registry. ") \
                    TEXT("Restored config may be incomplete."), Title, MB_ICONERROR | MB_OK );
			} else {
				_snprintf( g_MsgText, sizeof g_MsgText, "Click button %u to start", g_Config.JoystickButton );
			}

			if( FAILED( InitDirectInput( hDlg ) ) )
            {
                MessageBox( NULL, TEXT("Error Initializing DirectInput"), Title, MB_ICONERROR | MB_OK );
                EndDialog( hDlg, 0 );
				break;
			}

			// Make the joystick send us msgs
			if ( joySetCapture(hDlg, JOYSTICKID1, 0, TRUE) != JOYERR_NOERROR || 
				!g_pJoystick || g_pJoystick->Acquire() != S_OK )
			{ 
				MessageBeep(MB_ICONEXCLAMATION); 
				MessageBox(hDlg, "Couldn't initialize the joystick.", Title, MB_OK | MB_ICONEXCLAMATION);
				// Just continue....they have been warned, but can at least see the screen.
 				//PostMessage(hDlg,WM_CLOSE,0,0L); 
		    } 

			if ( g_Config.RememberWindow &&
					g_Config.WPosnX >= 0   && g_Config.WPosnY >= 0 &&
					g_Config.WSizeX >= 200 && g_Config.WSizeY >= 200 ) {
				MoveWindow( hDlg, g_Config.WPosnX, g_Config.WPosnY, g_Config.WSizeX, g_Config.WSizeY, TRUE );
			}

			// Make sure there is some activity so joystick buttons > 4 are noticed
			SetTimer( hDlg, 42, 100, NULL );
					
			break;

		case MM_JOY1BUTTONDOWN :               // button is down 
			// Note that this only picks up buttons 1--4, so we poll using OnjoystickButton below.
			if( (UINT)wParam & JOY_BUTTON2CHG) {

					// Toggle whether we show axis details
					g_Config.ShowAxes = !g_Config.ShowAxes;
			}
			break;

		case WM_TIMER:		// Timer 42 allows us to keep the GUI updated
		case MM_JOY1MOVE:	// changed position
            if( FAILED( UpdateInputState( hDlg ) ) )
            {
                MessageBox( NULL, TEXT("Error Reading Input State. ") \
                            TEXT("The monitor will now exit."), Title, 
                            MB_ICONERROR | MB_OK );
                EndDialog( hDlg, TRUE ); 
            }

			// Make sure all is well with writing.
			if ( g_bWriting && g_bWriteError )
			{
                MessageBox( NULL, TEXT("Error Writing Output File. ") \
                            TEXT("The monitor will now exit."), Title, 
                            MB_ICONERROR | MB_OK );
                EndDialog( hDlg, TRUE ); 
            }

			break; 

		case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
                case IDCANCEL:
				case ID_FILE_EXIT:
                    EndDialog( hDlg, 0 );
					break;

				case ID_EDIT_CONFIG:
					DialogBox( g_hInst, MAKEINTRESOURCE(IDD_CONFIG), hDlg, ConfigDlgProc );
					break;

				default:
					return FALSE;
			}
			break;

        case WM_SIZE:   // main window changed size 
			{
			    RECT rcClient; 
				// Get the dimensions of the main window's client 
				// area, and enumerate the child windows. Pass the 
				// dimensions to the child windows during enumeration. 
	            GetClientRect(hDlg, &rcClient); 
		        EnumChildWindows(hDlg, EnumChildProc, (LPARAM) &rcClient); 
				break;
			}

		case WM_GETMINMAXINFO:	// limit size when shrinking window
			{
				LPMINMAXINFO info = (LPMINMAXINFO) lParam;
				if ( info->ptMinTrackSize.x < 210 )
					info->ptMinTrackSize.x = 210;
				if ( info->ptMinTrackSize.y < 200 )
					info->ptMinTrackSize.y = 200;
				break;
			}

		case WM_DESTROY:
            // Cleanup everything
			if ( fp ) { fclose(fp); fp = 0; }
			joyReleaseCapture(JOYSTICKID1);
            KillTimer( hDlg, 0 );    
            FreeDirectInput();    
            break;

		default:
			// We don't get notified for button events for buttons > 4,
			// so poll here instead.
			if ( g_pJoystick )
				CheckJoystickButton( hDlg );

			return FALSE; // Message not handled 
    }

	if ( g_pJoystick )
		CheckJoystickButton( hDlg );

	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: WaitOrTimerCallback
// Desc: Catch MM timer ticks
//-----------------------------------------------------------------------------
VOID CALLBACK WaitOrTimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	if (g_bWriting && !WriteToFile())
	{
		// Pass a clue back to the gui thread.
		g_bWriteError = true;
	}
}

//-----------------------------------------------------------------------------
// Name: OnJoystickButton
// Desc: Checks to see if button controlling writing to file is pressed, and deals with it.
//-----------------------------------------------------------------------------
void CheckJoystickButton( HWND hDlg )
{
	typedef enum { Down, Up = !Down } ButtonState;
	static ButtonState state = Up;

	DIJOYSTATE js;
	static time_t lastclick = 0;
	static time_t started = 0;
	time_t timenow = time(0);
	static HANDLE MMtimer;
	static UINT period;

	if ( PollJoystick( js ) == S_OK ) {
		
		// If the button we want just went from up to down....
		if ( state == Up &&	g_JoystickButton ) {

			state = Down;
			g_JoystickButton = false;

			// avoid a triple-click reopening the file by requiring 3 secs to pass
			if ( !g_bWriting && timenow - lastclick >= 3 ) {

				started = timenow;

				if ( !StartWriting() ) {
					char errbuf[512];
					char errstart[] = "Error creating output file `";
					strcpy(errbuf, errstart);
					strncat(errbuf, g_Config.FilePattern, sizeof errbuf/sizeof errbuf[0] - strlen(errbuf) - 16);
					strcat(errbuf, "000': ");
					size_t errstartlen = strlen(errbuf);
					strerror_s(&errbuf[errstartlen], sizeof errbuf - errstartlen, errno);
					MessageBox( NULL, errbuf, Title, MB_ICONERROR | MB_OK );
//					EndDialog( hDlg, TRUE ); 

				} else {

					g_bWriteError = false;	// all good so far.

					if ( !g_Config.ShowFilename )
						g_MsgText[0] = 0;

					// Disable buttons while writing.
				    EnableWindow( GetDlgItem( hDlg, ID_EDIT_CONFIG ), FALSE );
					ShowWindow( GetDlgItem( hDlg, ID_EDIT_CONFIG ), SW_HIDE );
				    EnableWindow( GetDlgItem( hDlg, IDCANCEL ), FALSE );
					ShowWindow( GetDlgItem( hDlg, IDCANCEL ), SW_HIDE );

					// Make a noise
					MessageBeep(MB_ICONASTERISK); 

					// Try set a 1ms timer resolution, so we get better timing intervals. Then request
					// a tick at the required interval. Note that the clock usually ticks at 64 Hz, so
					// there is a max 15.6ms (1000/64) error. Using SetTimer() results in an accumulating
					// error, but CreateTimerQueuetimer() does better, with the system adjusting calls here
					// to compensate, with zero accumulating error, which is more desirable.
					TIMECAPS tc;
					if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR ||
						(period=(min(max(tc.wPeriodMin, 1), tc.wPeriodMax))) < 1 ||
						 timeBeginPeriod(period) != TIMERR_NOERROR ||
						 CreateTimerQueueTimer(&MMtimer, NULL, WaitOrTimerCallback, (PVOID)69,
							0, (unsigned int)(1000.0 / g_Config.TicksPerSec), WT_EXECUTEDEFAULT) == FALSE) {
			                MessageBox( NULL, TEXT("Timer initialisation failed; cannot continue."),
		                    TEXT("The monitor will now exit."), MB_ICONERROR | MB_OK );
				        EndDialog( hDlg, 0 );
					}

					g_timerstart = GetTickCount();
				}

			} else if ( g_bWriting && timenow - started > 2 ) {
				if ( timenow - lastclick <= 1 ) {
					// two clicks in a second means we stop writing, but must write for a couple of secs.
					timeEndPeriod(period);
					DeleteTimerQueueTimer(NULL, MMtimer, NULL);
					StopWriting();
					MessageBeep(MB_OK);
					EnableWindow( GetDlgItem( hDlg, ID_EDIT_CONFIG ), TRUE );
					ShowWindow( GetDlgItem( hDlg, ID_EDIT_CONFIG ), SW_SHOW );
					EnableWindow( GetDlgItem( hDlg, IDCANCEL ), TRUE );
					ShowWindow( GetDlgItem( hDlg, IDCANCEL ), SW_SHOW );
					_snprintf( g_MsgText, sizeof g_MsgText, "Click button %u to start", g_Config.JoystickButton );
				}
			}

			lastclick = timenow;
		
		} else if ( state == Down && !g_JoystickButton ) {

			state = Up;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: ConfigAboutProc
// Desc: Handles the `About' dialog.
//-----------------------------------------------------------------------------
INT_PTR CALLBACK ConfigAboutProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg ) 
    {
        case WM_INITDIALOG:
		    EnableWindow( GetWindow( hDlg, GW_OWNER ), FALSE );
			SetWindowText( GetDlgItem( hDlg, IDC_VERSION ), g_Version );
			break;

		case WM_COMMAND:
            switch( LOWORD(wParam) )
			{
				case IDOK:
				    EnableWindow( GetWindow( hDlg, GW_OWNER ), TRUE );
                    EndDialog( hDlg, 0 );
					break;

				default:
					return FALSE;
			}
		default:
			return FALSE;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: ConfigDialogProc
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK ConfigDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg ) 
    {
		case WM_CLOSE:
			LoadConfig(); // kill any changes we made
			EnableWindow( GetWindow( hDlg, GW_OWNER ), TRUE );
            EndDialog( hDlg, 0 );
			break;

        case WM_INITDIALOG:
			{
			    RECT rcParent; 
				char buf[64];
				HWND hDlgParent = GetWindow( hDlg, GW_OWNER );
				
				EnableWindow( hDlgParent, FALSE );

				GetWindowRect(hDlgParent, &rcParent);
				if ( rcParent.left < 0 ) rcParent.left = 0;
				if ( rcParent.right < 0 ) rcParent.right = 0;
				sprintf(buf, "Position X %u,  Y %u;  Size %u x %u", rcParent.left, rcParent.top,
					rcParent.right - rcParent.left, rcParent.bottom - rcParent.top );
				SetWindowText( GetDlgItem( hDlg, IDC_WINDOW_POSN ), buf );

				SetWindowText( GetDlgItem( hDlg, IDC_FILENAME ), g_Config.FilePattern );

				if ( g_Config.RememberWindow ==  true ) 
					CheckDlgButton( hDlg, IDC_REMEMBER_WINDOW, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_REMEMBER_WINDOW, BST_UNCHECKED );
				if ( g_Config.ShowFilename ==  true ) 
					CheckDlgButton( hDlg, IDC_SHOW_NAME, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_SHOW_NAME, BST_UNCHECKED );
			    if ( g_Config.OutputFileBanner ==  true ) 
					CheckDlgButton( hDlg, IDC_WRITE_BANNER, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_WRITE_BANNER, BST_UNCHECKED );
			    if ( g_Config.OriginLowerLeft ==  true ) 
					CheckDlgButton( hDlg, IDC_ORIGIN_LOWERLEFT, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_ORIGIN_LOWERLEFT, BST_UNCHECKED );
			    if ( g_Config.DrawOctants ==  true ) 
					CheckDlgButton( hDlg, IDC_DRAW_OCTANTS, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_DRAW_OCTANTS, BST_UNCHECKED );
			    if ( g_Config.ShowAxes ==  true ) 
					CheckDlgButton( hDlg, IDC_SHOW_COORDS, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_SHOW_COORDS, BST_UNCHECKED );
			    if ( g_Config.SoundFeedback ==  true ) 
					CheckDlgButton( hDlg, IDC_SOUND_FEEDBACK, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_SOUND_FEEDBACK, BST_UNCHECKED );
			    if ( g_Config.SuppressX ==  false && g_Config.SuppressY == false ) 
					CheckDlgButton( hDlg, IDC_SUPPRESS_NONE, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_SUPPRESS_NONE, BST_UNCHECKED );
			    if ( g_Config.SuppressX ==  true ) 
					CheckDlgButton( hDlg, IDC_SUPPRESS_X, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_SUPPRESS_X, BST_UNCHECKED );
			    if ( g_Config.SuppressY ==  true ) 
					CheckDlgButton( hDlg, IDC_SUPPRESS_Y, BST_CHECKED );
						else CheckDlgButton( hDlg, IDC_SUPPRESS_Y, BST_UNCHECKED );

				sprintf(buf, "%0.1lf", g_Config.TicksPerSec );
					SetWindowText( GetDlgItem( hDlg, IDC_SAMPLES_PER_SEC ), buf );
				sprintf(buf, "%u", g_Config.JoystickButton );
					SetWindowText( GetDlgItem( hDlg, IDC_JOYSTICK_BUTTON ), buf );
				sprintf(buf, "%u", g_Config.Button2 );
					SetWindowText( GetDlgItem( hDlg, IDC_BUTTON2 ), buf );
				sprintf(buf, "%u", g_Config.XYMinMax );
					SetWindowText( GetDlgItem( hDlg, IDC_XYMINMAX ), buf );
				sprintf(buf, "%u", g_Config.GridCount );
					SetWindowText( GetDlgItem( hDlg, IDC_GRID_COUNT ), buf );
				sprintf(buf, "%u", g_Config.TickCount );
					SetWindowText( GetDlgItem( hDlg, IDC_TICK_COUNT ), buf );
				sprintf(buf, "%u", g_Config.EllipseSize );
					SetWindowText( GetDlgItem( hDlg, IDC_POINTER_SIZE ), buf );

				SetWindowText( GetDlgItem( hDlg, IDC_BANNER_COMMENT ), g_Config.BannerComment );
				SetWindowText( GetDlgItem( hDlg, IDC_LABEL_POSX ), g_Config.LabelPosX );
				SetWindowText( GetDlgItem( hDlg, IDC_LABEL_NEGX ), g_Config.LabelNegX );
				SetWindowText( GetDlgItem( hDlg, IDC_LABEL_POSY ), g_Config.LabelPosY );
				SetWindowText( GetDlgItem( hDlg, IDC_LABEL_NEGY ), g_Config.LabelNegY );
				SetWindowText( GetDlgItem( hDlg, IDC_LABEL_TOP_LEFT ), g_Config.LabelTopLeft );
				SetWindowText( GetDlgItem( hDlg, IDC_LABEL_TOP_RIGHT ), g_Config.LabelTopRight );
				SetWindowText( GetDlgItem( hDlg, IDC_LABEL_BOTTOM_RIGHT ), g_Config.LabelBottomRight );
				SetWindowText( GetDlgItem( hDlg, IDC_LABEL_BOTTOM_LEFT ), g_Config.LabelBottomLeft );
			}
	        break;

		case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
                case IDC_CONFIG_ABOUT:
					DialogBox( g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hDlg, ConfigAboutProc );
					break;

				case IDC_CONFIG_CANCEL:
					LoadConfig(); // kill any changes we made
				    EnableWindow( GetWindow( hDlg, GW_OWNER ), TRUE );
                    EndDialog( hDlg, 0 );
					break;

                case IDC_CONFIG_OK:
					{
						char buf[16];
						GetWindowText( GetDlgItem( hDlg, IDC_FILENAME ), g_Config.FilePattern, sizeof g_Config.FilePattern );

						if( IsDlgButtonChecked( hDlg, IDC_REMEMBER_WINDOW ) == BST_CHECKED )
							g_Config.RememberWindow = true; else g_Config.RememberWindow = false;
						if( IsDlgButtonChecked( hDlg, IDC_SHOW_NAME ) == BST_CHECKED )
							g_Config.ShowFilename = true; else g_Config.ShowFilename = false;
						if( IsDlgButtonChecked( hDlg, IDC_WRITE_BANNER ) == BST_CHECKED )
							g_Config.OutputFileBanner = true; else g_Config.OutputFileBanner = false;
						if( IsDlgButtonChecked( hDlg, IDC_ORIGIN_LOWERLEFT ) == BST_CHECKED )
							g_Config.OriginLowerLeft = true; else g_Config.OriginLowerLeft = false;
						if( IsDlgButtonChecked( hDlg, IDC_DRAW_OCTANTS ) == BST_CHECKED )
							g_Config.DrawOctants = true; else g_Config.DrawOctants = false;
						if( IsDlgButtonChecked( hDlg, IDC_SHOW_COORDS ) == BST_CHECKED )
							g_Config.ShowAxes = true; else g_Config.ShowAxes = false;
						if( IsDlgButtonChecked( hDlg, IDC_SOUND_FEEDBACK ) == BST_CHECKED )
							g_Config.SoundFeedback = true; else g_Config.SoundFeedback = false;
						if( IsDlgButtonChecked( hDlg, IDC_SUPPRESS_X ) == BST_CHECKED )
							g_Config.SuppressX = true; else g_Config.SuppressX = false;
						if( IsDlgButtonChecked( hDlg, IDC_SUPPRESS_Y ) == BST_CHECKED )
							g_Config.SuppressY = true; else g_Config.SuppressY = false;

						if (g_Config.DrawOctants && g_Config.OriginLowerLeft ) {
								MessageBox(hDlg, "Cannot draw octants with the origin in the lower left.", Title, MB_OK | MB_ICONEXCLAMATION);
								break;
						}

						if (g_Config.OriginLowerLeft && (g_Config.SuppressX || g_Config.SuppressY)) {
								MessageBox(hDlg, "Cannot suppress axes with the origin in the lower left.", Title, MB_OK | MB_ICONEXCLAMATION);
								break;
						}

						if (g_Config.DrawOctants && (g_Config.SuppressX || g_Config.SuppressY)) {
								MessageBox(hDlg, "Cannot suppress axes when drawing octants.", Title, MB_OK | MB_ICONEXCLAMATION);
								break;
						}

						GetWindowText( GetDlgItem( hDlg, IDC_SAMPLES_PER_SEC ), buf, sizeof buf );
						errno = 0;
						if ( atof(buf) <= 0 || errno != 0 ) {
								MessageBox(hDlg, "Ticks per second must be a floating point number greater than zero.", Title, MB_OK | MB_ICONEXCLAMATION);
								break;
						}

						g_Config.TicksPerSec = atof(buf);
						unsigned long millisecs = (unsigned long)(1000.0 / g_Config.TicksPerSec);
						if ( millisecs < USER_TIMER_MINIMUM ) {
							millisecs = 1000 / USER_TIMER_MINIMUM;
							char text[128];
							_snprintf(text, sizeof text, "Warning: Your clock cannot exceed %lu ticks per second.", millisecs);
							MessageBox(hDlg, text, Title, MB_OK | MB_ICONWARNING);
							g_Config.TicksPerSec = 1000.0 / USER_TIMER_MINIMUM;
						}

						DIDEVCAPS dc;
						DIJOYSTATE js;
						dc.dwSize = sizeof dc;
						if ( !g_pJoystick || g_pJoystick->GetCapabilities(&dc) != DI_OK )
							dc.dwButtons = sizeof js.rgbButtons / sizeof js.rgbButtons[0]; // fake it & use the max

						GetWindowText( GetDlgItem( hDlg, IDC_JOYSTICK_BUTTON ), buf, sizeof buf );
						if ( atoi(buf) < 1 || (unsigned)atoi(buf) > dc.dwButtons ) {
							char text[128];
							_snprintf(text, sizeof text, "Joystick button to start & stop writing must be between 1 and %lu inclusive", dc.dwButtons );
							MessageBox(hDlg, text, Title, MB_OK | MB_ICONEXCLAMATION);
							break;
						}
						g_Config.JoystickButton = atoi(buf);
						if ( !g_bWriting )
							_snprintf( g_MsgText, sizeof g_MsgText, "Click button %u to start", g_Config.JoystickButton );

						GetWindowText( GetDlgItem( hDlg, IDC_BUTTON2 ), buf, sizeof buf );
						if ( atoi(buf) < 0 || (unsigned)atoi(buf) > dc.dwButtons ) {
							char text[128];
							_snprintf(text, sizeof text, "Joystick button to monitor must be between 1 and %lu inclusive", dc.dwButtons );
							MessageBox(hDlg, text, Title, MB_OK | MB_ICONEXCLAMATION);
							break;
						}
						g_Config.Button2 = atoi(buf);

						if ( g_Config.JoystickButton == g_Config.Button2 ) {
							MessageBox(hDlg, "The button to start & stop writing cannot be the same as the button to monitor", Title, MB_OK | MB_ICONEXCLAMATION);
							break;
						}

						GetWindowText( GetDlgItem( hDlg, IDC_XYMINMAX ), buf, sizeof buf);
						if ( atoi(buf) <= 0 ) {
								MessageBox(hDlg, "Axis maximum value must be greater than zero.", Title, MB_OK | MB_ICONEXCLAMATION);
								break;
						}
						if ( atoi(buf) != g_Config.XYMinMax ) {
							g_Config.XYMinMax = atoi(buf);
							// Try re-init the joystick to pick up the new axes
							if (g_pJoystick != NULL)
								g_pJoystick->EnumObjects( EnumObjectsCallback, (VOID*)hDlg, DIDFT_AXIS );
						}

						GetWindowText( GetDlgItem( hDlg, IDC_GRID_COUNT ), buf, sizeof buf );
						if ( atoi(buf) < 0 ) {
								MessageBox(hDlg, "Grid count must be greater than or equal to zero.", Title, MB_OK | MB_ICONEXCLAMATION);
								break;
						}
						g_Config.GridCount = atoi(buf);

						GetWindowText( GetDlgItem( hDlg, IDC_TICK_COUNT ), buf, sizeof buf );
						if ( atoi(buf) < 0 ) {
								MessageBox(hDlg, "Tick count must be greater than or equal to zero.", Title, MB_OK | MB_ICONEXCLAMATION);
								break;
						}
						g_Config.TickCount = atoi(buf);

						GetWindowText( GetDlgItem( hDlg, IDC_POINTER_SIZE ), buf, sizeof buf );
						if ( atoi(buf) <= 0 || atoi(buf) > 50 ) {
								MessageBox(hDlg, "Pointer size (the radius) must be between 1 and 50 inclusive.", Title, MB_OK | MB_ICONEXCLAMATION);
								break;
						}
						g_Config.EllipseSize = atoi(buf);

						GetWindowText( GetDlgItem( hDlg, IDC_BANNER_COMMENT ), g_Config.BannerComment, sizeof g_Config.BannerComment );
						GetWindowText( GetDlgItem( hDlg, IDC_LABEL_POSX ), g_Config.LabelPosX, sizeof g_Config.LabelPosX );
						GetWindowText( GetDlgItem( hDlg, IDC_LABEL_NEGX ), g_Config.LabelNegX, sizeof g_Config.LabelNegX );
						GetWindowText( GetDlgItem( hDlg, IDC_LABEL_POSY ), g_Config.LabelPosY, sizeof g_Config.LabelPosY );
						GetWindowText( GetDlgItem( hDlg, IDC_LABEL_NEGY ), g_Config.LabelNegY, sizeof g_Config.LabelNegY );
						GetWindowText( GetDlgItem( hDlg, IDC_LABEL_TOP_LEFT ), g_Config.LabelTopLeft, sizeof g_Config.LabelTopLeft );
						GetWindowText( GetDlgItem( hDlg, IDC_LABEL_TOP_RIGHT ), g_Config.LabelTopRight, sizeof g_Config.LabelTopRight );
						GetWindowText( GetDlgItem( hDlg, IDC_LABEL_BOTTOM_RIGHT ), g_Config.LabelBottomRight, sizeof g_Config.LabelBottomRight );
						GetWindowText( GetDlgItem( hDlg, IDC_LABEL_BOTTOM_LEFT ), g_Config.LabelBottomLeft, sizeof g_Config.LabelBottomLeft );

						if ( g_Config.RememberWindow ) {
						    RECT rcParent; 
							HWND hDlgParent = GetWindow( hDlg, GW_OWNER );
							GetWindowRect(hDlgParent, &rcParent);
							g_Config.WPosnX = rcParent.left;
							g_Config.WPosnY = rcParent.top;
							g_Config.WSizeX = rcParent.right - rcParent.left;
							g_Config.WSizeY = rcParent.bottom - rcParent.top;
						}

						if ( !SaveConfig() ) {
							MessageBox( NULL, TEXT("Problems writing config to the registry. (You may need to run this program with Administrator rights.) ") \
			                    TEXT("Saved config may be incomplete."), Title, MB_ICONERROR | MB_OK );
						}

						// TODO - if config changes something on screen, it's not redrawing properly
						InvalidateRect( GetWindow( hDlg, GW_OWNER ), NULL, false );
						UpdateWindow( GetWindow( hDlg, GW_OWNER ) );
						EnableWindow( GetWindow( hDlg, GW_OWNER ), TRUE );
						EndDialog( hDlg, 0 );
						break;
					}

				default:
					return FALSE;
			}
			break;

		default:
		    return FALSE; // Message not handled 
    }
	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: EnumChildProc()
// Desc: Called for each child window when the main window is resized.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam) 
{ 
    int idChild = GetWindowLong(hwndChild, GWL_ID);
    LPRECT rcParent = (LPRECT)lParam;
	RECT rcChild;
	
	GetClientRect(hwndChild, &rcChild); 
 
    // Set the position of the child window based on its id.
	switch ( idChild ) {

		// Crosshair box
		case IDC_CROSSHAIR:
			MoveWindow( hwndChild,
			rcParent->left + 2,
			rcParent->top + 2,
			rcParent->right - 4,
			rcParent->bottom - 56,
			TRUE );
			break;

		// Config button
		case ID_EDIT_CONFIG:
			MoveWindow( hwndChild,
			rcParent->left + 5,
			rcParent->bottom - 50,
			rcChild.right - rcChild.left,
			rcChild.bottom - rcChild.top,
			!g_bWriting );
			break;

		// Cancel button
		case IDCANCEL:
			MoveWindow( hwndChild,
			rcParent->right - 95,
			rcParent->bottom - 50,
			rcChild.right - rcChild.left,
			rcChild.bottom - rcChild.top,
			!g_bWriting );
			break;

		// Message line
		case IDC_MSGS:
			MoveWindow( hwndChild,
			rcParent->left + 5,
			rcParent->bottom - 18,
			rcParent->right - 80,
			rcChild.bottom - rcChild.top,
			TRUE );
			break;
	}
 
	// Make sure the child window is visible. 
//    ShowWindow(hwndChild, SW_SHOW); 
 
    return TRUE; 
} 

//-----------------------------------------------------------------------------
// Name: StartWriting()
// Desc: Initialize for writing to the output file.
//-----------------------------------------------------------------------------
bool StartWriting( void )
{
	long lResult = 0;
	char buf[MAX_PATH];
	strncpy(buf, g_Config.FilePattern, sizeof buf);
	buf[sizeof buf -1] = 0;

	char * p = &buf[strlen(buf)];
	if ( p >= &buf[sizeof buf -5] ) p = &buf[sizeof buf -5];

	// Try append a three digit number as an extension.
	for ( int i = 0; i < 1000 ; i++ ) {
		sprintf(p, "%03i", i);
		if ( access(buf, 0) == -1 && errno == ENOENT ) {
			if ( (fp = fopen(buf, "w")) != NULL ) {
				if ( g_Config.ShowFilename )
					_snprintf(g_MsgText, sizeof g_MsgText, "Writing to %s", buf);
				if ( g_Config.OutputFileBanner ) {
					time_t now = time(NULL);
					struct tm * nowtm = localtime(&now);
					if ( fprintf(fp, "# File created at %s# Axes maximum value: %u\n# Ticks / second: %0.1lf\n# %s\n",
							asctime(nowtm), g_Config.XYMinMax, g_Config.TicksPerSec, g_Config.BannerComment ) <= 0 ) {
						_snprintf(g_MsgText, sizeof g_MsgText, "Error %u writing to output file %s",
								errno, buf);
					}
				}
				g_bWriting = true;
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Name: StopWriting()
// Desc: Close the output file.
//-----------------------------------------------------------------------------
void StopWriting( void )
{
	fclose(fp);
	g_MsgText[0] = 0;
	g_bWriting = false;
}

//-----------------------------------------------------------------------------
// Name: WriteToFile()
// Desc: Write data to the output file.
//-----------------------------------------------------------------------------
bool WriteToFile( void )
{
    DIJOYSTATE js;           // DInput joystick state 
    HRESULT hr;
	bool retcode;

    // Get the input's device state
    if( FAILED( hr = PollJoystick( js ) ) )
        return false;

	DWORD timernow = GetTickCount();
	float elapsed = (float)(timernow - g_timerstart) / 1000.0f;

	// Constrain the axes if we're not going negative
	if (g_Config.OriginLowerLeft == true) {
		if (js.lX < 0) js.lX = 0;
		if (js.lY > 0) js.lY = 0;	// will flip the sign below
	}

	// Report state of extra button if we're watching it.
	if ( g_Config.Button2 ) {
		retcode = (fprintf(fp, "%6.3f,%5ld,%5ld,%2i\n", elapsed, js.lX, -js.lY, g_Button2 ) > 0 );	// flip Y axis
		g_Button2 = false;
	} else {
		retcode = (fprintf(fp, "%6.3f,%5ld,%5ld\n", elapsed, js.lX, -js.lY ) > 0 );
	}

	return retcode;
}

//-----------------------------------------------------------------------------
// Name: InitDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
HRESULT InitDirectInput( HWND hDlg )
{
    HRESULT hr;

    // Register with the DirectInput subsystem and get a pointer
    // to a IDirectInput interface we can use.
    // Create a DInput object
    if( FAILED( hr = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION, 
                                         IID_IDirectInput8, (VOID**)&g_pDI, NULL ) ) )
        return hr;

    // Look for a simple joystick we can use for this program.
    if( FAILED( hr = g_pDI->EnumDevices( DI8DEVCLASS_GAMECTRL, 
                                         EnumJoysticksCallback,
                                         NULL, DIEDFL_ATTACHEDONLY ) ) )
        return hr;

    // Make sure we got a joystick
    if( NULL == g_pJoystick )
    {
        MessageBox( NULL, TEXT("Joystick not found.\nThings will go downhill from here...."),  
                    Title, MB_ICONERROR | MB_OK );
        return S_OK;
    }

    // Set the cooperative level to let DInput know how this device should
    // interact with the system and with other DInput applications.
    if( FAILED( hr = g_pJoystick->SetCooperativeLevel( hDlg, DISCL_NONEXCLUSIVE |
                                                             DISCL_BACKGROUND ) ) )
        return hr;

    // Set the data format to "simple joystick" - a predefined data format 
    //
    // A data format specifies which controls on a device we are interested in,
    // and how they should be reported. This tells DInput that we will be
    // passing a DIJOYSTATE structure to IDirectInputDevice::GetDeviceState().
    if( FAILED( hr = g_pJoystick->SetDataFormat( &c_dfDIJoystick ) ) )
        return hr;

    // Enumerate the joystick objects. The callback function enables user
    // interface elements for objects that are found, and sets the min/max
    // values property for discovered axes.
    if( FAILED( hr = g_pJoystick->EnumObjects( EnumObjectsCallback, 
                                                (VOID*)hDlg, DIDFT_AXIS ) ) )
        return hr;

	// Set a buffer for storing events
    DIPROPDWORD dipwd; 
    dipwd.diph.dwSize       = sizeof(DIPROPDWORD); 
    dipwd.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
    dipwd.diph.dwHow        = DIPH_DEVICE; 
    dipwd.diph.dwObj        = 0;
	dipwd.dwData			= 1024;			  // number of events
    if( FAILED( hr = g_pJoystick->SetProperty( DIPROP_BUFFERSIZE, &dipwd.diph ) ) ) 
	    return hr;

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: EnumJoysticksCallback()
// Desc: Called once for each enumerated joystick. If we find one, create a
//       device interface on it so we can play with it.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance,
                                     VOID* pContext )
{
    HRESULT hr;

    // Obtain an interface to the enumerated joystick.
    hr = g_pDI->CreateDevice( pdidInstance->guidInstance, &g_pJoystick, NULL );

    // If it failed, then we can't use this joystick. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    if( FAILED(hr) ) 
        return DIENUM_CONTINUE;

    // Stop enumeration. Note: we're just taking the first joystick we get. You
    // could store all the enumerated joysticks and let the user pick.
    return DIENUM_STOP;
}

//-----------------------------------------------------------------------------
// Name: EnumObjectsCallback()
// Desc: Callback function for enumerating objects (axes, buttons, POVs) on a 
//       joystick. This function enables user interface elements for objects
//       that are found to exist, and scales axes min/max values.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumObjectsCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,
                                   VOID* pContext )
{
    HWND hDlg = (HWND)pContext;

    // For axes that are returned, set the DIPROP_RANGE property for the
    // enumerated axis in order to scale min/max values.
    if( pdidoi->dwType & DIDFT_AXIS )
    {
        DIPROPRANGE diprg; 
        diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
        diprg.diph.dwHow        = DIPH_BYID; 
        diprg.diph.dwObj        = pdidoi->dwType; // Specify the enumerated axis
		diprg.lMin				= -g_Config.XYMinMax;
		diprg.lMax              = +g_Config.XYMinMax; 

        // Set the range for the axis
        if( FAILED( g_pJoystick->SetProperty( DIPROP_RANGE, &diprg.diph ) ) ) 
            return DIENUM_STOP;
    }

    return DIENUM_CONTINUE;
}


//-----------------------------------------------------------------------------
// Name: PollJoystick()
// Desc: Return the current state of the joystick.
// Also sets the two global booleans to report if a button was pressed since
// the last check.
//-----------------------------------------------------------------------------
HRESULT PollJoystick( DIJOYSTATE& js )
{
    HRESULT     hr;

	if ( ! g_pJoystick )
		return -1;

	// Poll the device to read the current state. Not always necessary, in which
	// case it returns an error. Just ignore that.
    g_pJoystick->Poll(); 

    // Get the input's device state
    hr = g_pJoystick->GetDeviceState( sizeof js, &js );

	if( hr != DI_OK )
    {
        // DInput is telling us that the input stream has been
        // interrupted. We aren't tracking any state between polls, so
        // we don't have any special reset that needs to be done. We
        // just re-acquire and try again.
		hr = g_pJoystick->Acquire();
        while( hr == DIERR_INPUTLOST ) 
            hr = g_pJoystick->Acquire();

		// Try one more time
	    if( FAILED( hr = g_pJoystick->GetDeviceState( sizeof js, &js ) ) )
	        return hr;
    }

	// If we're suppressing motion, just clear the coords
	if ( g_Config.SuppressX == true ) {
		js.lX = 0;
	} else if ( g_Config.SuppressY == true ) {
		js.lY = 0;
	}

	// Loop through all events, looking just for button presses since the last check
	DIDEVICEOBJECTDATA rgdod;
	DWORD dwInOut = 1;
	while ( g_pJoystick->GetDeviceData( sizeof rgdod, &rgdod, &dwInOut, 0 ) == DI_OK &&	dwInOut == 1 ) {
		if ( rgdod.dwOfs == DIJOFS_BUTTON(g_Config.JoystickButton -1) && (rgdod.dwData & 0x80) )
			g_JoystickButton = true;
		else if (g_Config.Button2)
			if ( rgdod.dwOfs == DIJOFS_BUTTON(g_Config.Button2 -1) && (rgdod.dwData & 0x80) ) {
				g_Button2 = true;	// button was pressed
				if (g_Config.SoundFeedback)
					MessageBeep(-1);
			}
	}

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: UpdateInputState()
// Desc: Get the input device's state and display it.
//-----------------------------------------------------------------------------
HRESULT UpdateInputState( HWND hDlg )
{
    TCHAR       strText[512]; // Device state text
    DIJOYSTATE	js;           // DInput joystick state 
    HDC         hDC;
    INT         x, y, radius, xsize, ysize;
	RECT		rctWinSize;

	static const float deg2rad = 0.0174532925f;

	// Get the input's device state
    if( PollJoystick( js ) != S_OK )
		memset( &js, 0, sizeof js);	// it may be unplugged.

	// Display joystick state to dialog
	HWND hXhair = GetDlgItem( hDlg, IDC_CROSSHAIR );

	if ( !GetWindowRect( hXhair, &rctWinSize ) )
		return -1;

    // Calculate center of feedback window for center marker
	xsize = rctWinSize.right - rctWinSize.left;
    x = xsize / 2;
    ysize = rctWinSize.bottom - rctWinSize.top;
	y = ysize / 2;

	// Size used for drawing pointer circle
	radius = min(xsize,ysize) * g_Config.EllipseSize / 100;
	static INT	oldx = radius+1, oldy = radius+1;

	hDC = GetDC( hXhair );
    if( NULL == hDC ) 
        return -1;

	int BorderX, BorderY;	// leave some space around the edges
	if (g_Config.OriginLowerLeft) {
		BorderX = BorderY = 25;			// need more room for labels
	} else {
		TEXTMETRIC tm;
		if (GetTextMetrics(hDC, &tm) == 0)
			tm.tmHeight = 13;			// wild guess
		BorderY = tm.tmHeight;			// Just enough room to write labels outside the border.
		BorderX = tm.tmHeight;			// X border is the same size as the Y border.
		//BorderX = 4;					// Fixed width narrow border.
	}

	// Use system font for all output text.
	SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT)) ;

    // Show coordinates of the pointer (badly named showaxes...)
	if ( g_Config.ShowAxes ) {
		HDC hDCx = GetDC( GetDlgItem( hDlg, IDC_X_AXIS ) );
		SetTextColor( hDCx, RGB(0x00,0xf0,0x00) );
		SetBkColor( hDCx, RGB(0xff,0xff,0xff) );
		SelectObject(hDCx, GetStockObject(DEFAULT_GUI_FONT)) ;
		sprintf( strText, "X: %5ld ", js.lX ); 
		TextOut( hDCx, 1,1, strText, strlen(strText) );
	    ReleaseDC( GetDlgItem( hDlg, IDC_X_AXIS ), hDCx );

		HDC hDCy = GetDC( GetDlgItem( hDlg, IDC_Y_AXIS ) );
		SetTextColor( hDCy, RGB(0x00,0xf0,0x00) );
		SetBkColor( hDCy, RGB(0xff,0xff,0xff) );
		SelectObject(hDCy, GetStockObject(DEFAULT_GUI_FONT)) ;
		sprintf( strText, "Y: %5ld ", -js.lY );		// flip Y axis
		TextOut( hDCy, 1,1, strText, strlen(strText) );
	    ReleaseDC( GetDlgItem( hDlg, IDC_Y_AXIS ), hDCy );

		ShowWindow( GetDlgItem( hDlg, IDC_X_AXIS ), SW_SHOW );
	    ShowWindow( GetDlgItem( hDlg, IDC_Y_AXIS ), SW_SHOW );
	} else {
		ShowWindow( GetDlgItem( hDlg, IDC_X_AXIS ), SW_HIDE );
	    ShowWindow( GetDlgItem( hDlg, IDC_Y_AXIS ), SW_HIDE );
	}

	// Labels
	unsigned int oldalign = GetTextAlign( hDC );
	SetTextColor( hDC, RGB(0x00,0x00,0x00) );
	SetBkColor( hDC, RGB(0xff,0xff,0xff) );

	if ( g_Config.LabelNegX[0] != 0 ) {
		if (!g_Config.OriginLowerLeft) {
			SetTextAlign( hDC, TA_TOP | TA_LEFT );
			TextOut( hDC, BorderX+2,y+2, g_Config.LabelNegX, strlen(g_Config.LabelNegX) );

		} else {	// Draw the label vertically, outside the axis, rotated 90 degrees
			HFONT NegYfont = CreateFont(
			   0,                         // nHeight
			   0,                         // nWidth
			   900,                       // nEscapement
			   900,                       // nOrientation
			   FW_NORMAL,                 // nWeight
			   FALSE,                     // bItalic
			   FALSE,                     // bUnderline
			   0,                         // cStrikeOut
			   DEFAULT_CHARSET,           // nCharSet
			   OUT_DEFAULT_PRECIS,        // nOutPrecision
			   CLIP_DEFAULT_PRECIS,       // nClipPrecision
			   DEFAULT_QUALITY,           // nQuality
			   DEFAULT_PITCH | FF_DONTCARE,// nPitchAndFamily
			   NULL);	                  // lpszFacename 

			if (NegYfont != NULL) {
				HGDIOBJ oldfont = SelectObject(hDC, NegYfont);
				TEXTMETRIC tm;
				if (GetTextMetrics(hDC, &tm) == 0)
					tm.tmHeight = 13;	// wild guess
				SetTextAlign( hDC, TA_BOTTOM | TA_CENTER );
				TextOut(hDC, BorderY / 2 + tm.tmHeight/2, y, g_Config.LabelNegX, strlen(g_Config.LabelNegX));

				// Done with the font.  Delete the font object.
				if (oldfont != NULL) SelectObject(hDC, oldfont);
				DeleteObject(NegYfont); 

			} else {

				/* Last resort; this draws text vertically, but individual chars are not rotated. */
				TEXTMETRIC tm;
				if (GetTextMetrics(hDC, &tm) == 0)
					tm.tmHeight = 13;	// wild guess
				int xpos = BorderX / 2;
				int ypos = y - (strlen(g_Config.LabelNegX) * tm.tmHeight) /2;
				SetTextAlign( hDC, TA_TOP | TA_CENTER );
				for (unsigned int i=0; i < strlen(g_Config.LabelNegX); i++)
					TextOut(hDC, xpos, ypos + tm.tmHeight*i, &g_Config.LabelNegX[i], 1);
			}
		}
	}
	if ( g_Config.LabelPosX[0] != 0 ) {
		SetTextAlign( hDC, TA_TOP | TA_RIGHT );
		TextOut( hDC, xsize - BorderX - 2,y+2, g_Config.LabelPosX, strlen(g_Config.LabelPosX) );
	}
	if ( g_Config.LabelNegY[0] != 0 ) {	// axis is flipped
		SetTextAlign( hDC, TA_BOTTOM | TA_CENTER );
		if (!g_Config.OriginLowerLeft) {	// label goes inside the axis, else below
			TextOut( hDC, x+2, ysize - 2, g_Config.LabelNegY, strlen(g_Config.LabelNegY) );
		} else {
			// Force the same font we used for the negative Y axis, else they're different...
			HFONT NegYfont = CreateFont(
			   0,                         // nHeight
			   0,                         // nWidth
			   0,                         // nEscapement
			   0,                         // nOrientation
			   FW_NORMAL,                 // nWeight
			   FALSE,                     // bItalic
			   FALSE,                     // bUnderline
			   0,                         // cStrikeOut
			   DEFAULT_CHARSET,           // nCharSet
			   OUT_DEFAULT_PRECIS,        // nOutPrecision
			   CLIP_DEFAULT_PRECIS,       // nClipPrecision
			   DEFAULT_QUALITY,           // nQuality
			   DEFAULT_PITCH | FF_DONTCARE,// nPitchAndFamily
			   NULL);	                  // lpszFacename 

			HGDIOBJ oldfont = NULL;
			if (NegYfont != NULL)
				oldfont = SelectObject(hDC, NegYfont);
			TEXTMETRIC tm;
			if (GetTextMetrics(hDC, &tm) == 0)
				tm.tmHeight = 13;	// wild guess
			SetTextAlign( hDC, TA_BOTTOM | TA_CENTER );
			TextOut(hDC, x, ysize - (BorderY-tm.tmHeight)/2, g_Config.LabelNegY, strlen(g_Config.LabelNegY));

			// Done with the font.  Delete the font object.
			if (oldfont != NULL) SelectObject(hDC, oldfont);
			if (NegYfont != NULL) DeleteObject(NegYfont); 
		}
	}
	if ( g_Config.LabelPosY[0] != 0 ) {
		SetTextAlign( hDC, TA_TOP | TA_CENTER );
		TextOut( hDC, x-2, 2, g_Config.LabelPosY, strlen(g_Config.LabelPosY) );
	}

	// Need to measure string output sizes to avoid truncation at the edge of the screen
	// TODO messy, repetitive
	if ( g_Config.LabelTopLeft[0] != 0 ) {
		RECT r = { 0, 0, 0, 0 };
		DrawText(hDC, g_Config.LabelTopLeft, strlen(g_Config.LabelTopLeft), &r, DT_CALCRECT);
		int offset = r.right - x/2 + BorderX;
		if (offset<0) offset = 0;
		SetTextAlign( hDC, TA_BOTTOM | TA_RIGHT );
		TextOut( hDC, x/2 + offset,y/2, g_Config.LabelTopLeft, strlen(g_Config.LabelTopLeft) );
	}
	if ( g_Config.LabelTopRight[0] != 0 ) {
		RECT r = { 0, 0, 0, 0 };
		DrawText(hDC, g_Config.LabelTopRight, strlen(g_Config.LabelTopRight), &r, DT_CALCRECT);
		int offset = r.right - x/2 + BorderX;
		if (offset<0) offset = 0;
		SetTextAlign( hDC, TA_BOTTOM | TA_LEFT );
		TextOut( hDC, x+x-x/2 - offset,y/2, g_Config.LabelTopRight, strlen(g_Config.LabelTopRight) );
	}
	if ( g_Config.LabelBottomRight[0] != 0 ) {
		RECT r = { 0, 0, 0, 0 };
		DrawText(hDC, g_Config.LabelBottomRight, strlen(g_Config.LabelBottomRight), &r, DT_CALCRECT);
		int offset = r.right - x/2 + BorderX;
		if (offset<0) offset = 0;
		SetTextAlign( hDC, TA_TOP | TA_LEFT );
		TextOut( hDC, x+x-x/2 - offset,y+y-y/2, g_Config.LabelBottomRight, strlen(g_Config.LabelBottomRight) );
	}
	if ( g_Config.LabelBottomLeft[0] != 0 ) {
		RECT r = { 0, 0, 0, 0 };
		DrawText(hDC, g_Config.LabelBottomLeft, strlen(g_Config.LabelBottomLeft), &r, DT_CALCRECT);
		int offset = r.right - x/2 + BorderX;
		if (offset<0) offset = 0;
		SetTextAlign( hDC, TA_TOP | TA_RIGHT );
		TextOut( hDC, x/2 + offset,y+y-y/2, g_Config.LabelBottomLeft, strlen(g_Config.LabelBottomLeft) );
	}
	SetTextAlign( hDC, oldalign );

	// Display any msgs
	SetWindowText( GetDlgItem( hDlg, IDC_MSGS ), g_MsgText );

	// Erase old ellipse
	SelectPen( hDC, GetStockPen(WHITE_PEN) );
	SelectBrush( hDC, GetStockBrush(WHITE_BRUSH) );
	Ellipse( hDC, oldx-radius, oldy-radius, oldx+radius, oldy+radius );
	
	// Draw a grid
	if (g_Config.GridCount > 0) {
	    SelectPen(hDC, CreatePen(PS_DOT, 0, RGB(0xA0,0xA0,0xA0)));
		//printf("grid: xsize, ysize = %i,%i, x,y = %i,%i\n", xsize, ysize, x,y);
		for (int i = 0; i <= g_Config.GridCount; i++) {
			//int xpos = Border + stepx*i, ypos = Border + stepy*i;
			int xpos = BorderX + (xsize - 2*BorderX) * i / g_Config.GridCount;
			int ypos = BorderY + (ysize - 2*BorderY) * i / g_Config.GridCount;
			if (abs(xpos-x) == 1)
				xpos = x;	// avoid rounding error putting this right next to the axis
			if (abs(ypos-y) == 1)
				ypos = y;
			MoveToEx( hDC, BorderX, ypos, NULL );
			LineTo(   hDC, xsize - BorderX, ypos);
			//printf("grid: %4i,%4i -> %4i,%4i    ", Border, ypos, xsize - Border, ypos);
			MoveToEx( hDC, xpos, BorderY, NULL );
			LineTo(   hDC, xpos, ysize - BorderY);
			//printf("grid: %4i,%4i -> %4i,%4i\n", xpos, Border, xpos, ysize - Border);
		}
	}

	// Draw tickmarks on axes
	if (g_Config.TickCount > 0 && !g_Config.DrawOctants) {
	    SelectPen( hDC, GetStockPen(DC_PEN) );
		SetDCPenColor( hDC, RGB(0x0f,0x0f,0xff) );
		static const int TickSize = 10;	// pixels
		for (int i = 0; i <= g_Config.TickCount; i++) {
			int xpos = BorderX + (xsize - 2*BorderX) * i / g_Config.TickCount;
			int ypos = BorderY + (ysize - 2*BorderY) * i / g_Config.TickCount;
			if (abs(xpos-x) == 1)
				xpos = x;	// avoid rounding error putting this right next to the axis
			if (abs(ypos-y) == 1)
				ypos = y;
			if (g_Config.OriginLowerLeft) {
				MoveToEx( hDC, BorderX, ypos, NULL );
				LineTo(   hDC, BorderX + TickSize, ypos);
				MoveToEx( hDC, xpos, ysize - BorderY, NULL );
				LineTo(   hDC, xpos, ysize - BorderY - TickSize);
			} else {
				if (!g_Config.SuppressY) {
					MoveToEx( hDC, x - TickSize/2, ypos, NULL );
					LineTo(   hDC, x + TickSize/2, ypos);
				}
				if (!g_Config.SuppressX) {
					MoveToEx( hDC, xpos, y - TickSize/2, NULL );
					LineTo(   hDC, xpos, y + TickSize/2);
				}
			}
		}
	}

    // Draw axes
    SelectPen( hDC, GetStockPen(DC_PEN) );
	SetDCPenColor( hDC, RGB(0x0f,0x0f,0xff) );
	if (g_Config.DrawOctants) {
		// Will use a big hypoteneuse to make sure lines are long enough,
		// then clip the output to the edges of our drawing area.
		HRGN newRegn = CreateRectRgn(BorderX, BorderY, xsize-BorderX, ysize-BorderY),
			oldRegn = CreateRectRgn(0,0,0,0);
		int wasClipped = GetClipRgn(hDC, oldRegn);
		SelectClipRgn(hDC, newRegn);

		for (int i = 0; i < 8; i++) {
			float rads = (22.5f + (float)(45*i)) * deg2rad;
			int x2 = (int)((float)xsize * cos(rads));
			int y2 = (int)((float)ysize * sin(rads));
			MoveToEx( hDC, x, y, NULL );
			LineTo(   hDC, x+x2, y+y2 );
		}

		// Restore the clipping region to the old value
		if (wasClipped) SelectClipRgn(hDC, oldRegn);
		else SelectClipRgn(hDC, NULL);
		if (oldRegn != NULL) DeleteObject(oldRegn);
		if (newRegn != NULL) DeleteObject(newRegn);

	} else if (g_Config.OriginLowerLeft) {
		MoveToEx( hDC, BorderX, BorderY, NULL );
		LineTo(   hDC, BorderX, ysize - BorderY );
		//MoveToEx( hDC, BorderX, ysize - BorderY, NULL );
		LineTo(   hDC, xsize - BorderX, ysize - BorderY );

	} else {	// Standard X,Y axes, centred on screen
		if (!g_Config.SuppressY) {
			MoveToEx( hDC, x, BorderY, NULL );
			LineTo(   hDC, x, ysize - BorderY );
		} else {
			MoveToEx( hDC, x, y-radius-3, NULL );				// make a teeny bit bigger than the 
			LineTo(   hDC, x, y+radius+3);						// ellipse cursor.
		}
		if (!g_Config.SuppressX) {
			MoveToEx( hDC, BorderX, y, NULL );
			LineTo(   hDC, xsize - BorderX, y );
		} else {
			MoveToEx( hDC, x-radius-2, y, NULL );
			LineTo(   hDC, x+radius+2, y);
		}
	}

	// Draw mark, making sure to adjust so we don't erase window edges
	if (!g_Config.OriginLowerLeft) {
		x += MulDiv( xsize - 2*BorderX, js.lX, 2 * g_Config.XYMinMax );
		y += MulDiv( ysize - 2*BorderY, js.lY, 2 * g_Config.XYMinMax );
	} else {
		x = MulDiv( xsize - BorderX, js.lX, g_Config.XYMinMax );
		y = MulDiv( ysize - BorderY, js.lY, g_Config.XYMinMax ) + ysize;
	}
	if (x >= xsize - BorderX) x = xsize - BorderX;
	if (x < BorderX) x = BorderX;
	if (y > ysize - BorderY ) y = ysize - BorderY;
	if (y < BorderY) y = BorderY;
	
	// Save so we can erase on the next pass
	oldx = x, oldy = y;

    SelectBrush( hDC, GetStockBrush(DC_BRUSH) );
	SetDCBrushColor( hDC, g_bWriting ? RGB(0xff,0,0) : RGB(0xf0,0xf0,0xf0) );
    Ellipse( hDC, x-radius, y-radius, x+radius, y+radius );

    ReleaseDC( hXhair, hDC );

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
VOID FreeDirectInput()
{
    // Unacquire the device one last time just in case 
    // the app tried to exit while the device is still acquired.
    if( g_pJoystick ) 
        g_pJoystick->Unacquire();
    
    // Release any DirectInput objects.
    SAFE_RELEASE( g_pJoystick );
    SAFE_RELEASE( g_pDI );
}
