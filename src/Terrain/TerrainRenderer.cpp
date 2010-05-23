/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Terrain/TerrainRenderer.hpp"
#include "Terrain/RasterTerrain.hpp"
#include "Terrain/RasterWeather.hpp"
#include "Terrain/RasterMap.hpp"
#include "Screen/STScreenBuffer.h"
#include "Dialogs.h"
#include "Math/FastMath.h"
#include "Math/Earth.hpp"
#include "Screen/Ramp.hpp"
#include "Screen/Graphics.hpp"
#include "Screen/Layout.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Projection.hpp"

#include <assert.h>
#include <stdio.h>

#define NUM_COLOR_RAMP_LEVELS 13

const COLORRAMP weather_colors[6][NUM_COLOR_RAMP_LEVELS] = {
  { // Blue to red       // vertical speed
    {   0,       0,     0,     255}, // -200
    { 100,       0,     195,   255}, // -100
    { 200,     52,      192,    11}, // 0
    { 250,     182,     233,     4}, // 40
    { 300,     255,     233,     0}, // 80
    { 360,     255,     209,     0}, // 120
    { 420,     255,     155,     0}, // 160
    { 480,     255,     109,     0}, // 200
    { 540,     255,     35,      0}, // 240
    { 600,     255,     00,      0}, // 300
    {1000,         0xFF, 0x00, 0x00},
    {8000,         0xFF, 0x00, 0x00},
    {9000,         0xFF, 0x00, 0x00}
  },
  {
    {0,            0xFF, 0xFF, 0xFF},
    {250,          0x80, 0x80, 0xFF},
    {500,          0x80, 0xFF, 0xFF},
    {750,          0xFF, 0xFF, 0x80},
    {1000,         0xFF, 0x80, 0x80},
    {1250,         0xFF, 0x80, 0x80},
    {2000,         0xFF, 0xA0, 0xA0},
    {3000,         0xFF, 0xA0, 0xA0},
    {4000,         0xFF, 0x00, 0x00},
    {5000,         0xFF, 0x00, 0x00},
    {6000,         0xFF, 0x00, 0x00},
    {7000,         0xFF, 0x00, 0x00},
    {8000,         0xFF, 0x00, 0x00}
  },
  {
    {0,            0xFF, 0xFF, 0xFF},
    {750,          0x80, 0x80, 0xFF},
    {1500,          0x80, 0xFF, 0xFF},
    {2250,          0xFF, 0xFF, 0x80},
    {3000,          0xFF, 0x80, 0x80},
    {3500,         0xFF, 0x80, 0x80},
    {6000,         0xFF, 0xA0, 0xA0},
    {8000,         0xFF, 0xA0, 0xA0},
    {9000,         0xFF, 0x00, 0x00},
    {9500,         0xFF, 0x00, 0x00},
    {9600,         0xFF, 0x00, 0x00},
    {9700,         0xFF, 0x00, 0x00},
    {20000,         0xFF, 0x00, 0x00}
  },
  { // Blue to Gray, 8 steps
    {   0,       0,     153,     204},
    {  12,     102,     229,     255},
    {  25,     153,     255,     255},
    {  37,     204,     255,     255},
    {  50,     229,     229,     229},
    {  62,     173,     173,     173},
    {  75,     122,     122,     122},
    { 100,      81,      81,      81},
    {5000,      71,      71,      71},
    {6000,         0xFF, 0x00, 0x00},
    {7000,         0xFF, 0x00, 0x00},
    {8000,         0xFF, 0x00, 0x00},
    {9000,         0xFF, 0x00, 0x00}
  },
  { // sfctemp, blue to orange to red
    {   0,       7,      90,     255},
    {  30,      50,     118,     255},
    {  70,      89,     144,     255},
    {  73,     140,     178,     255},
    {  76,     191,     212,     255},
    {  79,     229,     238,     255},
    {  82,     247,     249,     255},
    {  85,     255,     255,     204},
    {  88,     255,     255,     153},
    {  91,     255,     255,       0},
    {  95,     255,     204,       0},
    { 100,     255,     153,       0},
    { 120,     255,       0,       0}
  },
  { // Blue to white to red       // vertical speed (convergence)
    {   0,       7,      90,     255},
    { 100,      50,     118,     255},
    { 140,      89,     144,     255},
    { 160,     140,     178,     255},
    { 180,     191,     212,     255},
    { 190,     229,     238,     255},
    { 200,     247,     249,     255},
    { 210,     255,     255,     204},
    { 220,     255,     255,     153},
    { 240,     255,     255,       0},
    { 260,     255,     204,       0},
    { 300,     255,     153,       0},
    {1000,     255,     102,       0},
  },
};


