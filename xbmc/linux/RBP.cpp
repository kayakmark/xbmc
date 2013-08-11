/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "RBP.h"
#if defined(TARGET_RASPBERRY_PI)

#include "utils/log.h"
#include "windowing/WindowingFactory.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/GraphicContext.h"
#include "xbmc/settings/DisplaySettings.h"
#include "Application.h"

CRBP::CRBP()
{
  m_initialized     = false;
  m_omx_initialized = false;
  m_DllBcmHost      = new DllBcmHost();
  m_OMX             = new COMXCore();
}

CRBP::~CRBP()
{
  Deinitialize();
  delete m_OMX;
  delete m_DllBcmHost;
}

bool CRBP::Initialize()
{
  m_initialized = m_DllBcmHost->Load();
  if(!m_initialized)
    return false;

  m_DllBcmHost->bcm_host_init();

  m_omx_initialized = m_OMX->Initialize();
  if(!m_omx_initialized)
    return false;

  char response[80] = "";
  m_arm_mem = 0;
  m_gpu_mem = 0;
  if (vc_gencmd(response, sizeof response, "get_mem arm") == 0)
    vc_gencmd_number_property(response, "arm", &m_arm_mem);
  if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
    vc_gencmd_number_property(response, "gpu", &m_gpu_mem);

  return true;
}

void CRBP::LogFirmwareVerison()
{
  char  response[160];
  m_DllBcmHost->vc_gencmd(response, sizeof response, "version");
  response[sizeof(response) - 1] = '\0';
  CLog::Log(LOGNOTICE, "Raspberry PI firmware version: %s", response);
  CLog::Log(LOGNOTICE, "ARM mem: %dMB GPU mem: %dMB", m_arm_mem, m_gpu_mem);
}

void CRBP::GetDisplaySize(int &width, int &height)
{
  DISPMANX_DISPLAY_HANDLE_T display;
  DISPMANX_MODEINFO_T info;

  display = vc_dispmanx_display_open( 0 /*screen*/ );
  if (vc_dispmanx_display_get_info(display, &info) == 0)
  {
    width = info.width;
    height = info.height;
  }
  else
  {
    width = 0;
    height = 0;
  }
  vc_dispmanx_display_close(display );
}

unsigned char *CRBP::CaptureDisplay(int width, int height, int *pstride, bool swap_red_blue)
{
  DISPMANX_DISPLAY_HANDLE_T display;
  DISPMANX_RESOURCE_HANDLE_T resource;
  VC_RECT_T rect;
  unsigned char *image = NULL;
  uint32_t vc_image_ptr;
  int stride;

  display = vc_dispmanx_display_open( 0 /*screen*/ );
  stride = ((width + 15) & ~15) * 4;
  image = new unsigned char [height * stride];

  if (image)
  {
    resource = vc_dispmanx_resource_create( VC_IMAGE_RGBA32, width, height, &vc_image_ptr );

    vc_dispmanx_snapshot(display, resource, (DISPMANX_TRANSFORM_T)0);

    vc_dispmanx_rect_set(&rect, 0, 0, width, height);
    vc_dispmanx_resource_read_data(resource, &rect, image, stride);
    vc_dispmanx_resource_delete( resource );
    vc_dispmanx_display_close(display );

    // we need to save in BGRA order so Swap RGBA -> BGRA
    if (swap_red_blue)
    {
      for (int y = 0; y < height; y++)
      {
        unsigned char *p = image + y * stride;
        for (int x = 0; x < width; x++, p+=4)
        {
          unsigned char t = p[0];
          p[0] = p[2];
          p[2] = t;
        }
      }
    }
    // assume we need to pack image if caller doesn't want stride
    if (!pstride && stride > width*4)
    {
      for (int y = 0; y < height; y++)
      {
        unsigned char *in  = image + y * stride;
        unsigned char *out = image + y * width * 4;
        memmove(out, in, width*4);
      }
    }
  }
  if (pstride)
    *pstride = stride;
  return image;
}

void CRBP::Deinitialize()
{
  if(m_omx_initialized)
    m_OMX->Deinitialize();

  m_DllBcmHost->bcm_host_deinit();

  if(m_initialized)
    m_DllBcmHost->Unload();

  m_initialized     = false;
  m_omx_initialized = false;
}

void CRBP::SuspendVideoOutput()
{
  CLog::Log(LOGDEBUG, "Raspberry PI suspending video output\n");
  g_Windowing.DestroyWindow();
}

void CRBP::ResumeVideoOutput()
{
  CLog::Log(LOGDEBUG, "Raspberry PI resuming video output\n");
  RESOLUTION_INFO res = CDisplaySettings::Get().GetCurrentResolution();
  g_Windowing.CreateNewWindow("XBMC", true, res, CApplication::OnEvent);
  g_windowManager.MarkDirty();
}

#endif