const COLORRAMP terrain_colors[8][NUM_COLOR_RAMP_LEVELS] = {
  {
    {0,           0x70, 0xc0, 0xa7},
    {250,         0xca, 0xe7, 0xb9},
    {500,         0xf4, 0xea, 0xaf},
    {750,         0xdc, 0xb2, 0x82},
    {1000,        0xca, 0x8e, 0x72},
    {1250,        0xde, 0xc8, 0xbd},
    {1500,        0xe3, 0xe4, 0xe9},
    {1750,        0xdb, 0xd9, 0xef},
    {2000,        0xce, 0xcd, 0xf5},
    {2250,        0xc2, 0xc1, 0xfa},
    {2500,        0xb7, 0xb9, 0xff},
    {5000,        0xb7, 0xb9, 0xff},
    {6000,        0xb7, 0xb9, 0xff}
  },
  {
    {0,           0x70, 0xc0, 0xa7},
    {500,         0xca, 0xe7, 0xb9},
    {1000,        0xf4, 0xea, 0xaf},
    {1500,        0xdc, 0xb2, 0x82},
    {2000,        0xca, 0x8e, 0x72},
    {2500,        0xde, 0xc8, 0xbd},
    {3000,        0xe3, 0xe4, 0xe9},
    {3500,        0xdb, 0xd9, 0xef},
    {4000,        0xce, 0xcd, 0xf5},
    {4500,        0xc2, 0xc1, 0xfa},
    {5000,        0xb7, 0xb9, 0xff},
    {6000,        0xb7, 0xb9, 0xff},
    {7000,        0xb7, 0xb9, 0xff}
  },
  { // Imhof Type 7, geomteric 1.35 9
    {0,    153, 178, 169},
    {368,  180, 205, 181},
    {496,  225, 233, 192},
    {670,  255, 249, 196},
    {905,  255, 249, 196},
    {1222, 255, 219, 173},
    {1650, 254, 170, 136},
    {2227, 253, 107, 100},
    {3007, 255, 255, 255},
    {5000, 255, 255, 255},
    {6000, 255, 255, 255},
    {7000, 255, 255, 255},
    {8000, 255, 255, 255}
  },
  { // Imhof Type 4, geomteric 1.5 8
    {0,    175, 224, 203},
    {264,  211, 237, 211},
    {396,  254, 254, 234},
    {594,  252, 243, 210},
    {891,  237, 221, 195},
    {1336, 221, 199, 175},
    {2004, 215, 170, 148},
    {3007, 255, 255, 255},
    {4000, 255, 255, 255},
    {5000, 255, 255, 255},
    {6000, 255, 255, 255},
    {7000, 255, 255, 255},
    {8000, 255, 255, 255}
  },
  { // Imhof Type 12, geomteric  1.5 8
    {0,    165, 220, 201},
    {399,  219, 239, 212},
    {558,  254, 253, 230},
    {782,  254, 247, 211},
    {1094,  254, 237, 202},
    {1532, 254, 226, 207},
    {2145, 254, 209, 204},
    {3004, 255, 255, 255},
    {4000, 255, 255, 255},
    {5000, 255, 255, 255},
    {6000, 255, 255, 255},
    {7000, 255, 255, 255},
    {8000, 255, 255, 255}
  },
  { // Imhof Atlas der Schweiz
    {0,     47, 101, 147},
    {368,   58, 129, 152},
    {496,  117, 148, 153},
    {670,  155, 178, 140},
    {905,  192, 190, 139},
    {1222, 215, 199, 137},
    {1650, 229, 203, 171},
    {2227, 246, 206, 171},
    {3007, 252, 246, 244},
    {5001, 252, 246, 244},
    {7000, 252, 246, 244},
    {8000, 252, 246, 244},
    {9000, 252, 246, 244}
  },
  { // ICAO
    {0,           180, 205, 181},
    {199,         180, 205, 181},
    {200,         225, 233, 192},
    {499,         225, 233, 192},
    {500,         255, 249, 196},
    {999,         255, 249, 196},
    {1000,        255, 219, 173},
    {1499,        255, 219, 173},
    {1500,        254, 170, 136},
    {1999,        254, 170, 136},
    {2000,        253, 107, 100},
    {2499,        253, 107, 100},
    {2500,        255, 255, 255}
  },
  { // Grey
    {0,           220, 220, 220},
    {100,         220, 220, 220},
    {200,         220, 220, 220},
    {300,         220, 220, 220},
    {500,         220, 220, 220},
    {700,         220, 220, 220},
    {1000,        220, 220, 220},
    {1250,        220, 220, 220},
    {1500,        220, 220, 220},
    {1750,        220, 220, 220},
    {2000,        220, 220, 220},
    {2250,        220, 220, 220},
    {2500,        220, 220, 220}
  }
};


#define MIX(x,y,i) (BYTE)((x*i+y*((1<<7)-i))>>7)

inline void
TerrainShading(const short illum, BYTE &r, BYTE &g, BYTE &b)
{
  char x;
  if (illum < 0) {
    // shadow to blue
    x = min(63, -illum);
    r = MIX(0,r,x);
    g = MIX(0,g,x);
    b = MIX(64,b,x);
  } else if (illum > 0) {
    // highlight to yellow
    x = min(32, illum / 2);
    r = MIX(255,r,x);
    g = MIX(255,g,x);
    b = MIX(16,b,x);
  }
}

// map scale is approximately 2 points on the grid
// therefore, want one to one mapping if mapscale is 0.5
// there are approx 30 pixels in mapscale
// 240/QUANTISATION_PIXELS resolution = 6 pixels per terrain
// (mapscale/30)  km/pixels
//        0.250   km/terrain
// (0.25*30/mapscale) pixels/terrain
//  mapscale/(0.25*30)
//  mapscale/7.5 terrain units/pixel
//
// this is for TerrainInfo.StepSize = 0.0025;
TerrainRenderer::TerrainRenderer(const RasterTerrain *_terrain,
    RasterWeather *_weather, const RECT &rc) :
  terrain(_terrain),
  weather(_weather),
  rect_big(rc)
{
  TerrainContrast = 150;
  TerrainBrightness = 36;
  TerrainRamp = 0;

  if (terrain == NULL || !terrain->IsDirectAccess()) {
    quantisation_pixels = 6;
  } else {
    // SAM: experiment with quantisation_pixels between 2 and 4
    quantisation_pixels = 2;

    // on my PDA (600MhZ, 320x240 screen):
    // quantisation_pixels=2, latency=170 ms
    // quantisation_pixels=3, latency=136 ms
    // quantisation_pixels=4, latency= 93 ms
  }

  blursize = (quantisation_pixels - 1) / 2;
  oversampling = max(1, (blursize + 1) / 2 + 1);
  if (blursize == 0)
    oversampling = 1;
    // no point in oversampling,
    // just let stretchblt do the scaling

  /*
  dtq  ovs  blur  res_x  res_y   sx  sy  terrain_loads  pixels
  1    1    0     320    240    320 240    76800        76800
  2    1    0     160    120    160 120    19200        19200
  3    2    1     213    160    107  80     8560        34080
  4    2    1     160    120     80  60     4800        19200
  5    3    2     192    144     64  48     3072        27648
  */

  // scale quantisation_pixels so resolution is not too high on large displays
  quantisation_pixels = Layout::FastScale(quantisation_pixels);

  const int res_x = 
    iround((rc.right - rc.left) * oversampling / quantisation_pixels);
  const int res_y = 
    iround((rc.bottom - rc.top) * oversampling / quantisation_pixels);

  sbuf = new CSTScreenBuffer();
  sbuf->Create(res_x, res_y, Color::WHITE);
  width_sub = sbuf->GetCorrectedWidth() / oversampling;
  height_sub = sbuf->GetHeight() / oversampling;

  hBuf = (unsigned short*)malloc(sizeof(unsigned short) * width_sub * height_sub);

  colorBuf = (BGRColor*)malloc(256 * 128 * sizeof(BGRColor));

  rounding = new RasterRounding();
}


TerrainRenderer::~TerrainRenderer()
{
  if (hBuf)
    free(hBuf);

  if (colorBuf)
    free(colorBuf);

  if (sbuf)
    delete sbuf;

  if (rounding)
    delete rounding;
}

bool
TerrainRenderer::SetMap(const GEOPOINT &loc, int day_time)
{
  if (weather != NULL && weather->GetParameter())
    weather->Reload(loc, day_time);

  interp_levels = 5;
  switch (weather != NULL ? weather->GetParameter() : 0) {
  case 1: // wstar
    is_terrain = false;
    do_water = false;
    height_scale = 2; // max range 256*(2**2) = 1024 cm/s = 10 m/s
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[0][0];
    break;

  case 2: // bl wind spd
    is_terrain = false;
    do_water = false;
    height_scale = 3;
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[1][0];
    break;

  case 3: // hbl
    is_terrain = false;
    do_water = false;
    height_scale = 4;
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[2][0];
    break;

  case 4: // dwcrit
    is_terrain = false;
    do_water = false;
    height_scale = 4;
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[2][0];
    break;

  case 5: // blcloudpct
    is_terrain = false;
    do_water = true;
    height_scale = 0;
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[3][0];
    break;

  case 6: // sfctemp
    is_terrain = false;
    do_water = false;
    height_scale = 0;
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[4][0];
    break;

  case 7: // hwcrit
    is_terrain = false;
    do_water = false;
    height_scale = 4;
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[2][0];
    break;

  case 8: // wblmaxmin
    is_terrain = false;
    do_water = false;
    height_scale = 1; // max range 256*(1**2) = 512 cm/s = 5.0 m/s
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[5][0];
    break;

  case 9: // blcwbase
    is_terrain = false;
    do_water = false;
    height_scale = 4;
    DisplayMap = weather->GetMap();
    color_ramp = &weather_colors[2][0];
    break;

  default:
  case 0: // terrain!
    interp_levels = 2;
    is_terrain = true;
    do_water = true;
    height_scale = 4;
    DisplayMap = terrain != NULL ? terrain->get_map() : NULL;
    color_ramp = &terrain_colors[TerrainRamp][0];
    break;
  }

  if (is_terrain)
    do_shading = true;
  else
    do_shading = false;

  if (DisplayMap)
    return true;
  else
    return false;
}

const RECT
TerrainRenderer::Height(const Projection &map_projection)
{
  GEOPOINT Gx, Gy, Gmid;
  int x, y;

  const RECT rect_visible = map_projection.GetMapRect();

  x = (rect_visible.left + rect_visible.right) / 2;
  y = (rect_visible.top + rect_visible.bottom) / 2;
  map_projection.Screen2LonLat(x, y, Gmid);

  const int dstep = (int)lround(quantisation_pixels);

  GEOPOINT delta_rounding;

  x = (rect_visible.left + rect_visible.right) / 2 + dstep;
  y = (rect_visible.top + rect_visible.bottom) / 2;
  map_projection.Screen2LonLat(x, y, Gx);
  delta_rounding.Longitude = (Gx.Longitude - Gmid.Longitude);

  const fixed pixelDX = Distance(Gmid, Gx);

  x = (rect_visible.left + rect_visible.right) / 2;
  y = (rect_visible.top + rect_visible.bottom) / 2 + dstep;
  map_projection.Screen2LonLat(x, y, Gy);
  delta_rounding.Latitude = (Gy.Latitude - Gmid.Latitude);

  const fixed pixelDY = Distance(Gmid, Gy);

  pixelsize_d = sqrt((pixelDX * pixelDX + pixelDY * pixelDY)*fixed_half);

  // OK, ready to start loading height

  DisplayMap->LockRead();

  if (DisplayMap->IsDirectAccess()) {
    delta_rounding = GEOPOINT();
  }

  // set resolution
  rounding->Set(*DisplayMap, delta_rounding);

  quantisation_effective = DisplayMap->GetEffectivePixelSize(pixelsize_d, Gmid);

  if (quantisation_effective > min(width_sub, height_sub) / 4) {
    do_shading = false;
  }

  RECT rect_quantised = rect_visible;
  rect_quantised.left= rect_visible.left/quantisation_pixels; 
  rect_quantised.right= min(width_sub+rect_quantised.left, 
                            rect_visible.right/quantisation_pixels); 
  rect_quantised.top= rect_visible.top/quantisation_pixels; 
  rect_quantised.bottom= min(height_sub+rect_quantised.top, 
                             rect_visible.bottom/quantisation_pixels); 

  FillHeightBuffer(map_projection, rect_visible);

  DisplayMap->Unlock();

  if (weather != NULL && weather->GetParameter())
    ScanSpotHeights(rect_visible);

  return rect_quantised;
}

void
TerrainRenderer::ScanSpotHeights(const RECT& rect)
{
  spot_max_pt.x = -1;
  spot_max_pt.y = -1;
  spot_min_pt.x = -1;
  spot_min_pt.y = -1;
  spot_max_val = -1;
  spot_min_val = 32767;

  const unsigned short* h_buf = hBuf;

  for (int y = rect.top; y < rect.bottom; y += quantisation_pixels) {
    for (int x = rect.left; x < rect.right; x += quantisation_pixels, ++h_buf) {
      const short val = *h_buf;
      if (val > spot_max_val) {
        spot_max_val = val;
        spot_max_pt.x = x;
        spot_max_pt.y = y;
      }
      if (val < spot_min_val) {
        spot_min_val = val;
        spot_min_pt.x = x;
        spot_min_pt.y = y;
      }
    }
  }
}


void
TerrainRenderer::FillHeightBuffer(const Projection &map_projection,
                                  const RECT& rect)
{
  const int width = (rect.right-rect.left)/quantisation_pixels;

  #ifndef SLOW_TERRAIN_STUFF
  // This code is quickest (by a little) but not so readable

  const GEOPOINT PanLocation = map_projection.GetPanLocation();
  const fixed InvDrawScale = map_projection.GetScreenScaleToLonLat();
  const POINT Orig_Screen = map_projection.GetOrigScreen();
  const int cost = map_projection.GetDisplayAngle().ifastcosine();
  const int sint = map_projection.GetDisplayAngle().ifastsine();

  GEOPOINT gp;
  for (int y = rect.top; y < rect.bottom; y += quantisation_pixels) {
    const int dy = y-Orig_Screen.y;
    const int dycost = dy * cost+512;
    const int dysint = dy * sint-512;

    unsigned short* h_buf = hBuf+width*y/quantisation_pixels+rect.left/quantisation_pixels;

    for (int x = rect.left; x < rect.right; x += quantisation_pixels, ++h_buf) {
      const int dx = x-Orig_Screen.x;
      const POINT r = { (dx*cost - dysint)/1024,
                        (dycost + dx*sint)/1024 };
      gp.Latitude = PanLocation.Latitude 
        - Angle::native(r.y*InvDrawScale);
      gp.Longitude = PanLocation.Longitude + Angle::native(r.x*InvDrawScale)
        *gp.Latitude.invfastcosine();
      
      *h_buf = max((short)0, DisplayMap->GetField(gp, *rounding));
    }
  }

  #else

  // This code is marginally slower but readable

  for (int y = rect.top; y < rect.bottom; y += quantisation_pixels) {

    unsigned short* h_buf = hBuf+width*y/quantisation_pixels+rect.left/quantisation_pixels;

    for (int x = rect.left; x < rect.right; x += quantisation_pixels, ++h_buf) {
      GEOPOINT gp;
      map_projection.Screen2LonLat(x, y, gp);
      *h_buf = max((short)0, DisplayMap->GetField(gp, *rounding));
    }
  }

  #endif
}

const RECT 
TerrainRenderer::BorderSlope(const RECT& rect_quantised, 
                             const int edge) const
{
  RECT border = rect_quantised;
  InflateRect(&border, -edge, -edge);
  return border;
}

// JMW: if zoomed right in (e.g. one unit is larger than terrain
// grid), then increase the step size to be equal to the terrain
// grid for purposes of calculating slope, to avoid shading problems
// (gridding of display) This is why quantisation_effective is used instead of 1
// previously.  for large zoom levels, quantisation_effective=1
void
TerrainRenderer::Slope(const RECT& rect_quantised, 
                       const int sx, const int sy, const int sz)
{
  const RECT border = BorderSlope(rect_quantised, quantisation_effective);
  const int height_slope_factor = max(1, (int)(pixelsize_d));
  const int terrain_contrast = TerrainContrast;
  const int width_q = (rect_quantised.right-rect_quantised.left);

  const unsigned short* h_buf = hBuf;

  const BGRColor* oColorBuf = colorBuf + 64 * 256;
  BGRColor* imageBuf = sbuf->GetBuffer();
  if (!imageBuf)
    return;

  if (do_shading) {
    for (int y = rect_quantised.top; y < rect_quantised.bottom; ++y) {
      const int row_plus_index = ((y< border.bottom)? 
                                  quantisation_effective: 
                                  rect_quantised.bottom-1-y);
      assert(row_plus_index>=0);

      const int row_plus_offset = width_q*row_plus_index;
      
      const int row_minus_index = (y>= border.top)?
        quantisation_effective: y-rect_quantised.top;

      assert(row_minus_index>=0);

      const int row_minus_offset = width_q*row_minus_index;
      
      const int p31 = row_plus_index+row_minus_index;
      
      BGRColor* i_buf = imageBuf+rect_quantised.left+y*width_sub;
      
      for (int x = rect_quantised.left; x < rect_quantised.right; ++x, ++h_buf) {

        assert(i_buf < imageBuf+width_sub*height_sub); 
        
        if (short h = *h_buf) {
          h = min(255, h >> height_scale);
          
          // no need to calculate slope if undefined height or sea level

          // Y direction
          assert(h_buf-row_minus_offset >= hBuf);
          assert(h_buf+row_plus_offset >= hBuf);
          assert(h_buf-row_minus_offset < hBuf+width_sub*height_sub);
          assert(h_buf+row_plus_offset < hBuf+width_sub*height_sub);

          const int p32 = 
            h_buf[-row_minus_offset]-
            h_buf[row_plus_offset];

          // X direction

          const int column_plus_index = (x< border.right)? 
            quantisation_effective: rect_quantised.right-1-x;

          assert(column_plus_index>=0);

          const int column_minus_index = (x>= border.left)?
            quantisation_effective: x-rect_quantised.left;

          assert(column_minus_index>=0);

          assert(h_buf-column_minus_index >= hBuf);
          assert(h_buf+column_plus_index >= hBuf);
          assert(h_buf-column_minus_index < hBuf+width_sub*height_sub);
          assert(h_buf+column_plus_index < hBuf+width_sub*height_sub);

          const int p22 = 
            h_buf[column_plus_index]-
            h_buf[-column_minus_index];

          const int p20 = column_plus_index+column_minus_index;

          const long dd0 = p22 * p31;
          const long dd1 = p20 * p32;
          const long dd2 = p20 * p31 * height_slope_factor;
          const long mag = (dd0 * dd0 + dd1 * dd1 + dd2 * dd2);
          if (mag>0) {
            const long num = (dd2 * sz + dd0 * sx + dd1 * sy);
            const int sval = num/(int)sqrt((fixed)mag);
            const int sindex = max(-64, min(63, (sval - sz) * terrain_contrast / 128));
            *i_buf++ = oColorBuf[h + 256*sindex];
            continue;        
          } else {
            // slope is zero, so just look up the color
            *i_buf++ = oColorBuf[h];
          }
        } else {
          // we're in the water, so look up the color for water
          *i_buf++ = oColorBuf[255];
        }
      }
    }
  } else {
    for (int y = rect_quantised.top; y < rect_quantised.bottom; ++y) {
      BGRColor* i_buf = imageBuf+rect_quantised.left+y*width_sub;
      for (int x = rect_quantised.left; x < rect_quantised.right; ++x) {
        if (short h = *h_buf++) {
          h = min(255, h >> height_scale);
          *i_buf++ = oColorBuf[h];
        } else {
          // we're in the water, so look up the color for water
          *i_buf++ = oColorBuf[255];
        }
      }
    }
  }
}


void
TerrainRenderer::ColorTable()
{
  static const COLORRAMP *lastColorRamp = NULL;
  if (color_ramp == lastColorRamp)
    // no need to update the color table
    return;

  lastColorRamp = color_ramp;

  for (int i = 0; i < 256; i++) {
    for (int mag = -64; mag < 64; mag++) {
      BYTE r, g, b;
      if (i == 255) {
        if (do_water) {
          // water colours
          r = 85;
          g = 160;
          b = 255;
        } else {
          r = 255;
          g = 255;
          b = 255;

          // ColorRampLookup(0, r, g, b,
          // Color_ramp, NUM_COLOR_RAMP_LEVELS, interp_levels);
        }
      } else {
        ColorRampLookup(i << height_scale, r, g, b, color_ramp,
            NUM_COLOR_RAMP_LEVELS, interp_levels);
        TerrainShading(mag, r, g, b);
      }

      colorBuf[i + (mag + 64) * 256] = BGRColor(r, g, b);
    }
  }
}

void
TerrainRenderer::Draw(Canvas &canvas, RECT rc)
{
  sbuf->Zoom(oversampling);

  if (blursize > 0) {
    sbuf->HorizontalBlur(blursize);
    sbuf->VerticalBlur(blursize);
  }
  sbuf->DrawStretch(canvas, rc);
}

/**
 * Draws the terrain to the given canvas
 * @param canvas The drawing canvas
 * @param map_projection The Projection
 * @param sunazimuth Azimuth of the sun (for terrain shading)
 * @param sunelevation Azimuth of the sun (for terrain shading)
 * @param loc Current location
 * @return (?)
 */
bool
TerrainRenderer::Draw(Canvas &canvas,
                      const Projection &map_projection,
    const Angle sunazimuth, const Angle sunelevation, const GEOPOINT &loc,
    int day_time)
{
  if (!SetMap(loc, day_time))
    return false;

  // step 1: calculate sunlight vector
  const Angle fudgeelevation = 
    Angle::degrees(fixed(10.0 + 80.0 * TerrainBrightness / 255.0));

  const int sx = (int)(255 * fudgeelevation.fastcosine() * sunazimuth.fastsine());
  const int sy = (int)(255 * fudgeelevation.fastcosine() * sunazimuth.fastcosine());
  const int sz = (int)(255 * fudgeelevation.fastsine());

  ColorTable();

  // step 2: fill height buffer
  const RECT rect_quantised = Height(map_projection);

  // step 3: calculate derivatives of height buffer
  // step 4: calculate illumination and colors
  const RECT rect_visible = map_projection.GetMapRect();

  Slope(rect_quantised, sx, sy, sz);

  // step 5: draw
  Draw(canvas, rect_visible);

  // note, not all of this really needs to be locked
  return true;
}
