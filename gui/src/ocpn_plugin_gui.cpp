/**************************************************************************
 *   Copyright (C) 2024 by David S. Register                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 **************************************************************************/

/**
 * \file
 *
 * ocpn_plugin.h GUI API funtions up to api level 1.20
 */
#include <cstddef>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "dychart.h"  // Must be ahead due to buggy GL includes handling

#include <wx/wx.h>
#include <wx/arrstr.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/event.h>
#include <wx/glcanvas.h>
#include <wx/notebook.h>
#include <wx/string.h>
#include <wx/window.h>

#include "o_sound/o_sound.h"

#include "model/ais_decoder.h"
#include "model/comm_bridge.h"
#include "model/comm_navmsg_bus.h"
#include "model/gui_vars.h"
#include "model/idents.h"
#include "model/multiplexer.h"
#include "model/navobj_db.h"
#include "model/notification_manager.h"
#include "model/own_ship.h"
#include "model/plugin_comm.h"
#include "model/svg_utils.h"
#include "model/route.h"
#include "model/track.h"

#include "ais.h"
#include "chartdb.h"
#include "chcanv.h"
#include "cm93.h"
#include "config_mgr.h"
#include "font_mgr.h"
#include "gl_chart_canvas.h"
#include "gui_lib.h"
#include "navutil.h"
#include "ocpn_aui_manager.h"
#include "ocpn_frame.h"
#include "ocpn_platform.h"
#include "ocpn_plugin.h"
#include "options.h"
#include "piano.h"
#include "pluginmanager.h"
#include "routemanagerdialog.h"
#include "routeman_gui.h"
#include "s52plib.h"
#include "s57chart.h"
#include "shapefile_basemap.h"
#include "toolbar.h"
#include "waypointman_gui.h"

#if wxUSE_XLOCALE || !wxCHECK_VERSION(3, 0, 0)
extern wxLocale* plocale_def_lang;
#endif

extern PlugInManager* s_ppim;  // FIXME (leamas) another name for global mgr

extern options* g_pOptions;  // FIXME (leamas) merge to g_options

extern arrayofCanvasPtr g_canvasArray;  // FIXME (leamas) find new home

void NotifySetupOptionsPlugin(const PlugInData* pic);

//---------------------------------------------------------------------------
/*  Implementation of OCPN core functions callable by plugins
 *  Sorted by API version number
 *  The definitions of this API are found in ocpn_plugin.h
 *  PlugIns may call these static functions as necessary for system services
 */
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//    API 1.6
//---------------------------------------------------------------------------
/*  Main Toolbar support  */
int InsertPlugInTool(wxString label, wxBitmap* bitmap, wxBitmap* bmpRollover,
                     wxItemKind kind, wxString shortHelp, wxString longHelp,
                     wxObject* clientData, int position, int tool_sel,
                     opencpn_plugin* pplugin) {
  if (s_ppim)
    return s_ppim->AddToolbarTool(label, bitmap, bmpRollover, kind, shortHelp,
                                  longHelp, clientData, position, tool_sel,
                                  pplugin);
  else
    return -1;
}

void RemovePlugInTool(int tool_id) {
  if (s_ppim) s_ppim->RemoveToolbarTool(tool_id);
}

void SetToolbarToolViz(int item, bool viz) {
  if (s_ppim) s_ppim->SetToolbarToolViz(item, viz);
}

void SetToolbarItemState(int item, bool toggle) {
  if (s_ppim) s_ppim->SetToolbarItemState(item, toggle);
}

void SetToolbarToolBitmaps(int item, wxBitmap* bitmap, wxBitmap* bmpRollover) {
  if (s_ppim) s_ppim->SetToolbarItemBitmaps(item, bitmap, bmpRollover);
}

int InsertPlugInToolSVG(wxString label, wxString SVGfile,
                        wxString SVGfileRollover, wxString SVGfileToggled,
                        wxItemKind kind, wxString shortHelp, wxString longHelp,
                        wxObject* clientData, int position, int tool_sel,
                        opencpn_plugin* pplugin) {
  if (s_ppim)
    return s_ppim->AddToolbarTool(label, SVGfile, SVGfileRollover,
                                  SVGfileToggled, kind, shortHelp, longHelp,
                                  clientData, position, tool_sel, pplugin);
  else
    return -1;
}

void SetToolbarToolBitmapsSVG(int item, wxString SVGfile,
                              wxString SVGfileRollover,
                              wxString SVGfileToggled) {
  if (s_ppim)
    s_ppim->SetToolbarItemBitmaps(item, SVGfile, SVGfileRollover,
                                  SVGfileToggled);
}

/*  Canvas Context Menu support  */
int AddCanvasMenuItem(wxMenuItem* pitem, opencpn_plugin* pplugin,
                      const char* name) {
  if (s_ppim)
    return s_ppim->AddCanvasContextMenuItemPIM(pitem, pplugin, name, false);
  else
    return -1;
}

void SetCanvasMenuItemViz(int item, bool viz, const char* name) {
  if (s_ppim) s_ppim->SetCanvasContextMenuItemViz(item, viz, name);
}

void SetCanvasMenuItemGrey(int item, bool grey, const char* name) {
  if (s_ppim) s_ppim->SetCanvasContextMenuItemGrey(item, grey, name);
}

void RemoveCanvasMenuItem(int item, const char* name) {
  if (s_ppim) s_ppim->RemoveCanvasContextMenuItem(item, name);
}

int AddCanvasContextMenuItem(wxMenuItem* pitem, opencpn_plugin* pplugin) {
  /* main context popup menu */
  if (s_ppim)
    return s_ppim->AddCanvasContextMenuItemPIM(pitem, pplugin, "", false);
  else
    return -1;
}

void SetCanvasContextMenuItemViz(int item, bool viz) {
  SetCanvasMenuItemViz(item, viz);
}

void SetCanvasContextMenuItemGrey(int item, bool grey) {
  SetCanvasMenuItemGrey(item, grey);
}

void RemoveCanvasContextMenuItem(int item) { RemoveCanvasMenuItem(item); }

int AddCanvasContextMenuItemExt(wxMenuItem* pitem, opencpn_plugin* pplugin,
                                const std::string object_type) {
  /* main context popup menu */
  if (s_ppim)
    return s_ppim->AddCanvasContextMenuItemPIM(pitem, pplugin,
                                               object_type.c_str(), true);
  else
    return -1;
}

/*  Utility functions  */
wxFileConfig* GetOCPNConfigObject() {
  if (s_ppim)
    return reinterpret_cast<wxFileConfig*>(
        pConfig);  // return the global application config object
  else
    return NULL;
}

wxWindow* GetOCPNCanvasWindow() {
  wxWindow* pret = NULL;
  if (s_ppim) {
    AbstractTopFrame* pFrame = s_ppim->GetParentFrame();
    pret = (wxWindow*)pFrame->GetAbstractPrimaryCanvas();
  }
  return pret;
}

void RequestRefresh(wxWindow* win) {
  if (win) win->Refresh(true);
}

void GetCanvasPixLL(PlugIn_ViewPort* vp, wxPoint* pp, double lat, double lon) {
  //    Make enough of an application viewport to run its method....
  ViewPort ocpn_vp;
  ocpn_vp.clat = vp->clat;
  ocpn_vp.clon = vp->clon;
  ocpn_vp.m_projection_type = vp->m_projection_type;
  ocpn_vp.view_scale_ppm = vp->view_scale_ppm;
  ocpn_vp.skew = vp->skew;
  ocpn_vp.rotation = vp->rotation;
  ocpn_vp.pix_width = vp->pix_width;
  ocpn_vp.pix_height = vp->pix_height;

  wxPoint ret = ocpn_vp.GetPixFromLL(lat, lon);
  pp->x = ret.x;
  pp->y = ret.y;
}

void GetDoubleCanvasPixLL(PlugIn_ViewPort* vp, wxPoint2DDouble* pp, double lat,
                          double lon) {
  //    Make enough of an application viewport to run its method....
  ViewPort ocpn_vp;
  ocpn_vp.clat = vp->clat;
  ocpn_vp.clon = vp->clon;
  ocpn_vp.m_projection_type = vp->m_projection_type;
  ocpn_vp.view_scale_ppm = vp->view_scale_ppm;
  ocpn_vp.skew = vp->skew;
  ocpn_vp.rotation = vp->rotation;
  ocpn_vp.pix_width = vp->pix_width;
  ocpn_vp.pix_height = vp->pix_height;

  *pp = ocpn_vp.GetDoublePixFromLL(lat, lon);
}

void GetCanvasLLPix(PlugIn_ViewPort* vp, wxPoint p, double* plat,
                    double* plon) {
  //    Make enough of an application viewport to run its method....
  ViewPort ocpn_vp;
  ocpn_vp.clat = vp->clat;
  ocpn_vp.clon = vp->clon;
  ocpn_vp.m_projection_type = vp->m_projection_type;
  ocpn_vp.view_scale_ppm = vp->view_scale_ppm;
  ocpn_vp.skew = vp->skew;
  ocpn_vp.rotation = vp->rotation;
  ocpn_vp.pix_width = vp->pix_width;
  ocpn_vp.pix_height = vp->pix_height;

  return ocpn_vp.GetLLFromPix(p, plat, plon);
}

bool GetGlobalColor(wxString colorName, wxColour* pcolour) {
  wxColour c = GetGlobalColor(colorName);
  *pcolour = c;

  return true;
}

wxFont* OCPNGetFont(wxString TextElement, int default_size) {
  return FontMgr::Get().GetFontLegacy(TextElement, default_size);
}

wxFont* GetOCPNScaledFont_PlugIn(wxString TextElement, int default_size) {
  return FontMgr::Get().GetFontLegacy(TextElement, default_size);
}

double GetOCPNGUIToolScaleFactor_PlugIn(int GUIScaleFactor) {
  return g_Platform->GetToolbarScaleFactor(GUIScaleFactor);
}

double GetOCPNGUIToolScaleFactor_PlugIn() {
  return g_Platform->GetToolbarScaleFactor(g_GUIScaleFactor);
}

float GetOCPNChartScaleFactor_Plugin() {
  return g_Platform->GetChartScaleFactorExp(g_ChartScaleFactor);
}

wxFont GetOCPNGUIScaledFont_PlugIn(wxString item) {
  return GetOCPNGUIScaledFont(item);
}

bool AddPersistentFontKey(wxString TextElement) {
  return FontMgr::Get().AddAuxKey(TextElement);
}

wxString GetActiveStyleName() {
  if (g_StyleManager)
    return g_StyleManager->GetCurrentStyle()->name;
  else
    return "";
}

wxBitmap GetBitmapFromSVGFile(wxString filename, unsigned int width,
                              unsigned int height) {
  wxBitmap bmp = LoadSVG(filename, width, height);

  if (bmp.IsOk())
    return bmp;
  else {
    // On error in requested width/height parameters,
    // try to find and use dimensions embedded in the SVG file
    unsigned int w, h;
    SVGDocumentPixelSize(filename, w, h);
    if (w == 0 || h == 0) {
      // We did not succeed in deducing the size from SVG (svg element
      // x misses width, height or both attributes), let's use some "safe"
      // default
      w = 32;
      h = 32;
    }
    return LoadSVG(filename, w, h);
  }
}

bool IsTouchInterface_PlugIn() { return g_btouch; }

wxColour GetFontColour_PlugIn(wxString TextElement) {
  return FontMgr::Get().GetFontColor(TextElement);
}

wxString* GetpSharedDataLocation() { return g_Platform->GetSharedDataDirPtr(); }

ArrayOfPlugIn_AIS_Targets* GetAISTargetArray() {
  if (!g_pAIS) return NULL;

  ArrayOfPlugIn_AIS_Targets* pret = new ArrayOfPlugIn_AIS_Targets;

  //      Iterate over the AIS Target Hashmap
  for (const auto& it : g_pAIS->GetTargetList()) {
    auto td = it.second;
    PlugIn_AIS_Target* ptarget = Create_PI_AIS_Target(td.get());
    pret->Add(ptarget);
  }

//  Test one alarm target
#if 0
    AisTargetData td;
    td.n_alarm_state = AIS_ALARM_SET;
    PlugIn_AIS_Target *ptarget = Create_PI_AIS_Target(&td);
    pret->Add(ptarget);
#endif
  return pret;
}

wxAuiManager* GetFrameAuiManager() { return g_pauimgr; }

void SendPluginMessage(wxString message_id, wxString message_body) {
  SendMessageToAllPlugins(message_id, message_body);

  //  We will send an event to the main application frame (gFrame)
  //  for informational purposes.
  //  Of course, gFrame is encouraged to use any or all the
  //  data flying by if judged useful and dependable....

  OCPN_MsgEvent Nevent(wxEVT_OCPN_MSG, 0);
  Nevent.SetID(message_id);
  Nevent.SetJSONText(message_body);
  gFrame->GetEventHandler()->AddPendingEvent(Nevent);
}

void DimeWindow(wxWindow* win) { DimeControl(win); }

void JumpToPosition(double lat, double lon, double scale) {
  gFrame->JumpToPosition(gFrame->GetFocusCanvas(), lat, lon, scale);
}

/*  Locale (i18N) support  */
bool AddLocaleCatalog(wxString catalog) {
#if wxUSE_XLOCALE || !wxCHECK_VERSION(3, 0, 0)

  if (plocale_def_lang) {
    // Add this catalog to the persistent catalog array
    g_locale_catalog_array.Add(catalog);

    return plocale_def_lang->AddCatalog(catalog);
  } else
#endif
    return false;
}

wxString GetLocaleCanonicalName() { return g_locale; }

/*  NMEA interface support  */
void PushNMEABuffer(wxString buf) {
  std::string full_sentence = buf.ToStdString();

  if ((full_sentence[0] == '$') || (full_sentence[0] == '!')) {  // Sanity check
    // We notify based on full message, including the Talker ID
    std::string id = full_sentence.substr(1, 5);

    // notify message listener
    auto address = std::make_shared<NavAddr0183>("virtual");
    auto msg = std::make_shared<const Nmea0183Msg>(id, full_sentence, address);
    NavMsgBus::GetInstance().Notify(std::move(msg));
  }
}

/*  Chart database access support  */
wxXmlDocument GetChartDatabaseEntryXML(int dbIndex, bool b_getGeom) {
  wxXmlDocument doc = ChartData->GetXMLDescription(dbIndex, b_getGeom);

  return doc;
}

bool UpdateChartDBInplace(wxArrayString dir_array, bool b_force_update,
                          bool b_ProgressDialog) {
  //    Make an array of CDI
  ArrayOfCDI ChartDirArray;
  for (unsigned int i = 0; i < dir_array.GetCount(); i++) {
    wxString dirname = dir_array[i];
    ChartDirInfo cdi;
    cdi.fullpath = dirname;
    cdi.magic_number = "";
    ChartDirArray.Add(cdi);
  }
  bool b_ret = gFrame->UpdateChartDatabaseInplace(
      ChartDirArray, b_force_update, b_ProgressDialog, ChartListFileName);
  gFrame->RefreshGroupIndices();
  gFrame->ChartsRefresh();
  return b_ret;
}

wxArrayString GetChartDBDirArrayString() {
  return ChartData->GetChartDirArrayString();
}

int AddChartToDBInPlace(wxString& full_path, bool b_RefreshCanvas) {
  // extract the path from the chart name
  wxFileName fn(full_path);
  wxString fdir = fn.GetPath();

  bool bret = false;
  if (ChartData) {
    bret = ChartData->AddSingleChart(full_path);

    if (bret) {
      // Save to disk
      pConfig->UpdateChartDirs(ChartData->GetChartDirArray());
      ChartData->SaveBinary(ChartListFileName);

      //  Completely reload the chart database, for a fresh start
      ArrayOfCDI XnewChartDirArray;
      pConfig->LoadChartDirArray(XnewChartDirArray);
      delete ChartData;
      ChartData = new ChartDB();
      ChartData->LoadBinary(ChartListFileName, XnewChartDirArray);

      // Update group contents
      if (g_pGroupArray) ChartData->ApplyGroupArray(g_pGroupArray);

      if (g_options && g_options->IsShown())
        g_options->UpdateDisplayedChartDirList(ChartData->GetChartDirArray());

      if (b_RefreshCanvas || !gFrame->GetPrimaryCanvas()->GetQuiltMode()) {
        gFrame->ChartsRefresh();
      }
    }
  }
  return bret;
}

int RemoveChartFromDBInPlace(wxString& full_path) {
  bool bret = false;
  if (ChartData) {
    bret = ChartData->RemoveSingleChart(full_path);

    // Save to disk
    pConfig->UpdateChartDirs(ChartData->GetChartDirArray());
    ChartData->SaveBinary(ChartListFileName);

    //  Completely reload the chart database, for a fresh start
    ArrayOfCDI XnewChartDirArray;
    pConfig->LoadChartDirArray(XnewChartDirArray);
    delete ChartData;
    ChartData = new ChartDB();
    ChartData->LoadBinary(ChartListFileName, XnewChartDirArray);

    // Update group contents
    if (g_pGroupArray) ChartData->ApplyGroupArray(g_pGroupArray);

    if (g_options && g_options->IsShown())
      g_options->UpdateDisplayedChartDirList(ChartData->GetChartDirArray());

    gFrame->ChartsRefresh();
  }

  return bret;
}

//---------------------------------------------------------------------------
//    API 1.9
//---------------------------------------------------------------------------
wxScrolledWindow* AddOptionsPage(OptionsParentPI parent, wxString title) {
  if (!g_pOptions) return NULL;

  size_t parentid;
  switch (parent) {
    case PI_OPTIONS_PARENT_DISPLAY:
      parentid = g_pOptions->m_pageDisplay;
      break;
    case PI_OPTIONS_PARENT_CONNECTIONS:
      parentid = g_pOptions->m_pageConnections;
      break;
    case PI_OPTIONS_PARENT_CHARTS:
      parentid = g_pOptions->m_pageCharts;
      break;
    case PI_OPTIONS_PARENT_SHIPS:
      parentid = g_pOptions->m_pageShips;
      break;
    case PI_OPTIONS_PARENT_UI:
      parentid = g_pOptions->m_pageUI;
      break;
    case PI_OPTIONS_PARENT_PLUGINS:
      parentid = g_pOptions->m_pagePlugins;
      break;
    default:
      wxLogMessage("Error in PluginManager::AddOptionsPage: Unknown parent");
      return NULL;
      break;
  }

  return g_pOptions->AddPage(parentid, title);
}

bool DeleteOptionsPage(wxScrolledWindow* page) {
  if (!g_pOptions) return false;
  return g_pOptions->DeletePluginPage(page);
}

bool DecodeSingleVDOMessage(const wxString& str, PlugIn_Position_Fix_Ex* pos,
                            wxString* accumulator) {
  if (!pos) return false;

  GenericPosDatEx gpd;
  AisError nerr = AIS_GENERIC_ERROR;
  if (g_pAIS) nerr = g_pAIS->DecodeSingleVDO(str, &gpd, accumulator);
  if (nerr == AIS_NoError) {
    pos->Lat = gpd.kLat;
    pos->Lon = gpd.kLon;
    pos->Cog = gpd.kCog;
    pos->Sog = gpd.kSog;
    pos->Hdt = gpd.kHdt;

    //  Fill in the dummy values
    pos->FixTime = 0;
    pos->Hdm = 1000;
    pos->Var = 1000;
    pos->nSats = 0;

    return true;
  }

  return false;
}

int GetChartbarHeight() {
  int val = 0;
  if (g_bShowChartBar) {
    ChartCanvas* cc = gFrame->GetPrimaryCanvas();
    if (cc && cc->GetPiano()) {
      val = cc->GetPiano()->GetHeight();
    }
  }
  return val;
}

bool GetRoutepointGPX(RoutePoint* pRoutePoint, char* buffer,
                      unsigned int buffer_length) {
  bool ret = false;

  NavObjectCollection1* pgpx = new NavObjectCollection1;
  pgpx->AddGPXWaypoint(pRoutePoint);
  wxString gpxfilename = wxFileName::CreateTempFileName("gpx");
  pgpx->SaveFile(gpxfilename);
  delete pgpx;

  wxFFile gpxfile(gpxfilename);
  wxString s;
  if (gpxfile.ReadAll(&s)) {
    if (s.Length() < buffer_length) {
      strncpy(buffer, (const char*)s.mb_str(wxConvUTF8), buffer_length - 1);
      ret = true;
    }
  }

  gpxfile.Close();
  ::wxRemoveFile(gpxfilename);

  return ret;
}

bool GetActiveRoutepointGPX(char* buffer, unsigned int buffer_length) {
  if (g_pRouteMan->IsAnyRouteActive())
    return GetRoutepointGPX(g_pRouteMan->GetpActivePoint(), buffer,
                            buffer_length);
  else
    return false;
}

void PositionBearingDistanceMercator_Plugin(double lat, double lon, double brg,
                                            double dist, double* dlat,
                                            double* dlon) {
  PositionBearingDistanceMercator(lat, lon, brg, dist, dlat, dlon);
}

void DistanceBearingMercator_Plugin(double lat0, double lon0, double lat1,
                                    double lon1, double* brg, double* dist) {
  DistanceBearingMercator(lat0, lon0, lat1, lon1, brg, dist);
}

double DistGreatCircle_Plugin(double slat, double slon, double dlat,
                              double dlon) {
  return DistGreatCircle(slat, slon, dlat, dlon);
}

void toTM_Plugin(float lat, float lon, float lat0, float lon0, double* x,
                 double* y) {
  toTM(lat, lon, lat0, lon0, x, y);
}

void fromTM_Plugin(double x, double y, double lat0, double lon0, double* lat,
                   double* lon) {
  fromTM(x, y, lat0, lon0, lat, lon);
}

void toSM_Plugin(double lat, double lon, double lat0, double lon0, double* x,
                 double* y) {
  toSM(lat, lon, lat0, lon0, x, y);
}

void fromSM_Plugin(double x, double y, double lat0, double lon0, double* lat,
                   double* lon) {
  fromSM(x, y, lat0, lon0, lat, lon);
}

void toSM_ECC_Plugin(double lat, double lon, double lat0, double lon0,
                     double* x, double* y) {
  toSM_ECC(lat, lon, lat0, lon0, x, y);
}

void fromSM_ECC_Plugin(double x, double y, double lat0, double lon0,
                       double* lat, double* lon) {
  fromSM_ECC(x, y, lat0, lon0, lat, lon);
}

double toUsrDistance_Plugin(double nm_distance, int unit) {
  return toUsrDistance(nm_distance, unit);
}

double fromUsrDistance_Plugin(double usr_distance, int unit) {
  return fromUsrDistance(usr_distance, unit);
}

double toUsrSpeed_Plugin(double kts_speed, int unit) {
  return toUsrSpeed(kts_speed, unit);
}

double toUsrWindSpeed_Plugin(double kts_speed, int unit) {
  return toUsrWindSpeed(kts_speed, unit);
}

double fromUsrSpeed_Plugin(double usr_speed, int unit) {
  return fromUsrSpeed(usr_speed, unit);
}

double fromUsrWindSpeed_Plugin(double usr_wspeed, int unit) {
  return fromUsrWindSpeed(usr_wspeed, unit);
}

double toUsrTemp_Plugin(double cel_temp, int unit) {
  return toUsrTemp(cel_temp, unit);
}

double fromUsrTemp_Plugin(double usr_temp, int unit) {
  return fromUsrTemp(usr_temp, unit);
}

wxString getUsrDistanceUnit_Plugin(int unit) {
  return getUsrDistanceUnit(unit);
}

wxString getUsrSpeedUnit_Plugin(int unit) { return getUsrSpeedUnit(unit); }

wxString getUsrWindSpeedUnit_Plugin(int unit) {
  return getUsrWindSpeedUnit(unit);
}

wxString getUsrTempUnit_Plugin(int unit) { return getUsrTempUnit(unit); }

/*
 * Depth Conversion Functions
 */
double toUsrDepth_Plugin(double m_depth, int unit) {
  return toUsrDepth(m_depth, unit);
}

double fromUsrDepth_Plugin(double usr_depth, int unit) {
  return fromUsrDepth(usr_depth, unit);
}

wxString getUsrDepthUnit_Plugin(int unit) { return getUsrDepthUnit(unit); }

/**
 * Height Conversion Functions
 */
double toUsrHeight_Plugin(double m_height, int unit) {
  return toUsrHeight(m_height, unit);
}

double fromUsrHeight_Plugin(double usr_height, int unit) {
  return fromUsrHeight(usr_height, unit);
}

wxString getUsrHeightUnit_Plugin(int unit) { return getUsrHeightUnit(unit); }

double fromDMM_PlugIn(wxString sdms) { return fromDMM(sdms); }

bool PlugIn_GSHHS_CrossesLand(double lat1, double lon1, double lat2,
                              double lon2) {
  // TODO: Enable call to gShapeBasemap.CrossesLand after fixing performance
  // issues. if (gShapeBasemap.IsUsable()) {
  //   return gShapeBasemap.CrossesLand(lat1, lon1, lat2, lon2);
  // } else {
  //  Fall back to the GSHHS data.
  static bool loaded = false;
  if (!loaded) {
    gshhsCrossesLandInit();
    loaded = true;
  }
  return gshhsCrossesLand(lat1, lon1, lat2, lon2);
  //}
}

namespace {

void SetSegmentSafetyMessage(PlugInSegmentSafetyResult* result,
                             const char* message) {
  if (!result) return;
  if (result->struct_size <
      (int)(offsetof(PlugInSegmentSafetyResult, message) +
            sizeof(result->message)))
    return;
  strncpy(result->message, message, sizeof(result->message) - 1);
  result->message[sizeof(result->message) - 1] = '\0';
}

void InitSegmentSafetyResult(PlugInSegmentSafetyResult* result) {
  if (!result) return;
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, status) +
            sizeof(result->status)))
    result->status = PI_SEGMENT_SAFETY_ERROR;
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, source) +
            sizeof(result->source)))
    result->source = PI_SEGMENT_SAFETY_SOURCE_NONE;
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, used_fallback) +
            sizeof(result->used_fallback)))
    result->used_fallback = 0;
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, message) +
            sizeof(result->message)))
    result->message[0] = '\0';
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, geometry_check_ms) +
            sizeof(result->geometry_check_ms))) {
    result->diagnostic_reason = PI_SEGMENT_SAFETY_DIAG_NONE;
    result->chart_stack_entries = 0;
    result->candidate_chart_count = 0;
    result->raster_chart_count = 0;
    result->unsupported_chart_count = 0;
    result->s57_chart_count = 0;
    result->land_ring_count = 0;
    result->bbox_ring_tests = 0;
    result->edge_tests = 0;
    result->cache_build_ms = 0;
    result->chart_select_ms = 0;
    result->geometry_check_ms = 0;
  }
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, chart_path) +
            sizeof(result->chart_path))) {
    result->chart_db_index = -1;
    result->hit_cause = PI_SEGMENT_SAFETY_HIT_NONE;
    result->hit_ring_min_lat = 0.0;
    result->hit_ring_max_lat = 0.0;
    result->hit_ring_min_lon = 0.0;
    result->hit_ring_max_lon = 0.0;
    result->hit_ring_point_count = 0;
    result->hit_edge_index = -1;
    result->chart_path[0] = '\0';
  }
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, hit_object) +
            sizeof(result->hit_object))) {
    result->hit_sample_lat = 0.0;
    result->hit_sample_lon = 0.0;
    result->hit_sample_index = -1;
    result->hit_sample_count = 0;
    result->chart_scale = -1;
    result->hit_object[0] = '\0';
  }
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, point_cache_misses) +
            sizeof(result->point_cache_misses))) {
    result->point_cache_hits = 0;
    result->point_cache_misses = 0;
  }
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, grid_lookups) +
            sizeof(result->grid_lookups))) {
    result->grid_cache_hits = 0;
    result->grid_cache_misses = 0;
    result->grid_build_ms = 0;
    result->grid_cells_total = 0;
    result->grid_cells_land = 0;
    result->grid_cells_water = 0;
    result->grid_cells_drying = 0;
    result->grid_cells_unknown = 0;
    result->grid_lookups = 0;
  }
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, unexpected_tile_builds) +
            sizeof(result->unexpected_tile_builds))) {
    result->grid_lookup_ms = 0;
    result->segment_sample_count = 0;
    result->water_tile_shortcuts = 0;
    result->unexpected_tile_builds = 0;
  }
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, unexpected_tile_min_lon) +
            sizeof(result->unexpected_tile_min_lon))) {
    result->unexpected_lat_tile = 0;
    result->unexpected_lon_tile = 0;
    result->unexpected_tile_min_lat = 0.0;
    result->unexpected_tile_min_lon = 0.0;
  }
}

void SetSegmentSafetyStatus(PlugInSegmentSafetyResult* result,
                            PlugInSegmentSafetyStatus status) {
  if (!result) return;
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, status) +
            sizeof(result->status)))
    result->status = status;
}

void SetSegmentSafetySource(PlugInSegmentSafetyResult* result,
                            PlugInSegmentSafetySource source) {
  if (!result) return;
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, source) +
            sizeof(result->source)))
    result->source = source;
}

void SetSegmentSafetyFallback(PlugInSegmentSafetyResult* result,
                              bool used_fallback) {
  if (!result) return;
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, used_fallback) +
            sizeof(result->used_fallback)))
    result->used_fallback = used_fallback ? 1 : 0;
}

PlugInSegmentSafetySource GetSegmentSafetySource(
    const PlugInSegmentSafetyResult* result) {
  if (!result) return PI_SEGMENT_SAFETY_SOURCE_NONE;
  if (result->struct_size >=
      (int)(offsetof(PlugInSegmentSafetyResult, source) +
            sizeof(result->source)))
    return (PlugInSegmentSafetySource)result->source;
  return PI_SEGMENT_SAFETY_SOURCE_NONE;
}

bool SegmentSafetyResultHas(const PlugInSegmentSafetyResult* result,
                            size_t offset, size_t size) {
  return result && result->struct_size >= (int)(offset + size);
}

void SetSegmentSafetyDiagnosticReason(
    PlugInSegmentSafetyResult* result,
    PlugInSegmentSafetyDiagnosticReason reason) {
  if (!SegmentSafetyResultHas(
          result, offsetof(PlugInSegmentSafetyResult, diagnostic_reason),
          sizeof(result->diagnostic_reason)))
    return;
  result->diagnostic_reason = reason;
}

void SetSegmentSafetyDiagnosticInt(PlugInSegmentSafetyResult* result,
                                   size_t offset, int value) {
  if (!SegmentSafetyResultHas(result, offset, sizeof(int))) return;
  *reinterpret_cast<int*>(reinterpret_cast<char*>(result) + offset) = value;
}

bool SegmentSafetyOptionsHas(const PlugInSegmentSafetyOptions* options,
                             size_t offset, size_t size) {
  return options && options->struct_size >= (int)(offset + size);
}

double SegmentSafetyOptionMargin(const PlugInSegmentSafetyOptions* options) {
  if (SegmentSafetyOptionsHas(
          options, offsetof(PlugInSegmentSafetyOptions, safety_margin_nm),
          sizeof(options->safety_margin_nm)))
    return wxMax(0.0, options->safety_margin_nm);
  return 0.0;
}

bool SegmentSafetyOptionCheckLand(const PlugInSegmentSafetyOptions* options) {
  if (SegmentSafetyOptionsHas(options,
                              offsetof(PlugInSegmentSafetyOptions, check_land),
                              sizeof(options->check_land)))
    return options->check_land != 0;
  return true;
}

bool SegmentSafetyOptionAllowGshhsFallback(
    const PlugInSegmentSafetyOptions* options) {
  if (SegmentSafetyOptionsHas(
          options, offsetof(PlugInSegmentSafetyOptions, allow_gshhs_fallback),
          sizeof(options->allow_gshhs_fallback)))
    return options->allow_gshhs_fallback != 0;
  return true;
}

bool IsSegmentSafetyLandObject(const char* feature_name) {
  return feature_name && !strncmp(feature_name, "LNDARE", 6);
}

bool IsCm93Chart(ChartBase* chart) {
  return chart && chart->GetChartType() == CHART_TYPE_CM93COMP;
}

std::string SegmentSafetyCacheKey(int db_index, bool cm93, double lat,
                                  double lon) {
  if (!cm93) return std::to_string(db_index);

  double lat_bucket = floor(lat * 4.0) / 4.0;
  double lon_bucket = floor(lon * 4.0) / 4.0;
  return wxString::Format("%d:%.2f:%.2f", db_index, lat_bucket, lon_bucket)
      .ToStdString();
}

ViewPort SegmentSafetyViewPortAt(double lat, double lon) {
  ViewPort vp;
  ChartCanvas* canvas = gFrame ? gFrame->GetFocusCanvas() : NULL;
  if (canvas) vp = canvas->GetVP();

  vp.clat = lat;
  vp.clon = lon;
  if (vp.pix_width <= 0) vp.pix_width = 1024;
  if (vp.pix_height <= 0) vp.pix_height = 768;
  if (vp.view_scale_ppm <= 0.0) vp.view_scale_ppm = 1.0 / 1852.0;
  if (vp.chart_scale <= 0.0) vp.chart_scale = 100000;
  if (vp.ref_scale <= 0) vp.ref_scale = vp.chart_scale;
  vp.SetBoxes();
  return vp;
}

double SegmentSafetyNormalizeBearing(double bearing) {
  while (bearing < 0.0) bearing += 360.0;
  while (bearing >= 360.0) bearing -= 360.0;
  return bearing;
}

bool ChartPointIsLand(s57chart* chart, double lat, double lon, ViewPort& vp) {
  if (!chart) return false;

  ListOfObjRazRules* rule_list =
      chart->GetObjRuleListAtLatLon(lat, lon, 0.0, &vp, MASK_AREA);
  if (!rule_list) return false;

  bool is_land = false;
  for (ListOfObjRazRules::Node* node = rule_list->GetFirst(); node;
       node = node->GetNext()) {
    ObjRazRules* rule = node->GetData();
    if (rule && rule->obj && IsSegmentSafetyLandObject(rule->obj->FeatureName)) {
      is_land = true;
      break;
    }
  }

  rule_list->Clear();
  delete rule_list;
  return is_land;
}

enum SegmentSafetyPointClass {
  SEGMENT_SAFETY_POINT_NO_DATA = 0,
  SEGMENT_SAFETY_POINT_WATER,
  SEGMENT_SAFETY_POINT_LAND,
  SEGMENT_SAFETY_POINT_DRYING
};

const char* SegmentSafetyPrimitiveName(GeoPrim_t primitive) {
  switch (primitive) {
    case GEO_POINT:
      return "point";
    case GEO_LINE:
      return "line";
    case GEO_AREA:
      return "area";
    case GEO_META:
      return "meta";
    case GEO_PRIM:
      return "prim";
    default:
      return "unknown";
  }
}

wxString SegmentSafetyObjectAttr(S57Obj* obj, const char* attr) {
  if (!obj || !attr) return wxString();
  wxString value = obj->GetAttrValueAsString(attr);
  value.Replace("\"", "'");
  value.Replace(";", ",");
  return value;
}

wxString SegmentSafetyRuleSummary(ObjRazRules* rule) {
  if (!rule || !rule->obj) return wxString();

  S57Obj* obj = rule->obj;
  wxString summary = wxString::Format("%s/%s", obj->FeatureName,
                                      SegmentSafetyPrimitiveName(
                                          obj->Primitive_type));
  if (rule->LUP) {
    summary += wxString::Format("/TNAM=%d/DPRI=%c/DISC=%c", rule->LUP->TNAM,
                                rule->LUP->DPRI, rule->LUP->DISC);
    if (!rule->LUP->INST.empty()) {
      wxString inst = rule->LUP->INST.Left(80);
      inst.Replace("\"", "'");
      inst.Replace(";", ",");
      summary += wxString::Format("/INST=%s", inst);
    }
  }

  const char* attrs[] = {"DRVAL1", "DRVAL2", "VALDCO", "WATLEV", "CATWAT"};
  for (size_t i = 0; i < WXSIZEOF(attrs); ++i) {
    wxString value = SegmentSafetyObjectAttr(obj, attrs[i]);
    if (!value.empty()) summary += wxString::Format("/%s=%s", attrs[i], value);
  }

  return summary;
}

s57chart* GetSegmentSafetyChartAtPoint(ChartCanvas* canvas, double lat,
                                       double lon, ViewPort& vp,
                                       PlugInSegmentSafetySource* source) {
  if (!canvas) return NULL;

  wxPoint point;
  if (!canvas->GetCanvasPointPixVP(vp, lat, lon, &point)) return NULL;

  ChartBase* chart = NULL;
  if (canvas->GetQuiltMode() && canvas->m_pQuilt) {
    chart = canvas->m_pQuilt->GetChartAtPix(vp, point);
    if (!chart) chart = canvas->m_pQuilt->GetOverlayChartAtPix(vp, point);
  } else {
    chart = canvas->m_singleChart;
  }

  s57chart* s57 = dynamic_cast<s57chart*>(chart);
  if (s57 && source) {
    *source = IsCm93Chart(chart) ? PI_SEGMENT_SAFETY_SOURCE_CM93
                                : PI_SEGMENT_SAFETY_SOURCE_VECTOR_CHART;
  }
  return s57;
}

bool ChartSegmentPointSamplesHitLand(double lat1, double lon1, double lat2,
                                     double lon2, double safety_margin_nm,
                                     PlugInSegmentSafetyResult* result,
                                     int* chart_sample_count,
                                     int* total_sample_count) {
  ChartCanvas* canvas =
      gFrame && gFrame->GetFocusCanvas() ? gFrame->GetFocusCanvas()
                                         : (gFrame ? gFrame->GetPrimaryCanvas()
                                                   : NULL);
  if (!canvas) return false;

  ViewPort vp = canvas->GetVP();
  double bearing = 0.0;
  double dist_nm = 0.0;
  ll_gc_ll_reverse(lat1, lon1, lat2, lon2, &bearing, &dist_nm);

  const int max_samples = 256;
  int samples = wxMax(2, wxMin(max_samples, (int)ceil(dist_nm / 0.1) + 1));
  if (total_sample_count) *total_sample_count = samples;

  for (int i = 0; i < samples; ++i) {
    double sample_dist = samples == 1 ? 0.0 : dist_nm * i / (samples - 1);
    double lat = lat1;
    double lon = lon1;
    if (sample_dist > 0.0)
      ll_gc_ll(lat1, lon1, bearing, sample_dist, &lat, &lon);

    PlugInSegmentSafetySource source = PI_SEGMENT_SAFETY_SOURCE_NONE;
    s57chart* chart = GetSegmentSafetyChartAtPoint(canvas, lat, lon, vp, &source);
    if (!chart) continue;

    if (chart_sample_count) ++*chart_sample_count;
    if (GetSegmentSafetySource(result) == PI_SEGMENT_SAFETY_SOURCE_NONE)
      SetSegmentSafetySource(result, source);

    if (ChartPointIsLand(chart, lat, lon, vp)) {
      if (result) {
        SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_CROSSES_LAND);
        SetSegmentSafetySource(result, source);
        SetSegmentSafetyMessage(result, "segment intersects chart land area");
      }
      return true;
    }

    if (safety_margin_nm > 0.0) {
      double left_lat, left_lon, right_lat, right_lon;
      ll_gc_ll(lat, lon, SegmentSafetyNormalizeBearing(bearing - 90.0),
               safety_margin_nm, &left_lat, &left_lon);
      ll_gc_ll(lat, lon, SegmentSafetyNormalizeBearing(bearing + 90.0),
               safety_margin_nm, &right_lat, &right_lon);
      if (ChartPointIsLand(chart, left_lat, left_lon, vp) ||
          ChartPointIsLand(chart, right_lat, right_lon, vp)) {
        if (result) {
          SetSegmentSafetyStatus(result,
                                 PI_SEGMENT_SAFETY_WITHIN_LAND_MARGIN);
          SetSegmentSafetySource(result, source);
          SetSegmentSafetyMessage(result,
                                  "segment is within chart land safety margin");
        }
        return true;
      }
    }
  }

  return false;
}

struct SegmentSafetyBBox {
  double min_lat;
  double max_lat;
  double min_lon;
  double max_lon;
};

struct CachedLandRing {
  std::vector<wxPoint2DDouble> points;
  SegmentSafetyBBox bbox;
};

struct CachedChartLandGeometry {
  bool loaded;
  PlugInSegmentSafetySource source;
  std::string cache_key;
  wxString chart_path;
  std::vector<CachedLandRing> rings;

  CachedChartLandGeometry()
      : loaded(false), source(PI_SEGMENT_SAFETY_SOURCE_NONE) {}
};

std::map<std::string, CachedChartLandGeometry> s_segment_safety_land_cache;

struct CachedPointSafetyClassification {
  SegmentSafetyPointClass point_class;
  PlugInSegmentSafetySource source;
  int chart_db_index;
  int chart_scale;
  char chart_path[256];
  char hit_object[128];

  CachedPointSafetyClassification()
      : point_class(SEGMENT_SAFETY_POINT_NO_DATA),
        source(PI_SEGMENT_SAFETY_SOURCE_NONE),
        chart_db_index(-1),
        chart_scale(-1) {
    chart_path[0] = '\0';
    hit_object[0] = '\0';
  }
};

std::map<std::string, CachedPointSafetyClassification>
    s_segment_safety_point_cache;
const size_t kMaxSegmentSafetyPointCacheEntries = 250000;

const double kSegmentSafetyGridTileDegrees = 0.05;
const double kSegmentSafetyGridResolutionDegrees = 0.00125;
const size_t kMaxSegmentSafetyGridTiles = 4096;

struct CachedPointSafetyGridTile {
  int group_index;
  long lat_tile;
  long lon_tile;
  double min_lat;
  double min_lon;
  double resolution;
  int rows;
  int cols;
  int land_count;
  int water_count;
  int drying_count;
  int unknown_count;
  bool built;
  int chart_db_index;
  int chart_scale;
  PlugInSegmentSafetySource source;
  char chart_path[256];
  std::vector<unsigned char> classes;

  CachedPointSafetyGridTile()
      : group_index(0),
        lat_tile(0),
        lon_tile(0),
        min_lat(0.0),
        min_lon(0.0),
        resolution(kSegmentSafetyGridResolutionDegrees),
        rows(0),
        cols(0),
        land_count(0),
        water_count(0),
        drying_count(0),
        unknown_count(0),
        built(false),
        chart_db_index(-1),
        chart_scale(-1),
        source(PI_SEGMENT_SAFETY_SOURCE_NONE) {
    chart_path[0] = '\0';
  }
};

std::map<std::string, CachedPointSafetyGridTile> s_segment_safety_grid_cache;

long s_segment_safety_chart_hit_logs = 0;
const long kMaxSegmentSafetyChartHitLogs = 20;

struct SegmentSafetyCoreStats {
  int chart_stack_entries;
  int candidate_chart_count;
  int raster_chart_count;
  int unsupported_chart_count;
  int s57_chart_count;
  int land_ring_count;
  int bbox_ring_tests;
  int edge_tests;
  int cache_build_ms;
  int chart_select_ms;
  int geometry_check_ms;
  int point_cache_hits;
  int point_cache_misses;
  int grid_cache_hits;
  int grid_cache_misses;
  int grid_build_ms;
  int grid_cells_total;
  int grid_cells_land;
  int grid_cells_water;
  int grid_cells_drying;
  int grid_cells_unknown;
  int grid_lookups;
  int grid_lookup_ms;
  int segment_sample_count;
  int water_tile_shortcuts;
  int unexpected_tile_builds;
  int unexpected_lat_tile;
  int unexpected_lon_tile;
  double unexpected_tile_min_lat;
  double unexpected_tile_min_lon;
  bool no_chart_database;
  bool chart_load_failed;
  bool zero_land_geometry;

  SegmentSafetyCoreStats()
      : chart_stack_entries(0),
        candidate_chart_count(0),
        raster_chart_count(0),
        unsupported_chart_count(0),
        s57_chart_count(0),
        land_ring_count(0),
        bbox_ring_tests(0),
        edge_tests(0),
        cache_build_ms(0),
        chart_select_ms(0),
        geometry_check_ms(0),
        point_cache_hits(0),
        point_cache_misses(0),
        grid_cache_hits(0),
        grid_cache_misses(0),
        grid_build_ms(0),
        grid_cells_total(0),
        grid_cells_land(0),
        grid_cells_water(0),
        grid_cells_drying(0),
        grid_cells_unknown(0),
        grid_lookups(0),
        grid_lookup_ms(0),
        segment_sample_count(0),
        water_tile_shortcuts(0),
        unexpected_tile_builds(0),
        unexpected_lat_tile(0),
        unexpected_lon_tile(0),
        unexpected_tile_min_lat(0.0),
        unexpected_tile_min_lon(0.0),
        no_chart_database(false),
        chart_load_failed(false),
        zero_land_geometry(false) {}
};

void ApplySegmentSafetyStats(PlugInSegmentSafetyResult* result,
                             const SegmentSafetyCoreStats& stats) {
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, chart_stack_entries),
      stats.chart_stack_entries);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, candidate_chart_count),
      stats.candidate_chart_count);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, raster_chart_count),
      stats.raster_chart_count);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, unsupported_chart_count),
      stats.unsupported_chart_count);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, s57_chart_count),
      stats.s57_chart_count);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, land_ring_count),
      stats.land_ring_count);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, bbox_ring_tests),
      stats.bbox_ring_tests);
  SetSegmentSafetyDiagnosticInt(result,
                                offsetof(PlugInSegmentSafetyResult, edge_tests),
                                stats.edge_tests);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, cache_build_ms),
      stats.cache_build_ms);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, chart_select_ms),
      stats.chart_select_ms);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, geometry_check_ms),
      stats.geometry_check_ms);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, point_cache_hits),
      stats.point_cache_hits);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, point_cache_misses),
      stats.point_cache_misses);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_cache_hits),
      stats.grid_cache_hits);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_cache_misses),
      stats.grid_cache_misses);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_build_ms),
      stats.grid_build_ms);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_cells_total),
      stats.grid_cells_total);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_cells_land),
      stats.grid_cells_land);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_cells_water),
      stats.grid_cells_water);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_cells_drying),
      stats.grid_cells_drying);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_cells_unknown),
      stats.grid_cells_unknown);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_lookups),
      stats.grid_lookups);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, grid_lookup_ms),
      stats.grid_lookup_ms);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, segment_sample_count),
      stats.segment_sample_count);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, water_tile_shortcuts),
      stats.water_tile_shortcuts);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, unexpected_tile_builds),
      stats.unexpected_tile_builds);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, unexpected_lat_tile),
      stats.unexpected_lat_tile);
  SetSegmentSafetyDiagnosticInt(
      result, offsetof(PlugInSegmentSafetyResult, unexpected_lon_tile),
      stats.unexpected_lon_tile);
  if (SegmentSafetyResultHas(
          result, offsetof(PlugInSegmentSafetyResult,
                           unexpected_tile_min_lon),
          sizeof(result->unexpected_tile_min_lon))) {
    result->unexpected_tile_min_lat = stats.unexpected_tile_min_lat;
    result->unexpected_tile_min_lon = stats.unexpected_tile_min_lon;
  }
}

PlugInSegmentSafetyDiagnosticReason SegmentSafetyUnavailableReason(
    const SegmentSafetyCoreStats& stats) {
  if (stats.no_chart_database) return PI_SEGMENT_SAFETY_DIAG_NO_CHART_DATABASE;
  if (stats.chart_load_failed) return PI_SEGMENT_SAFETY_DIAG_CHART_LOAD_FAILED;
  if (stats.candidate_chart_count == 0) {
    if (stats.raster_chart_count > 0 && stats.unsupported_chart_count == 0)
      return PI_SEGMENT_SAFETY_DIAG_RASTER_ONLY;
    if (stats.unsupported_chart_count > 0 && stats.raster_chart_count == 0)
      return PI_SEGMENT_SAFETY_DIAG_UNSUPPORTED_CHART_TYPE;
    return PI_SEGMENT_SAFETY_DIAG_NO_CANDIDATE_CHART;
  }
  if (stats.zero_land_geometry)
    return PI_SEGMENT_SAFETY_DIAG_NO_LANDARE_GEOMETRY;
  return PI_SEGMENT_SAFETY_DIAG_NO_CANDIDATE_CHART;
}

const char* SegmentSafetyUnavailableMessage(
    PlugInSegmentSafetyDiagnosticReason reason) {
  switch (reason) {
    case PI_SEGMENT_SAFETY_DIAG_NO_CHART_DATABASE:
      return "no chart database available for chart land checks";
    case PI_SEGMENT_SAFETY_DIAG_NO_CANDIDATE_CHART:
      return "no candidate vector chart found for segment";
    case PI_SEGMENT_SAFETY_DIAG_RASTER_ONLY:
      return "only raster chart coverage found for segment";
    case PI_SEGMENT_SAFETY_DIAG_UNSUPPORTED_CHART_TYPE:
      return "only unsupported chart types found for segment";
    case PI_SEGMENT_SAFETY_DIAG_CHART_LOAD_FAILED:
      return "candidate chart could not be loaded for segment safety";
    case PI_SEGMENT_SAFETY_DIAG_NO_LANDARE_GEOMETRY:
      return "candidate vector chart has no LNDARE land geometry";
    default:
      return "chart land geometry unavailable for segment";
  }
}

double SegmentSafetyDegToRad(double degrees) {
  return degrees * 3.14159265358979323846 / 180.0;
}

double SegmentSafetyCross(const wxPoint2DDouble& a, const wxPoint2DDouble& b,
                          const wxPoint2DDouble& c) {
  return (b.m_x - a.m_x) * (c.m_y - a.m_y) -
         (b.m_y - a.m_y) * (c.m_x - a.m_x);
}

bool SegmentSafetyBBoxIntersects(const SegmentSafetyBBox& a,
                                 const SegmentSafetyBBox& b) {
  return !(a.max_lat < b.min_lat || a.min_lat > b.max_lat ||
           a.max_lon < b.min_lon || a.min_lon > b.max_lon);
}

SegmentSafetyBBox SegmentSafetyRingBBox(
    const std::vector<wxPoint2DDouble>& points) {
  SegmentSafetyBBox box;
  box.min_lat = box.max_lat = points.empty() ? 0.0 : points[0].m_y;
  box.min_lon = box.max_lon = points.empty() ? 0.0 : points[0].m_x;
  for (size_t i = 1; i < points.size(); ++i) {
    box.min_lat = wxMin(box.min_lat, points[i].m_y);
    box.max_lat = wxMax(box.max_lat, points[i].m_y);
    box.min_lon = wxMin(box.min_lon, points[i].m_x);
    box.max_lon = wxMax(box.max_lon, points[i].m_x);
  }
  return box;
}

SegmentSafetyBBox SegmentSafetySegmentBBox(double lat1, double lon1,
                                           double lat2, double lon2,
                                           double margin_nm) {
  double margin_lat = margin_nm / 60.0;
  double mid_lat = (lat1 + lat2) / 2.0;
  double cos_lat = wxMax(0.1, fabs(cos(SegmentSafetyDegToRad(mid_lat))));
  double margin_lon = margin_nm / (60.0 * cos_lat);

  SegmentSafetyBBox box;
  box.min_lat = wxMin(lat1, lat2) - margin_lat;
  box.max_lat = wxMax(lat1, lat2) + margin_lat;
  box.min_lon = wxMin(lon1, lon2) - margin_lon;
  box.max_lon = wxMax(lon1, lon2) + margin_lon;
  return box;
}

bool SegmentSafetyPointInRing(double lat, double lon,
                              const std::vector<wxPoint2DDouble>& ring) {
  bool inside = false;
  size_t count = ring.size();
  if (count < 3) return false;

  for (size_t i = 0, j = count - 1; i < count; j = i++) {
    double xi = ring[i].m_x, yi = ring[i].m_y;
    double xj = ring[j].m_x, yj = ring[j].m_y;
    bool intersect = ((yi > lat) != (yj > lat)) &&
                     (lon < (xj - xi) * (lat - yi) / (yj - yi) + xi);
    if (intersect) inside = !inside;
  }
  return inside;
}

bool SegmentSafetyOnSegment(const wxPoint2DDouble& a,
                            const wxPoint2DDouble& b,
                            const wxPoint2DDouble& p) {
  const double eps = 1e-10;
  return fabs(SegmentSafetyCross(a, b, p)) < eps &&
         p.m_x >= wxMin(a.m_x, b.m_x) - eps &&
         p.m_x <= wxMax(a.m_x, b.m_x) + eps &&
         p.m_y >= wxMin(a.m_y, b.m_y) - eps &&
         p.m_y <= wxMax(a.m_y, b.m_y) + eps;
}

bool SegmentSafetySegmentsIntersect(const wxPoint2DDouble& a,
                                    const wxPoint2DDouble& b,
                                    const wxPoint2DDouble& c,
                                    const wxPoint2DDouble& d) {
  double c1 = SegmentSafetyCross(a, b, c);
  double c2 = SegmentSafetyCross(a, b, d);
  double c3 = SegmentSafetyCross(c, d, a);
  double c4 = SegmentSafetyCross(c, d, b);

  if (((c1 > 0 && c2 < 0) || (c1 < 0 && c2 > 0)) &&
      ((c3 > 0 && c4 < 0) || (c3 < 0 && c4 > 0)))
    return true;

  return SegmentSafetyOnSegment(a, b, c) || SegmentSafetyOnSegment(a, b, d) ||
         SegmentSafetyOnSegment(c, d, a) || SegmentSafetyOnSegment(c, d, b);
}

double SegmentSafetyPointSegmentDistanceNm(const wxPoint2DDouble& p,
                                           const wxPoint2DDouble& a,
                                           const wxPoint2DDouble& b,
                                           double mean_lat) {
  double cos_lat = wxMax(0.1, fabs(cos(SegmentSafetyDegToRad(mean_lat))));
  double px = p.m_x * 60.0 * cos_lat;
  double py = p.m_y * 60.0;
  double ax = a.m_x * 60.0 * cos_lat;
  double ay = a.m_y * 60.0;
  double bx = b.m_x * 60.0 * cos_lat;
  double by = b.m_y * 60.0;

  double dx = bx - ax;
  double dy = by - ay;
  double denom = dx * dx + dy * dy;
  double t = denom > 0.0 ? ((px - ax) * dx + (py - ay) * dy) / denom : 0.0;
  t = wxMax(0.0, wxMin(1.0, t));
  double cx = ax + t * dx;
  double cy = ay + t * dy;
  return sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
}

double SegmentSafetySegmentDistanceNm(const wxPoint2DDouble& a,
                                      const wxPoint2DDouble& b,
                                      const wxPoint2DDouble& c,
                                      const wxPoint2DDouble& d) {
  if (SegmentSafetySegmentsIntersect(a, b, c, d)) return 0.0;
  double mean_lat = (a.m_y + b.m_y + c.m_y + d.m_y) / 4.0;
  return wxMin(
      wxMin(SegmentSafetyPointSegmentDistanceNm(a, c, d, mean_lat),
            SegmentSafetyPointSegmentDistanceNm(b, c, d, mean_lat)),
      wxMin(SegmentSafetyPointSegmentDistanceNm(c, a, b, mean_lat),
            SegmentSafetyPointSegmentDistanceNm(d, a, b, mean_lat)));
}

int SegmentSafetyCurrentGroupIndex() {
  ChartCanvas* canvas =
      gFrame && gFrame->GetFocusCanvas() ? gFrame->GetFocusCanvas()
                                         : (gFrame ? gFrame->GetPrimaryCanvas()
                                                   : NULL);
  return canvas ? canvas->m_groupIndex : 0;
}

std::string SegmentSafetyPointCacheKey(double lat, double lon) {
  const double bucket_degrees = 0.00025;
  long lat_bucket = lround(lat / bucket_degrees);
  long lon_bucket = lround(lon / bucket_degrees);
  return wxString::Format("%d:%ld:%ld", SegmentSafetyCurrentGroupIndex(),
                          lat_bucket, lon_bucket)
      .ToStdString();
}

void StoreSegmentSafetyPointCache(
    const std::string& key, const CachedPointSafetyClassification& value) {
  if (s_segment_safety_point_cache.size() >=
      kMaxSegmentSafetyPointCacheEntries) {
    s_segment_safety_point_cache.clear();
  }
  s_segment_safety_point_cache[key] = value;
}

void CopySegmentSafetyPointCacheToResult(
    const CachedPointSafetyClassification& cached,
    PlugInSegmentSafetyResult* result) {
  if (!SegmentSafetyResultHas(result,
                              offsetof(PlugInSegmentSafetyResult, hit_object),
                              sizeof(result->hit_object)))
    return;

  result->chart_db_index = cached.chart_db_index;
  result->chart_scale = cached.chart_scale;
  strncpy(result->chart_path, cached.chart_path,
          sizeof(result->chart_path) - 1);
  result->chart_path[sizeof(result->chart_path) - 1] = '\0';
  strncpy(result->hit_object, cached.hit_object,
          sizeof(result->hit_object) - 1);
  result->hit_object[sizeof(result->hit_object) - 1] = '\0';
}

CachedPointSafetyClassification MakeSegmentSafetyPointCacheEntry(
    SegmentSafetyPointClass point_class, PlugInSegmentSafetySource source,
    int chart_db_index, int chart_scale, const char* chart_path,
    const char* hit_object) {
  CachedPointSafetyClassification cached;
  cached.point_class = point_class;
  cached.source = source;
  cached.chart_db_index = chart_db_index;
  cached.chart_scale = chart_scale;
  if (chart_path) {
    strncpy(cached.chart_path, chart_path, sizeof(cached.chart_path) - 1);
    cached.chart_path[sizeof(cached.chart_path) - 1] = '\0';
  }
  if (hit_object) {
    strncpy(cached.hit_object, hit_object, sizeof(cached.hit_object) - 1);
    cached.hit_object[sizeof(cached.hit_object) - 1] = '\0';
  }
  return cached;
}

std::string SegmentSafetyGridTileKeyForIndices(long lat_tile, long lon_tile) {
  return wxString::Format("%d:%ld:%ld:%.6f",
                          SegmentSafetyCurrentGroupIndex(), lat_tile, lon_tile,
                          kSegmentSafetyGridResolutionDegrees)
      .ToStdString();
}

std::string SegmentSafetyGridTileKey(double lat, double lon, long* lat_tile,
                                     long* lon_tile) {
  long lt = floor(lat / kSegmentSafetyGridTileDegrees);
  long ln = floor(lon / kSegmentSafetyGridTileDegrees);
  if (lat_tile) *lat_tile = lt;
  if (lon_tile) *lon_tile = ln;
  return SegmentSafetyGridTileKeyForIndices(lt, ln);
}

SegmentSafetyPointClass ChartPointSafetyClassAtRaw(
    double lat, double lon, PlugInSegmentSafetySource* source,
    SegmentSafetyCoreStats* stats, PlugInSegmentSafetyResult* result = NULL);
CachedPointSafetyGridTile BuildSegmentSafetyGridTile(
    double lat, double lon, long lat_tile, long lon_tile,
    SegmentSafetyCoreStats* stats);

void StoreSegmentSafetyGridTile(const std::string& key,
                                const CachedPointSafetyGridTile& tile) {
  if (s_segment_safety_grid_cache.size() >= kMaxSegmentSafetyGridTiles)
    s_segment_safety_grid_cache.clear();
  s_segment_safety_grid_cache[key] = tile;
}

bool EnsureSegmentSafetyGridTile(long lat_tile, long lon_tile,
                                 SegmentSafetyCoreStats* stats,
                                 bool* built = NULL) {
  if (built) *built = false;
  std::string key = SegmentSafetyGridTileKeyForIndices(lat_tile, lon_tile);
  if (s_segment_safety_grid_cache.find(key) !=
      s_segment_safety_grid_cache.end()) {
    if (stats) ++stats->grid_cache_hits;
    return true;
  }

  if (stats) ++stats->grid_cache_misses;
  CachedPointSafetyGridTile tile = BuildSegmentSafetyGridTile(
      lat_tile * kSegmentSafetyGridTileDegrees,
      lon_tile * kSegmentSafetyGridTileDegrees, lat_tile, lon_tile, stats);
  StoreSegmentSafetyGridTile(key, tile);
  if (built) *built = true;
  return s_segment_safety_grid_cache.find(key) !=
         s_segment_safety_grid_cache.end();
}

void RecordUnexpectedSegmentSafetyTileBuild(SegmentSafetyCoreStats* stats,
                                            long lat_tile, long lon_tile) {
  if (!stats) return;
  ++stats->unexpected_tile_builds;
  if (stats->unexpected_tile_builds == 1) {
    stats->unexpected_lat_tile = lat_tile;
    stats->unexpected_lon_tile = lon_tile;
    stats->unexpected_tile_min_lat =
        lat_tile * kSegmentSafetyGridTileDegrees;
    stats->unexpected_tile_min_lon =
        lon_tile * kSegmentSafetyGridTileDegrees;
  }
}

void SegmentSafetyCandidateChartsAt(double lat, double lon,
                                    std::set<int>& chart_indexes,
                                    SegmentSafetyCoreStats* stats) {
  if (!ChartData) {
    if (stats) stats->no_chart_database = true;
    return;
  }

  ChartStack stack;
  ChartData->BuildChartStack(&stack, lat, lon, SegmentSafetyCurrentGroupIndex());
  if (stats) stats->chart_stack_entries += stack.nEntry;
  for (int i = 0; i < stack.nEntry; ++i) {
    int db_index = stack.GetDBIndex(i);
    if (db_index < 0) continue;
    ChartFamilyEnum family =
        (ChartFamilyEnum)ChartData->GetCSChartFamily(&stack, i);
    ChartTypeEnum type = (ChartTypeEnum)ChartData->GetCSChartType(&stack, i);
    if (family == CHART_FAMILY_VECTOR ||
        type == CHART_TYPE_CM93 || type == CHART_TYPE_CM93COMP) {
      chart_indexes.insert(db_index);
    } else if (family == CHART_FAMILY_RASTER) {
      if (stats) ++stats->raster_chart_count;
    } else {
      if (stats) ++stats->unsupported_chart_count;
    }
  }
}

SegmentSafetyPointClass ChartPointSafetyClassAtRaw(
    double lat, double lon, PlugInSegmentSafetySource* source,
    SegmentSafetyCoreStats* stats, PlugInSegmentSafetyResult* result) {
  std::string point_cache_key = SegmentSafetyPointCacheKey(lat, lon);
  std::map<std::string, CachedPointSafetyClassification>::const_iterator
      cache_it = s_segment_safety_point_cache.find(point_cache_key);
  if (cache_it != s_segment_safety_point_cache.end()) {
    if (stats) ++stats->point_cache_hits;
    if (source) *source = cache_it->second.source;
    CopySegmentSafetyPointCacheToResult(cache_it->second, result);
    return cache_it->second.point_class;
  }
  if (stats) ++stats->point_cache_misses;

  std::set<int> chart_indexes;
  SegmentSafetyCandidateChartsAt(lat, lon, chart_indexes, stats);

  bool chart_checked = false;
  for (std::set<int>::const_iterator it = chart_indexes.begin();
       it != chart_indexes.end(); ++it) {
    ChartBase* chart = ChartData ? ChartData->OpenChartFromDB(*it, FULL_INIT)
                                 : NULL;
    s57chart* s57 = dynamic_cast<s57chart*>(chart);
    if (!s57) continue;

    chart_checked = true;
    bool cm93 = IsCm93Chart(chart);
    PlugInSegmentSafetySource chart_source =
        cm93 ? PI_SEGMENT_SAFETY_SOURCE_CM93
             : PI_SEGMENT_SAFETY_SOURCE_VECTOR_CHART;
    if (source)
      *source = chart_source;
    if (stats) ++stats->s57_chart_count;

    ViewPort vp = SegmentSafetyViewPortAt(lat, lon);
    if (cm93) {
      cm93compchart* cm93_chart = dynamic_cast<cm93compchart*>(chart);
      if (cm93_chart) cm93_chart->SetVPParms(vp);
    }

    ListOfObjRazRules* rule_list =
        s57->GetObjRuleListAtLatLon(lat, lon, 0.0, &vp, MASK_AREA);
    if (!rule_list) continue;

    bool drying = false;
    for (ListOfObjRazRules::Node* node = rule_list->GetFirst(); node;
         node = node->GetNext()) {
      ObjRazRules* rule = node->GetData();
      if (!rule || !rule->obj) continue;
      if (!strncmp(rule->obj->FeatureName, "LNDARE", 6)) {
        wxString chart_path = chart->GetFullPath();
        wxString object = SegmentSafetyRuleSummary(rule);
        if (SegmentSafetyResultHas(
                result, offsetof(PlugInSegmentSafetyResult, hit_object),
                sizeof(result->hit_object))) {
          result->chart_db_index = *it;
          result->chart_scale = chart->GetNativeScale();
          strncpy(result->chart_path, chart_path.mb_str(),
                  sizeof(result->chart_path) - 1);
          result->chart_path[sizeof(result->chart_path) - 1] = '\0';
          strncpy(result->hit_object, object.mb_str(),
                  sizeof(result->hit_object) - 1);
          result->hit_object[sizeof(result->hit_object) - 1] = '\0';
        }
        StoreSegmentSafetyPointCache(
            point_cache_key,
            MakeSegmentSafetyPointCacheEntry(
                SEGMENT_SAFETY_POINT_LAND, chart_source, *it,
                chart->GetNativeScale(), chart_path.mb_str(), object.mb_str()));
        rule_list->Clear();
        delete rule_list;
        return SEGMENT_SAFETY_POINT_LAND;
      }
      if (!strncmp(rule->obj->FeatureName, "DRGARE", 6)) drying = true;
    }

    rule_list->Clear();
    delete rule_list;
    SegmentSafetyPointClass point_class =
        drying ? SEGMENT_SAFETY_POINT_DRYING : SEGMENT_SAFETY_POINT_WATER;
    wxString chart_path = chart->GetFullPath();
    StoreSegmentSafetyPointCache(
        point_cache_key,
        MakeSegmentSafetyPointCacheEntry(point_class, chart_source, *it,
                                         chart->GetNativeScale(),
                                         chart_path.mb_str(), ""));
    return point_class;
  }

  SegmentSafetyPointClass point_class =
      chart_checked ? SEGMENT_SAFETY_POINT_WATER : SEGMENT_SAFETY_POINT_NO_DATA;
  StoreSegmentSafetyPointCache(
      point_cache_key,
      MakeSegmentSafetyPointCacheEntry(
          point_class, chart_checked ? PI_SEGMENT_SAFETY_SOURCE_VECTOR_CHART
                                     : PI_SEGMENT_SAFETY_SOURCE_NONE,
          -1, -1, "", ""));
  return point_class;
}

CachedPointSafetyGridTile BuildSegmentSafetyGridTile(double lat, double lon,
                                                     long lat_tile,
                                                     long lon_tile,
                                                     SegmentSafetyCoreStats* stats) {
  wxStopWatch timer;
  CachedPointSafetyGridTile tile;
  tile.group_index = SegmentSafetyCurrentGroupIndex();
  tile.lat_tile = lat_tile;
  tile.lon_tile = lon_tile;
  tile.min_lat = lat_tile * kSegmentSafetyGridTileDegrees;
  tile.min_lon = lon_tile * kSegmentSafetyGridTileDegrees;
  tile.resolution = kSegmentSafetyGridResolutionDegrees;
  tile.rows = (int)ceil(kSegmentSafetyGridTileDegrees / tile.resolution) + 1;
  tile.cols = tile.rows;
  tile.built = true;
  tile.classes.assign(tile.rows * tile.cols,
                      (unsigned char)SEGMENT_SAFETY_POINT_NO_DATA);

  int land = 0, water = 0, drying = 0, unknown = 0;
  for (int r = 0; r < tile.rows; ++r) {
    double cell_lat = tile.min_lat + r * tile.resolution;
    for (int c = 0; c < tile.cols; ++c) {
      double cell_lon = tile.min_lon + c * tile.resolution;
      PlugInSegmentSafetySource source = PI_SEGMENT_SAFETY_SOURCE_NONE;
      PlugInSegmentSafetyResult cell_result = {};
      cell_result.struct_size = sizeof(cell_result);
      InitSegmentSafetyResult(&cell_result);
      SegmentSafetyPointClass point_class =
          ChartPointSafetyClassAtRaw(cell_lat, cell_lon, &source, stats,
                                     &cell_result);
      tile.classes[r * tile.cols + c] = (unsigned char)point_class;
      if (tile.source == PI_SEGMENT_SAFETY_SOURCE_NONE &&
          source != PI_SEGMENT_SAFETY_SOURCE_NONE)
        tile.source = source;
      if (tile.chart_db_index < 0 && cell_result.chart_db_index >= 0) {
        tile.chart_db_index = cell_result.chart_db_index;
        tile.chart_scale = cell_result.chart_scale;
        snprintf(tile.chart_path, sizeof(tile.chart_path), "%s",
                 cell_result.chart_path);
      }
      switch (point_class) {
        case SEGMENT_SAFETY_POINT_LAND:
          ++land;
          break;
        case SEGMENT_SAFETY_POINT_WATER:
          ++water;
          break;
        case SEGMENT_SAFETY_POINT_DRYING:
          ++drying;
          break;
        default:
          ++unknown;
          break;
      }
    }
  }

  int build_ms = timer.Time();
  tile.land_count = land;
  tile.water_count = water;
  tile.drying_count = drying;
  tile.unknown_count = unknown;
  if (stats) {
    stats->grid_build_ms += build_ms;
    stats->grid_cells_total += tile.rows * tile.cols;
    stats->grid_cells_land += land;
    stats->grid_cells_water += water;
    stats->grid_cells_drying += drying;
    stats->grid_cells_unknown += unknown;
  }

  wxLogMessage(
      "SEGMENT_SAFETY_GRID built key=%ld:%ld group=%d bbox=[lat %.6f..%.6f "
      "lon %.6f..%.6f] resolution_deg=%.6f cells=%d land=%d water=%d "
      "drying=%d unknown=%d build_ms=%d source=%d chart_db_index=%d "
      "chart_scale=%d chart_path=\"%s\"",
      lat_tile, lon_tile, tile.group_index, tile.min_lat,
      tile.min_lat + kSegmentSafetyGridTileDegrees, tile.min_lon,
      tile.min_lon + kSegmentSafetyGridTileDegrees, tile.resolution,
      tile.rows * tile.cols, land, water, drying, unknown,
      build_ms, tile.source, tile.chart_db_index, tile.chart_scale,
      tile.chart_path);

  return tile;
}

bool SegmentSafetyAllTouchedTilesAreWater(double lat1, double lon1,
                                          double lat2, double lon2,
                                          double safety_margin_nm,
                                          double bearing, double dist_nm,
                                          int samples,
                                          SegmentSafetyCoreStats* stats) {
  std::set<std::pair<long, long> > tiles;
  for (int i = 0; i < samples; ++i) {
    double sample_dist = samples == 1 ? 0.0 : dist_nm * i / (samples - 1);
    double lat = lat1;
    double lon = lon1;
    if (sample_dist > 0.0)
      ll_gc_ll(lat1, lon1, bearing, sample_dist, &lat, &lon);

    long lat_tile = 0;
    long lon_tile = 0;
    SegmentSafetyGridTileKey(lat, lon, &lat_tile, &lon_tile);
    tiles.insert(std::make_pair(lat_tile, lon_tile));

    if (safety_margin_nm > 0.0) {
      double left_lat, left_lon, right_lat, right_lon;
      ll_gc_ll(lat, lon, SegmentSafetyNormalizeBearing(bearing - 90.0),
               safety_margin_nm, &left_lat, &left_lon);
      ll_gc_ll(lat, lon, SegmentSafetyNormalizeBearing(bearing + 90.0),
               safety_margin_nm, &right_lat, &right_lon);
      SegmentSafetyGridTileKey(left_lat, left_lon, &lat_tile, &lon_tile);
      tiles.insert(std::make_pair(lat_tile, lon_tile));
      SegmentSafetyGridTileKey(right_lat, right_lon, &lat_tile, &lon_tile);
      tiles.insert(std::make_pair(lat_tile, lon_tile));
    }
  }

  if (tiles.empty()) return false;

  for (std::set<std::pair<long, long> >::const_iterator it = tiles.begin();
       it != tiles.end(); ++it) {
    std::string key = SegmentSafetyGridTileKeyForIndices(it->first, it->second);
    std::map<std::string, CachedPointSafetyGridTile>::const_iterator tile_it =
        s_segment_safety_grid_cache.find(key);
    if (tile_it == s_segment_safety_grid_cache.end()) {
      bool built = false;
      if (!EnsureSegmentSafetyGridTile(it->first, it->second, stats, &built))
        return false;
      if (built)
        RecordUnexpectedSegmentSafetyTileBuild(stats, it->first, it->second);
      tile_it = s_segment_safety_grid_cache.find(key);
      if (tile_it == s_segment_safety_grid_cache.end()) return false;
    } else if (stats) {
      ++stats->grid_cache_hits;
    }

    const CachedPointSafetyGridTile& tile = tile_it->second;
    if (tile.classes.empty() || tile.unknown_count > 0 ||
        tile.land_count > 0 || tile.drying_count > 0)
      return false;
  }

  if (stats) ++stats->water_tile_shortcuts;
  return true;
}

SegmentSafetyPointClass ChartPointSafetyClassAt(
    double lat, double lon, PlugInSegmentSafetySource* source,
    SegmentSafetyCoreStats* stats, PlugInSegmentSafetyResult* result = NULL) {
  long lat_tile = 0;
  long lon_tile = 0;
  std::string key = SegmentSafetyGridTileKey(lat, lon, &lat_tile, &lon_tile);
  std::map<std::string, CachedPointSafetyGridTile>::const_iterator it =
      s_segment_safety_grid_cache.find(key);
  if (it == s_segment_safety_grid_cache.end()) {
    bool built = false;
    EnsureSegmentSafetyGridTile(lat_tile, lon_tile, stats, &built);
    if (built)
      RecordUnexpectedSegmentSafetyTileBuild(stats, lat_tile, lon_tile);
    it = s_segment_safety_grid_cache.find(key);
  } else if (stats) {
    ++stats->grid_cache_hits;
  }

  if (it == s_segment_safety_grid_cache.end())
    return ChartPointSafetyClassAtRaw(lat, lon, source, stats, result);

  const CachedPointSafetyGridTile& tile = it->second;
  if (stats) ++stats->grid_lookups;
  int row = (int)lround((lat - tile.min_lat) / tile.resolution);
  int col = (int)lround((lon - tile.min_lon) / tile.resolution);
  if (row < 0 || row >= tile.rows || col < 0 || col >= tile.cols)
    return ChartPointSafetyClassAtRaw(lat, lon, source, stats, result);

  SegmentSafetyPointClass point_class =
      (SegmentSafetyPointClass)tile.classes[row * tile.cols + col];
  if (source) *source = tile.source;
  if (SegmentSafetyResultHas(result, offsetof(PlugInSegmentSafetyResult,
                                              hit_object),
                             sizeof(result->hit_object))) {
    result->chart_db_index = tile.chart_db_index;
    result->chart_scale = tile.chart_scale;
    strncpy(result->chart_path, tile.chart_path,
            sizeof(result->chart_path) - 1);
    result->chart_path[sizeof(result->chart_path) - 1] = '\0';
    if (point_class == SEGMENT_SAFETY_POINT_LAND)
      strncpy(result->hit_object, "grid LAND cell",
              sizeof(result->hit_object) - 1);
  }
  return point_class;
}

wxString SegmentSafetyPointDiagnostic(double lat, double lon) {
  SegmentSafetyCoreStats stats;
  std::set<int> chart_indexes;
  SegmentSafetyCandidateChartsAt(lat, lon, chart_indexes, &stats);

  wxString objects;
  wxString source_name = "none";
  wxString chart_path;
  wxString point_class = "UNKNOWN";
  int chart_db_index = -1;
  int chart_scale = -1;
  int area_count = 0;
  int land_count = 0;
  int drying_count = 0;
  int depare_count = 0;
  bool chart_checked = false;

  for (std::set<int>::const_iterator it = chart_indexes.begin();
       it != chart_indexes.end(); ++it) {
    ChartBase* chart = ChartData ? ChartData->OpenChartFromDB(*it, FULL_INIT)
                                 : NULL;
    s57chart* s57 = dynamic_cast<s57chart*>(chart);
    if (!s57) continue;

    chart_checked = true;
    chart_db_index = *it;
    chart_path = chart->GetFullPath();
    chart_scale = chart->GetNativeScale();
    bool cm93 = IsCm93Chart(chart);
    source_name = cm93 ? "CM93" : "VECTOR_CHART";
    ViewPort vp = SegmentSafetyViewPortAt(lat, lon);
    if (cm93) {
      cm93compchart* cm93_chart = dynamic_cast<cm93compchart*>(chart);
      if (cm93_chart) cm93_chart->SetVPParms(vp);
    }

    ListOfObjRazRules* rule_list =
        s57->GetObjRuleListAtLatLon(lat, lon, 0.0, &vp, MASK_AREA);
    if (!rule_list) break;

    for (ListOfObjRazRules::Node* node = rule_list->GetFirst(); node;
         node = node->GetNext()) {
      ObjRazRules* rule = node->GetData();
      if (!rule || !rule->obj) continue;
      ++area_count;
      if (!objects.empty()) objects += ";";
      objects += SegmentSafetyRuleSummary(rule);

      if (!strncmp(rule->obj->FeatureName, "LNDARE", 6)) ++land_count;
      if (!strncmp(rule->obj->FeatureName, "DRGARE", 6)) ++drying_count;
      if (!strncmp(rule->obj->FeatureName, "DEPARE", 6)) ++depare_count;
    }

    rule_list->Clear();
    delete rule_list;
    break;
  }

  if (!chart_checked)
    point_class = "NO_DATA";
  else if (land_count > 0)
    point_class = "LAND";
  else if (drying_count > 0)
    point_class = "DRYING";
  else
    point_class = "WATER_OR_NO_UNSAFE_AREA";

  if (objects.empty()) objects = "none";
  chart_path.Replace("\"", "'");
  return wxString::Format(
      "class=%s source=%s chart_db_index=%d chart_scale=%d chart_path=\"%s\" "
      "chart_stack_entries=%d candidate_charts=%zu area_objects=%d "
      "land_objects=%d drying_objects=%d depare_objects=%d objects=\"%s\"",
      point_class, source_name, chart_db_index, chart_scale, chart_path,
      stats.chart_stack_entries, chart_indexes.size(), area_count, land_count,
      drying_count, depare_count, objects);
}

CachedChartLandGeometry& SegmentSafetyLoadChartLandGeometry(
    int db_index, double lat, double lon, SegmentSafetyCoreStats* stats) {
  if (!ChartData) {
    if (stats) stats->no_chart_database = true;
    static CachedChartLandGeometry no_chart_data;
    return no_chart_data;
  }
  wxStopWatch cache_timer;
  ChartBase* chart = ChartData->OpenChartFromDB(db_index, FULL_INIT);
  bool cm93 = IsCm93Chart(chart);
  std::string cache_key = SegmentSafetyCacheKey(db_index, cm93, lat, lon);
  CachedChartLandGeometry& cached = s_segment_safety_land_cache[cache_key];
  if (cached.loaded) return cached;
  cached.loaded = true;
  cached.cache_key = cache_key;
  if (chart) cached.chart_path = chart->GetFullPath();

  s57chart* s57 = dynamic_cast<s57chart*>(chart);
  if (!s57) {
    if (stats) {
      stats->chart_load_failed = chart == NULL;
      stats->cache_build_ms += cache_timer.Time();
    }
    return cached;
  }

  cached.source = cm93 ? PI_SEGMENT_SAFETY_SOURCE_CM93
                       : PI_SEGMENT_SAFETY_SOURCE_VECTOR_CHART;
  if (stats) ++stats->s57_chart_count;

  if (cm93) {
    cm93compchart* cm93_chart = dynamic_cast<cm93compchart*>(chart);
    if (cm93_chart) cm93_chart->SetVPParms(SegmentSafetyViewPortAt(lat, lon));
  }

  std::vector<std::vector<wxPoint2DDouble> > rings;
  s57->CollectFeatureAreaRings("LNDARE", rings);
  for (size_t i = 0; i < rings.size(); ++i) {
    if (rings[i].size() < 3) continue;
    CachedLandRing ring;
    ring.points.swap(rings[i]);
    ring.bbox = SegmentSafetyRingBBox(ring.points);
    if (ring.bbox.max_lat < lat - 10.0 || ring.bbox.min_lat > lat + 10.0 ||
        ring.bbox.max_lon < lon - 10.0 || ring.bbox.min_lon > lon + 10.0)
      continue;
    cached.rings.push_back(ring);
  }

  wxLogMessage(
      "OpenCPN segment safety: cached chart land geometry "
      "db_index=%d key=%s source=%d type=%d scale=%d land_rings=%zu",
      db_index, cache_key.c_str(), (int)cached.source,
      chart ? (int)chart->GetChartType() : -1,
      chart ? chart->GetNativeScale() : -1, cached.rings.size());
  for (size_t i = 0; i < cached.rings.size() && i < 8; ++i) {
    const CachedLandRing& ring = cached.rings[i];
    wxLogMessage(
        "OpenCPN segment safety: land ring sample db_index=%d key=%s "
        "ring=%zu bbox=[lat %.8f..%.8f lon %.8f..%.8f] points=%zu",
        db_index, cache_key.c_str(), i, ring.bbox.min_lat,
        ring.bbox.max_lat, ring.bbox.min_lon, ring.bbox.max_lon,
        ring.points.size());
  }
  if (cached.rings.empty()) {
    wxString chart_path = chart ? chart->GetFullPath() : wxString();
    wxLogMessage(
        "OpenCPN segment safety: no LNDARE rings db_index=%d key=%s "
        "path=%s summary=%s",
        db_index, cache_key.c_str(), chart_path.c_str(),
        s57->GetFeatureDebugSummary());
  }
  if (stats) {
    stats->cache_build_ms += cache_timer.Time();
    if (cached.rings.empty()) stats->zero_land_geometry = true;
  }

  return cached;
}

void LogSegmentSafetyChartHit(const char* cause, int db_index,
                              const std::string& cache_key,
                              PlugInSegmentSafetySource source,
                              const CachedLandRing& ring,
                              double lat1, double lon1, double lat2,
                              double lon2, double safety_margin_nm,
                              size_t edge_index) {
  if (s_segment_safety_chart_hit_logs >= kMaxSegmentSafetyChartHitLogs) return;
  ++s_segment_safety_chart_hit_logs;
  wxLogMessage(
      "OpenCPN segment safety: LNDARE hit #%ld cause=%s db_index=%d key=%s "
      "source=%d segment=(%.8f,%.8f)->(%.8f,%.8f) margin_nm=%.3f "
      "ring_bbox=[lat %.8f..%.8f lon %.8f..%.8f] ring_points=%zu "
      "edge_index=%zu",
      s_segment_safety_chart_hit_logs, cause, db_index, cache_key.c_str(),
      (int)source, lat1, lon1, lat2, lon2, safety_margin_nm, ring.bbox.min_lat,
      ring.bbox.max_lat, ring.bbox.min_lon, ring.bbox.max_lon,
      ring.points.size(), edge_index);
}

void SetSegmentSafetyChartHitDetails(PlugInSegmentSafetyResult* result,
                                     PlugInSegmentSafetyHitCause cause,
                                     int db_index,
                                     const CachedChartLandGeometry& chart_cache,
                                     const CachedLandRing& ring,
                                     size_t edge_index) {
  if (!SegmentSafetyResultHas(
          result, offsetof(PlugInSegmentSafetyResult, chart_path),
          sizeof(result->chart_path)))
    return;

  result->chart_db_index = db_index;
  result->hit_cause = cause;
  result->hit_ring_min_lat = ring.bbox.min_lat;
  result->hit_ring_max_lat = ring.bbox.max_lat;
  result->hit_ring_min_lon = ring.bbox.min_lon;
  result->hit_ring_max_lon = ring.bbox.max_lon;
  result->hit_ring_point_count = (int)ring.points.size();
  result->hit_edge_index = (int)edge_index;
  strncpy(result->chart_path, chart_cache.chart_path.mb_str(),
          sizeof(result->chart_path) - 1);
  result->chart_path[sizeof(result->chart_path) - 1] = '\0';
}

bool CachedChartSegmentSafetyCheck(double lat1, double lon1, double lat2,
                                   double lon2, double safety_margin_nm,
                                   PlugInSegmentSafetyResult* result,
                                   bool* chart_data_available,
                                   SegmentSafetyCoreStats* stats) {
  wxStopWatch select_timer;
  std::set<int> chart_indexes;
  SegmentSafetyCandidateChartsAt(lat1, lon1, chart_indexes, stats);
  SegmentSafetyCandidateChartsAt(lat2, lon2, chart_indexes, stats);
  SegmentSafetyCandidateChartsAt((lat1 + lat2) / 2.0, (lon1 + lon2) / 2.0,
                                 chart_indexes, stats);
  if (stats) {
    stats->candidate_chart_count = chart_indexes.size();
    stats->chart_select_ms += select_timer.Time();
  }
  if (chart_indexes.empty()) return false;

  wxStopWatch geometry_timer;
  wxPoint2DDouble start(lon1, lat1);
  wxPoint2DDouble end(lon2, lat2);
  SegmentSafetyBBox segment_box =
      SegmentSafetySegmentBBox(lat1, lon1, lat2, lon2, safety_margin_nm);

  for (std::set<int>::const_iterator it = chart_indexes.begin();
       it != chart_indexes.end(); ++it) {
    CachedChartLandGeometry& chart_cache =
        SegmentSafetyLoadChartLandGeometry(*it, (lat1 + lat2) / 2.0,
                                           (lon1 + lon2) / 2.0, stats);
    if (chart_cache.source == PI_SEGMENT_SAFETY_SOURCE_NONE) continue;
    if (chart_cache.rings.empty()) {
      if (stats) stats->zero_land_geometry = true;
      continue;
    }
    if (chart_data_available) *chart_data_available = true;
    if (GetSegmentSafetySource(result) == PI_SEGMENT_SAFETY_SOURCE_NONE)
      SetSegmentSafetySource(result, chart_cache.source);
    if (stats) stats->land_ring_count += chart_cache.rings.size();

    for (size_t i = 0; i < chart_cache.rings.size(); ++i) {
      const CachedLandRing& ring = chart_cache.rings[i];
      if (!SegmentSafetyBBoxIntersects(segment_box, ring.bbox)) continue;
      if (stats) ++stats->bbox_ring_tests;

      if (SegmentSafetyPointInRing(lat1, lon1, ring.points) ||
          SegmentSafetyPointInRing(lat2, lon2, ring.points)) {
        if (stats) stats->geometry_check_ms += geometry_timer.Time();
        LogSegmentSafetyChartHit("endpoint-inside-LNDARE", *it,
                                 chart_cache.cache_key, chart_cache.source,
                                 ring, lat1, lon1, lat2, lon2,
                                 safety_margin_nm, 0);
        SetSegmentSafetyChartHitDetails(
            result, PI_SEGMENT_SAFETY_HIT_ENDPOINT_IN_LANDARE, *it,
            chart_cache, ring, 0);
        SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_CROSSES_LAND);
        SetSegmentSafetySource(result, chart_cache.source);
        SetSegmentSafetyMessage(result,
                                "segment endpoint is inside chart land area");
        return true;
      }

      for (size_t j = 0; j < ring.points.size(); ++j) {
        const wxPoint2DDouble& a = ring.points[j];
        const wxPoint2DDouble& b = ring.points[(j + 1) % ring.points.size()];
        if (stats) ++stats->edge_tests;
        if (SegmentSafetySegmentsIntersect(start, end, a, b)) {
          if (stats) stats->geometry_check_ms += geometry_timer.Time();
          LogSegmentSafetyChartHit("segment-intersects-LNDARE-edge", *it,
                                   chart_cache.cache_key, chart_cache.source,
                                   ring, lat1, lon1, lat2, lon2,
                                   safety_margin_nm, j);
          SetSegmentSafetyChartHitDetails(
              result, PI_SEGMENT_SAFETY_HIT_SEGMENT_INTERSECTS_LANDARE_EDGE,
              *it, chart_cache, ring, j);
          SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_CROSSES_LAND);
          SetSegmentSafetySource(result, chart_cache.source);
          SetSegmentSafetyMessage(result,
                                  "segment intersects chart land boundary");
          return true;
        }

        if (safety_margin_nm > 0.0 &&
            SegmentSafetySegmentDistanceNm(start, end, a, b) <=
                safety_margin_nm) {
          if (stats) stats->geometry_check_ms += geometry_timer.Time();
          LogSegmentSafetyChartHit("approx-margin-to-LNDARE-edge", *it,
                                   chart_cache.cache_key, chart_cache.source,
                                   ring, lat1, lon1, lat2, lon2,
                                   safety_margin_nm, j);
          SetSegmentSafetyChartHitDetails(
              result, PI_SEGMENT_SAFETY_HIT_MARGIN_TO_LANDARE_EDGE, *it,
              chart_cache, ring, j);
          SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_WITHIN_LAND_MARGIN);
          SetSegmentSafetySource(result, chart_cache.source);
          SetSegmentSafetyMessage(
              result,
              "segment is within approximate chart land safety margin");
          return true;
        }
      }
    }
  }

  if (stats) stats->geometry_check_ms += geometry_timer.Time();
  return false;
}

bool ChartSegmentPointClassificationCheck(double lat1, double lon1,
                                          double lat2, double lon2,
                                          double safety_margin_nm,
                                          PlugInSegmentSafetyResult* result,
                                          bool* chart_data_available,
                                          SegmentSafetyCoreStats* stats) {
  wxStopWatch geometry_timer;
  double bearing = 0.0;
  double dist_nm = 0.0;
  ll_gc_ll_reverse(lat1, lon1, lat2, lon2, &bearing, &dist_nm);

  const int max_samples = 512;
  int samples = wxMax(2, wxMin(max_samples, (int)ceil(dist_nm / 0.05) + 1));
  bool any_chart_data = false;
  PlugInSegmentSafetySource first_source = PI_SEGMENT_SAFETY_SOURCE_NONE;

  if (stats) stats->segment_sample_count += samples;

  wxStopWatch grid_lookup_timer;
  if (SegmentSafetyAllTouchedTilesAreWater(lat1, lon1, lat2, lon2,
                                           safety_margin_nm, bearing, dist_nm,
                                           samples, stats)) {
    if (stats) {
      stats->grid_lookup_ms += grid_lookup_timer.Time();
      stats->geometry_check_ms += geometry_timer.Time();
    }
    if (chart_data_available) *chart_data_available = true;
    if (GetSegmentSafetySource(result) == PI_SEGMENT_SAFETY_SOURCE_NONE)
      SetSegmentSafetySource(result, PI_SEGMENT_SAFETY_SOURCE_VECTOR_CHART);
    return false;
  }

  for (int i = 0; i < samples; ++i) {
    double sample_dist = samples == 1 ? 0.0 : dist_nm * i / (samples - 1);
    double lat = lat1;
    double lon = lon1;
    if (sample_dist > 0.0)
      ll_gc_ll(lat1, lon1, bearing, sample_dist, &lat, &lon);

    PlugInSegmentSafetySource source = PI_SEGMENT_SAFETY_SOURCE_NONE;
    wxStopWatch lookup_timer;
    SegmentSafetyPointClass point_class =
        ChartPointSafetyClassAt(lat, lon, &source, stats, result);
    if (stats) stats->grid_lookup_ms += lookup_timer.Time();
    if (point_class == SEGMENT_SAFETY_POINT_NO_DATA) continue;

    any_chart_data = true;
    if (first_source == PI_SEGMENT_SAFETY_SOURCE_NONE) first_source = source;
    if (GetSegmentSafetySource(result) == PI_SEGMENT_SAFETY_SOURCE_NONE)
      SetSegmentSafetySource(result, source);

    if (point_class == SEGMENT_SAFETY_POINT_LAND) {
      if (stats) stats->geometry_check_ms += geometry_timer.Time();
      if (chart_data_available) *chart_data_available = true;
      if (SegmentSafetyResultHas(
              result, offsetof(PlugInSegmentSafetyResult, hit_object),
              sizeof(result->hit_object))) {
        result->hit_sample_lat = lat;
        result->hit_sample_lon = lon;
        result->hit_sample_index = i;
        result->hit_sample_count = samples;
      }
      SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_CROSSES_LAND);
      SetSegmentSafetySource(result, source);
      SetSegmentSafetyMessage(
          result, "segment samples intersect chart land area");
      if (s_segment_safety_chart_hit_logs < kMaxSegmentSafetyChartHitLogs) {
        ++s_segment_safety_chart_hit_logs;
        wxLogMessage(
            "FIRST_LAND_HIT source=chart-point segment=(%.8f,%.8f)->"
            "(%.8f,%.8f) sample=(%.8f,%.8f) sample_index=%d/%d "
            "object=\"%s\" chart_db_index=%d chart_scale=%d "
            "chart_path=\"%s\"",
            lat1, lon1, lat2, lon2, lat, lon, i + 1, samples,
            result ? result->hit_object : "", result ? result->chart_db_index : -1,
            result ? result->chart_scale : -1,
            result ? result->chart_path : "");
      }
      return true;
    }

    if (safety_margin_nm > 0.0) {
      double left_lat, left_lon, right_lat, right_lon;
      ll_gc_ll(lat, lon, SegmentSafetyNormalizeBearing(bearing - 90.0),
               safety_margin_nm, &left_lat, &left_lon);
      ll_gc_ll(lat, lon, SegmentSafetyNormalizeBearing(bearing + 90.0),
               safety_margin_nm, &right_lat, &right_lon);
      PlugInSegmentSafetySource left_source = PI_SEGMENT_SAFETY_SOURCE_NONE;
      PlugInSegmentSafetySource right_source = PI_SEGMENT_SAFETY_SOURCE_NONE;
      wxStopWatch margin_lookup_timer;
      SegmentSafetyPointClass left_class =
          ChartPointSafetyClassAt(left_lat, left_lon, &left_source, stats);
      SegmentSafetyPointClass right_class =
          ChartPointSafetyClassAt(right_lat, right_lon, &right_source, stats);
      if (stats) stats->grid_lookup_ms += margin_lookup_timer.Time();
      if (left_class != SEGMENT_SAFETY_POINT_NO_DATA ||
          right_class != SEGMENT_SAFETY_POINT_NO_DATA)
        any_chart_data = true;
      if (left_class == SEGMENT_SAFETY_POINT_LAND ||
          right_class == SEGMENT_SAFETY_POINT_LAND) {
        if (stats) stats->geometry_check_ms += geometry_timer.Time();
        if (chart_data_available) *chart_data_available = true;
        SetSegmentSafetyStatus(result,
                               PI_SEGMENT_SAFETY_WITHIN_LAND_MARGIN);
        SetSegmentSafetySource(
            result, left_class == SEGMENT_SAFETY_POINT_LAND ? left_source
                                                            : right_source);
        SetSegmentSafetyMessage(
            result,
            "segment samples are within approximate chart land margin");
        return true;
      }
    }
  }

  if (any_chart_data) {
    if (chart_data_available) *chart_data_available = true;
    if (GetSegmentSafetySource(result) == PI_SEGMENT_SAFETY_SOURCE_NONE)
      SetSegmentSafetySource(result, first_source);
  }
  if (stats) stats->geometry_check_ms += geometry_timer.Time();
  return false;
}

bool GshhsSegmentSafetyHitsLand(double lat1, double lon1, double lat2,
                                double lon2, double safety_margin_nm,
                                PlugInSegmentSafetyStatus* status) {
  if (PlugIn_GSHHS_CrossesLand(lat1, lon1, lat2, lon2)) {
    if (status) *status = PI_SEGMENT_SAFETY_CROSSES_LAND;
    return true;
  }

  if (safety_margin_nm <= 0.0) return false;

  double bearing = 0.0;
  double dist_nm = 0.0;
  ll_gc_ll_reverse(lat1, lon1, lat2, lon2, &bearing, &dist_nm);

  double lat_up1, lon_up1, lat_up2, lon_up2;
  double lat_down1, lon_down1, lat_down2, lon_down2;
  ll_gc_ll(lat1, lon1, SegmentSafetyNormalizeBearing(bearing - 90.0),
           safety_margin_nm, &lat_up1, &lon_up1);
  ll_gc_ll(lat2, lon2, SegmentSafetyNormalizeBearing(bearing - 90.0),
           safety_margin_nm, &lat_up2, &lon_up2);
  ll_gc_ll(lat1, lon1, SegmentSafetyNormalizeBearing(bearing + 90.0),
           safety_margin_nm, &lat_down1, &lon_down1);
  ll_gc_ll(lat2, lon2, SegmentSafetyNormalizeBearing(bearing + 90.0),
           safety_margin_nm, &lat_down2, &lon_down2);

  if (PlugIn_GSHHS_CrossesLand(lat_up1, lon_up1, lat_up2, lon_up2) ||
      PlugIn_GSHHS_CrossesLand(lat_down1, lon_down1, lat_down2, lon_down2) ||
      PlugIn_GSHHS_CrossesLand(lat_up1, lon_up1, lat_down2, lon_down2) ||
      PlugIn_GSHHS_CrossesLand(lat_down1, lon_down1, lat_up2, lon_up2)) {
    if (status) *status = PI_SEGMENT_SAFETY_WITHIN_LAND_MARGIN;
    return true;
  }

  return false;
}

}  // namespace

wxString PlugIn_SegmentSafetyPointDiagnostic(double lat, double lon) {
  return SegmentSafetyPointDiagnostic(lat, lon);
}

bool PlugIn_CheckSegmentSafety(double lat1, double lon1, double lat2,
                               double lon2,
                               const PlugInSegmentSafetyOptions* options,
                               PlugInSegmentSafetyResult* result) {
  InitSegmentSafetyResult(result);

  if (options && options->struct_size < (int)sizeof(int)) {
    SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_ERROR);
    SetSegmentSafetyMessage(result, "invalid segment safety options");
    return false;
  }

  if (result && result->struct_size < (int)sizeof(int)) return false;

  const double safety_margin_nm = SegmentSafetyOptionMargin(options);
  const bool check_land = SegmentSafetyOptionCheckLand(options);
  const bool allow_gshhs_fallback =
      SegmentSafetyOptionAllowGshhsFallback(options);

  if (!check_land) {
    if (result) {
      SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_SAFE);
      SetSegmentSafetyMessage(result, "land checks disabled");
    }
    return true;
  }

  bool chart_data_available = false;
  SegmentSafetyCoreStats stats;
  if (ChartSegmentPointClassificationCheck(
          lat1, lon1, lat2, lon2, safety_margin_nm, result,
          &chart_data_available, &stats)) {
    SetSegmentSafetyDiagnosticReason(
        result, PI_SEGMENT_SAFETY_DIAG_CHART_GEOMETRY_HIT);
    ApplySegmentSafetyStats(result, stats);
    return true;
  }

  if (!chart_data_available &&
      CachedChartSegmentSafetyCheck(lat1, lon1, lat2, lon2, safety_margin_nm,
                                    result, &chart_data_available, &stats)) {
    SetSegmentSafetyDiagnosticReason(
        result, PI_SEGMENT_SAFETY_DIAG_CHART_GEOMETRY_HIT);
    ApplySegmentSafetyStats(result, stats);
    return true;
  }

  if (chart_data_available) {
    if (result) {
      SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_SAFE);
      if (GetSegmentSafetySource(result) == PI_SEGMENT_SAFETY_SOURCE_NONE)
        SetSegmentSafetySource(result, PI_SEGMENT_SAFETY_SOURCE_VECTOR_CHART);
      SetSegmentSafetyDiagnosticReason(
          result, PI_SEGMENT_SAFETY_DIAG_CHART_GEOMETRY_CLEAR);
      SetSegmentSafetyMessage(
          result,
          "segment is clear using chart point land classification");
      ApplySegmentSafetyStats(result, stats);
    }
    return true;
  }

  PlugInSegmentSafetyDiagnosticReason unavailable_reason =
      SegmentSafetyUnavailableReason(stats);
  if (allow_gshhs_fallback) {
    PlugInSegmentSafetyStatus fallback_status = PI_SEGMENT_SAFETY_SAFE;
    bool crosses_land = GshhsSegmentSafetyHitsLand(
        lat1, lon1, lat2, lon2, safety_margin_nm, &fallback_status);
    if (result) {
      SetSegmentSafetySource(result, PI_SEGMENT_SAFETY_SOURCE_GSHHS_FALLBACK);
      SetSegmentSafetyFallback(result, true);
      SetSegmentSafetyStatus(result, crosses_land ? fallback_status
                                                  : PI_SEGMENT_SAFETY_SAFE);
      SetSegmentSafetyDiagnosticReason(result, unavailable_reason);
      SetSegmentSafetyMessage(
          result, crosses_land ? "segment crosses GSHHS shoreline fallback"
                               : SegmentSafetyUnavailableMessage(
                                     unavailable_reason));
      ApplySegmentSafetyStats(result, stats);
    }
    return true;
  }

  if (result) {
    SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_NO_DATA);
    SetSegmentSafetyDiagnosticReason(result, unavailable_reason);
    SetSegmentSafetyMessage(result,
                            SegmentSafetyUnavailableMessage(unavailable_reason));
    ApplySegmentSafetyStats(result, stats);
  }
  return true;
}

bool PlugIn_PrewarmSegmentSafetyGrid(double min_lat, double min_lon,
                                     double max_lat, double max_lon,
                                     PlugInSegmentSafetyResult* result) {
  InitSegmentSafetyResult(result);

  if (result && result->struct_size < (int)sizeof(int)) return false;

  if (min_lat > max_lat) std::swap(min_lat, max_lat);
  if (min_lon > max_lon) std::swap(min_lon, max_lon);
  min_lat = wxMax(-90.0, wxMin(90.0, min_lat));
  max_lat = wxMax(-90.0, wxMin(90.0, max_lat));

  long min_lat_tile = floor(min_lat / kSegmentSafetyGridTileDegrees);
  long max_lat_tile = floor(max_lat / kSegmentSafetyGridTileDegrees);
  long min_lon_tile = floor(min_lon / kSegmentSafetyGridTileDegrees);
  long max_lon_tile = floor(max_lon / kSegmentSafetyGridTileDegrees);

  long lat_count = max_lat_tile - min_lat_tile + 1;
  long lon_count = max_lon_tile - min_lon_tile + 1;
  long requested_tiles =
      lat_count > 0 && lon_count > 0 ? lat_count * lon_count : 0;
  const long max_prewarm_tiles = 1024;

  SegmentSafetyCoreStats stats;
  long built_tiles = 0;
  long reused_tiles = 0;
  bool capped = requested_tiles > max_prewarm_tiles;

  for (long lat_tile = min_lat_tile; lat_tile <= max_lat_tile; ++lat_tile) {
    for (long lon_tile = min_lon_tile; lon_tile <= max_lon_tile; ++lon_tile) {
      if (built_tiles + reused_tiles >= max_prewarm_tiles) goto done;

      std::string key = SegmentSafetyGridTileKeyForIndices(lat_tile, lon_tile);
      if (s_segment_safety_grid_cache.find(key) !=
          s_segment_safety_grid_cache.end()) {
        ++stats.grid_cache_hits;
        ++reused_tiles;
        continue;
      }

      bool built = false;
      EnsureSegmentSafetyGridTile(lat_tile, lon_tile, &stats, &built);
      if (built) ++built_tiles;
    }
  }

done:
  if (result) {
    SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_SAFE);
    SetSegmentSafetySource(result, PI_SEGMENT_SAFETY_SOURCE_VECTOR_CHART);
    SetSegmentSafetyDiagnosticReason(
        result, PI_SEGMENT_SAFETY_DIAG_CHART_GEOMETRY_CLEAR);
    SetSegmentSafetyMessage(
        result, capped ? "segment safety grid prewarm capped"
                       : "segment safety grid prewarmed");
    ApplySegmentSafetyStats(result, stats);
  }

  wxString message = wxString::Format(
      "SEGMENT_SAFETY_GRID prewarm bbox=[lat %.6f..%.6f lon %.6f..%.6f] "
      "requested_tiles=%ld built_tiles=%ld reused_tiles=%ld capped=%d ",
      min_lat, max_lat, min_lon, max_lon, requested_tiles, built_tiles,
      reused_tiles, capped ? 1 : 0);
  message += wxString::Format(
      "build_ms=%d cells=%d land=%d water=%d drying=%d unknown=%d "
      "point_cache_hits=%d point_cache_misses=%d",
      stats.grid_build_ms, stats.grid_cells_total, stats.grid_cells_land,
      stats.grid_cells_water, stats.grid_cells_drying,
      stats.grid_cells_unknown, stats.point_cache_hits,
      stats.point_cache_misses);
  wxLogMessage("%s", message.c_str());

  return true;
}

bool PlugIn_PrewarmSegmentSafetyGridForSegment(
    double lat1, double lon1, double lat2, double lon2,
    double safety_margin_nm, PlugInSegmentSafetyResult* result) {
  InitSegmentSafetyResult(result);

  if (result && result->struct_size < (int)sizeof(int)) return false;

  double bearing = 0.0;
  double dist_nm = 0.0;
  ll_gc_ll_reverse(lat1, lon1, lat2, lon2, &bearing, &dist_nm);

  const int max_samples = 512;
  int samples = wxMax(2, wxMin(max_samples, (int)ceil(dist_nm / 0.05) + 1));
  std::set<std::pair<long, long> > tiles;

  for (int i = 0; i < samples; ++i) {
    double sample_dist = samples == 1 ? 0.0 : dist_nm * i / (samples - 1);
    double lat = lat1;
    double lon = lon1;
    if (sample_dist > 0.0)
      ll_gc_ll(lat1, lon1, bearing, sample_dist, &lat, &lon);

    long lat_tile = 0;
    long lon_tile = 0;
    SegmentSafetyGridTileKey(lat, lon, &lat_tile, &lon_tile);
    tiles.insert(std::make_pair(lat_tile, lon_tile));

    if (safety_margin_nm > 0.0) {
      double left_lat, left_lon, right_lat, right_lon;
      ll_gc_ll(lat, lon, SegmentSafetyNormalizeBearing(bearing - 90.0),
               safety_margin_nm, &left_lat, &left_lon);
      ll_gc_ll(lat, lon, SegmentSafetyNormalizeBearing(bearing + 90.0),
               safety_margin_nm, &right_lat, &right_lon);
      SegmentSafetyGridTileKey(left_lat, left_lon, &lat_tile, &lon_tile);
      tiles.insert(std::make_pair(lat_tile, lon_tile));
      SegmentSafetyGridTileKey(right_lat, right_lon, &lat_tile, &lon_tile);
      tiles.insert(std::make_pair(lat_tile, lon_tile));
    }
  }

  const long max_prewarm_tiles = 512;
  SegmentSafetyCoreStats stats;
  long built_tiles = 0;
  long reused_tiles = 0;
  bool capped = (long)tiles.size() > max_prewarm_tiles;

  long visited = 0;
  for (std::set<std::pair<long, long> >::const_iterator it = tiles.begin();
       it != tiles.end(); ++it) {
    if (visited++ >= max_prewarm_tiles) break;
    std::string key = SegmentSafetyGridTileKeyForIndices(it->first, it->second);
    if (s_segment_safety_grid_cache.find(key) !=
        s_segment_safety_grid_cache.end()) {
      ++stats.grid_cache_hits;
      ++reused_tiles;
      continue;
    }
    bool built = false;
    EnsureSegmentSafetyGridTile(it->first, it->second, &stats, &built);
    if (built) ++built_tiles;
  }

  if (result) {
    SetSegmentSafetyStatus(result, PI_SEGMENT_SAFETY_SAFE);
    SetSegmentSafetySource(result, PI_SEGMENT_SAFETY_SOURCE_VECTOR_CHART);
    SetSegmentSafetyDiagnosticReason(
        result, PI_SEGMENT_SAFETY_DIAG_CHART_GEOMETRY_CLEAR);
    SetSegmentSafetyMessage(
        result, capped ? "segment safety corridor prewarm capped"
                       : "segment safety corridor prewarmed");
    ApplySegmentSafetyStats(result, stats);
  }

  wxString message = wxString::Format(
      "SEGMENT_SAFETY_GRID prewarm_segment start=(%.6f,%.6f) "
      "end=(%.6f,%.6f) margin_nm=%.3f samples=%d requested_tiles=%lu "
      "built_tiles=%ld reused_tiles=%ld capped=%d ",
      lat1, lon1, lat2, lon2, safety_margin_nm, samples,
      static_cast<unsigned long>(tiles.size()), built_tiles, reused_tiles,
      capped ? 1 : 0);
  message += wxString::Format(
      "build_ms=%d cells=%d land=%d water=%d drying=%d unknown=%d "
      "point_cache_hits=%d point_cache_misses=%d",
      stats.grid_build_ms, stats.grid_cells_total, stats.grid_cells_land,
      stats.grid_cells_water, stats.grid_cells_drying,
      stats.grid_cells_unknown, stats.point_cache_hits,
      stats.point_cache_misses);
  wxLogMessage("%s", message.c_str());

  return true;
}

void PlugInPlaySound(wxString& sound_file) {
  PlugInPlaySoundEx(sound_file, -1);
}

//---------------------------------------------------------------------------
//    API 1.10
//---------------------------------------------------------------------------

// API Route and Waypoint Support
PlugIn_Waypoint::PlugIn_Waypoint() { m_HyperlinkList = NULL; }

PlugIn_Waypoint::PlugIn_Waypoint(double lat, double lon,
                                 const wxString& icon_ident,
                                 const wxString& wp_name,
                                 const wxString& GUID) {
  wxDateTime now = wxDateTime::Now();
  m_CreateTime = now.ToUTC();
  m_HyperlinkList = NULL;

  m_lat = lat;
  m_lon = lon;
  m_IconName = icon_ident;
  m_MarkName = wp_name;
  m_GUID = GUID;
}

PlugIn_Waypoint::~PlugIn_Waypoint() {}

//      PlugInRoute implementation
PlugIn_Route::PlugIn_Route() { pWaypointList = new Plugin_WaypointList; }

PlugIn_Route::~PlugIn_Route() {
  pWaypointList->DeleteContents(false);  // do not delete Waypoints
  pWaypointList->Clear();

  delete pWaypointList;
}

//      PlugInTrack implementation
PlugIn_Track::PlugIn_Track() { pWaypointList = new Plugin_WaypointList; }

PlugIn_Track::~PlugIn_Track() {
  pWaypointList->DeleteContents(false);  // do not delete Waypoints
  pWaypointList->Clear();

  delete pWaypointList;
}

wxString GetNewGUID() { return GpxDocument::GetUUID(); }

bool AddCustomWaypointIcon(wxBitmap* pimage, wxString key,
                           wxString description) {
  wxImage image = pimage->ConvertToImage();
  WayPointmanGui(*pWayPointMan).ProcessIcon(image, key, description);
  return true;
}

static void cloneHyperlinkList(RoutePoint* dst, const PlugIn_Waypoint* src) {
  //  Transcribe (clone) the html HyperLink List, if present
  if (src->m_HyperlinkList == nullptr) return;

  if (src->m_HyperlinkList->GetCount() > 0) {
    wxPlugin_HyperlinkListNode* linknode = src->m_HyperlinkList->GetFirst();
    while (linknode) {
      Plugin_Hyperlink* link = linknode->GetData();

      Hyperlink* h = new Hyperlink();
      h->DescrText = link->DescrText;
      h->Link = link->Link;
      h->LType = link->Type;

      dst->m_HyperlinkList->push_back(h);

      linknode = linknode->GetNext();
    }
  }
}

bool AddSingleWaypoint(PlugIn_Waypoint* pwaypoint, bool b_permanent) {
  //  Validate the waypoint parameters a little bit

  //  GUID
  //  Make sure that this GUID is indeed unique in the Routepoint list
  bool b_unique = true;
  for (RoutePoint* prp : *pWayPointMan->GetWaypointList()) {
    if (prp->m_GUID == pwaypoint->m_GUID) {
      b_unique = false;
      break;
    }
  }

  if (!b_unique) return false;

  RoutePoint* pWP =
      new RoutePoint(pwaypoint->m_lat, pwaypoint->m_lon, pwaypoint->m_IconName,
                     pwaypoint->m_MarkName, pwaypoint->m_GUID);

  pWP->m_bIsolatedMark = true;  // This is an isolated mark

  cloneHyperlinkList(pWP, pwaypoint);

  pWP->m_MarkDescription = pwaypoint->m_MarkDescription;

  if (pwaypoint->m_CreateTime.IsValid())
    pWP->SetCreateTime(pwaypoint->m_CreateTime);
  else {
    pWP->SetCreateTime(wxDateTime::Now().ToUTC());
  }

  pWP->m_btemp = (b_permanent == false);

  pSelect->AddSelectableRoutePoint(pwaypoint->m_lat, pwaypoint->m_lon, pWP);
  if (b_permanent) {
    // pConfig->AddNewWayPoint(pWP, -1);
    NavObj_dB::GetInstance().InsertRoutePoint(pWP);
  }

  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateWptListCtrl();

  return true;
}

bool DeleteSingleWaypoint(wxString& GUID) {
  //  Find the RoutePoint
  bool b_found = false;
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(GUID);

  if (prp) b_found = true;

  if (b_found) {
    pWayPointMan->DestroyWaypoint(prp);
    if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
      pRouteManagerDialog->UpdateWptListCtrl();
  }

  return b_found;
}

bool UpdateSingleWaypoint(PlugIn_Waypoint* pwaypoint) {
  //  Find the RoutePoint
  bool b_found = false;
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(pwaypoint->m_GUID);

  if (prp) b_found = true;

  if (b_found) {
    double lat_save = prp->m_lat;
    double lon_save = prp->m_lon;

    prp->m_lat = pwaypoint->m_lat;
    prp->m_lon = pwaypoint->m_lon;
    prp->SetIconName(pwaypoint->m_IconName);
    prp->SetName(pwaypoint->m_MarkName);
    prp->m_MarkDescription = pwaypoint->m_MarkDescription;
    prp->SetVisible(pwaypoint->m_IsVisible);
    if (pwaypoint->m_CreateTime.IsValid())
      prp->SetCreateTime(pwaypoint->m_CreateTime);

    //  Transcribe (clone) the html HyperLink List, if present

    if (pwaypoint->m_HyperlinkList) {
      prp->m_HyperlinkList->clear();
      if (pwaypoint->m_HyperlinkList->GetCount() > 0) {
        wxPlugin_HyperlinkListNode* linknode =
            pwaypoint->m_HyperlinkList->GetFirst();
        while (linknode) {
          Plugin_Hyperlink* link = linknode->GetData();

          Hyperlink* h = new Hyperlink();
          h->DescrText = link->DescrText;
          h->Link = link->Link;
          h->LType = link->Type;

          prp->m_HyperlinkList->push_back(h);

          linknode = linknode->GetNext();
        }
      }
    }

    if (prp) prp->ReLoadIcon();

    auto canvas = gFrame->GetPrimaryCanvas();
    SelectCtx ctx(canvas->m_bShowNavobjects, canvas->GetCanvasTrueScale(),
                  canvas->GetScaleValue());
    SelectItem* pFind =
        pSelect->FindSelection(ctx, lat_save, lon_save, SELTYPE_ROUTEPOINT);
    if (pFind) {
      pFind->m_slat = pwaypoint->m_lat;  // update the SelectList entry
      pFind->m_slon = pwaypoint->m_lon;
    }

    if (!prp->m_btemp) {
      // pConfig->UpdateWayPoint(prp);
      NavObj_dB::GetInstance().UpdateRoutePoint(prp);
    }

    if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
      pRouteManagerDialog->UpdateWptListCtrl();
  }

  return b_found;
}

// translate O route class to Plugin one
static void PlugInFromRoutePoint(PlugIn_Waypoint* dst,
                                 /* const*/ RoutePoint* src) {
  dst->m_lat = src->m_lat;
  dst->m_lon = src->m_lon;
  dst->m_IconName = src->GetIconName();
  dst->m_MarkName = src->GetName();
  dst->m_MarkDescription = src->m_MarkDescription;
  dst->m_IsVisible = src->IsVisible();
  dst->m_CreateTime = src->GetCreateTime();  // not const
  dst->m_GUID = src->m_GUID;

  //  Transcribe (clone) the html HyperLink List, if present
  if (src->m_HyperlinkList == nullptr) return;

  delete dst->m_HyperlinkList;
  dst->m_HyperlinkList = nullptr;

  if (src->m_HyperlinkList->size() > 0) {
    dst->m_HyperlinkList = new Plugin_HyperlinkList;
    for (Hyperlink* link : *src->m_HyperlinkList) {
      Plugin_Hyperlink* h = new Plugin_Hyperlink();
      h->DescrText = link->DescrText;
      h->Link = link->Link;
      h->Type = link->LType;

      dst->m_HyperlinkList->Append(h);
    }
  }
}

bool GetSingleWaypoint(wxString GUID, PlugIn_Waypoint* pwaypoint) {
  //  Find the RoutePoint
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(GUID);

  if (!prp) return false;

  PlugInFromRoutePoint(pwaypoint, prp);

  return true;
}

wxArrayString GetWaypointGUIDArray() {
  wxArrayString result;
  if (pWayPointMan) {
    for (RoutePoint* prp : *pWayPointMan->GetWaypointList()) {
      result.Add(prp->m_GUID);
    }
  }
  return result;
}

wxArrayString GetRouteGUIDArray() {
  wxArrayString result;
  for (Route* proute : *pRouteList) {
    result.Add(proute->m_GUID);
  }
  return result;
}

wxArrayString GetTrackGUIDArray() {
  wxArrayString result;
  for (Track* ptrack : g_TrackList) {
    result.Add(ptrack->m_GUID);
  }

  return result;
}

wxArrayString GetWaypointGUIDArray(OBJECT_LAYER_REQ req) {
  wxArrayString result;
  for (RoutePoint* prp : *pWayPointMan->GetWaypointList()) {
    switch (req) {
      case OBJECTS_ALL:
        result.Add(prp->m_GUID);
        break;
      case OBJECTS_NO_LAYERS:
        if (!prp->m_bIsInLayer) result.Add(prp->m_GUID);
        break;
      case OBJECTS_ONLY_LAYERS:
        if (prp->m_bIsInLayer) result.Add(prp->m_GUID);
        break;
    }
  }
  return result;
}

wxArrayString GetRouteGUIDArray(OBJECT_LAYER_REQ req) {
  wxArrayString result;

  for (Route* proute : *pRouteList) {
    switch (req) {
      case OBJECTS_ALL:
        result.Add(proute->m_GUID);
        break;
      case OBJECTS_NO_LAYERS:
        if (!proute->m_bIsInLayer) result.Add(proute->m_GUID);
        break;
      case OBJECTS_ONLY_LAYERS:
        if (proute->m_bIsInLayer) result.Add(proute->m_GUID);
        break;
    }
  }

  return result;
}

wxArrayString GetTrackGUIDArray(OBJECT_LAYER_REQ req) {
  wxArrayString result;
  for (Track* ptrack : g_TrackList) {
    switch (req) {
      case OBJECTS_ALL:
        result.Add(ptrack->m_GUID);
        break;
      case OBJECTS_NO_LAYERS:
        if (!ptrack->m_bIsInLayer) result.Add(ptrack->m_GUID);
        break;
      case OBJECTS_ONLY_LAYERS:
        if (ptrack->m_bIsInLayer) result.Add(ptrack->m_GUID);
        break;
    }
  }

  return result;
}

wxArrayString GetIconNameArray() {
  wxArrayString result;

  for (int i = 0; i < pWayPointMan->GetNumIcons(); i++) {
    wxString* ps = pWayPointMan->GetIconKey(i);
    result.Add(*ps);
  }
  return result;
}

bool AddPlugInRoute(PlugIn_Route* proute, bool b_permanent) {
  Route* route = new Route();

  PlugIn_Waypoint* pwp;
  RoutePoint* pWP_src;
  int ip = 0;
  wxDateTime plannedDeparture;

  wxPlugin_WaypointListNode* pwpnode = proute->pWaypointList->GetFirst();
  while (pwpnode) {
    pwp = pwpnode->GetData();

    RoutePoint* pWP = new RoutePoint(pwp->m_lat, pwp->m_lon, pwp->m_IconName,
                                     pwp->m_MarkName, pwp->m_GUID);

    //  Transcribe (clone) the html HyperLink List, if present
    cloneHyperlinkList(pWP, pwp);
    pWP->m_MarkDescription = pwp->m_MarkDescription;
    pWP->m_bShowName = false;
    pWP->SetCreateTime(pwp->m_CreateTime);

    route->AddPoint(pWP);

    pSelect->AddSelectableRoutePoint(pWP->m_lat, pWP->m_lon, pWP);

    if (ip > 0)
      pSelect->AddSelectableRouteSegment(pWP_src->m_lat, pWP_src->m_lon,
                                         pWP->m_lat, pWP->m_lon, pWP_src, pWP,
                                         route);
    else
      plannedDeparture = pwp->m_CreateTime;
    ip++;
    pWP_src = pWP;

    pwpnode = pwpnode->GetNext();  // PlugInWaypoint
  }

  route->m_PlannedDeparture = plannedDeparture;

  route->m_RouteNameString = proute->m_NameString;
  route->m_RouteStartString = proute->m_StartString;
  route->m_RouteEndString = proute->m_EndString;
  if (!proute->m_GUID.IsEmpty()) {
    route->m_GUID = proute->m_GUID;
  }
  route->m_btemp = (b_permanent == false);

  pRouteList->push_back(route);

  if (b_permanent) {
    // pConfig->AddNewRoute(route);
    NavObj_dB::GetInstance().InsertRoute(route);
  }
  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateRouteListCtrl();

  return true;
}

bool DeletePlugInRoute(wxString& GUID) {
  bool b_found = false;

  //  Find the Route
  Route* pRoute = g_pRouteMan->FindRouteByGUID(GUID);
  if (pRoute) {
    g_pRouteMan->DeleteRoute(pRoute);
    b_found = true;
  }
  return b_found;
}

bool UpdatePlugInRoute(PlugIn_Route* proute) {
  bool b_found = false;

  //  Find the Route
  Route* pRoute = g_pRouteMan->FindRouteByGUID(proute->m_GUID);
  if (pRoute) b_found = true;

  if (b_found) {
    bool b_permanent = (pRoute->m_btemp == false);
    g_pRouteMan->DeleteRoute(pRoute);
    b_found = AddPlugInRoute(proute, b_permanent);
  }

  return b_found;
}

bool AddPlugInTrack(PlugIn_Track* ptrack, bool b_permanent) {
  Track* track = new Track();

  PlugIn_Waypoint* pwp = 0;
  TrackPoint* pWP_src = 0;
  int ip = 0;

  wxPlugin_WaypointListNode* pwpnode = ptrack->pWaypointList->GetFirst();
  while (pwpnode) {
    pwp = pwpnode->GetData();

    TrackPoint* pWP = new TrackPoint(pwp->m_lat, pwp->m_lon);
    pWP->SetCreateTime(pwp->m_CreateTime);

    track->AddPoint(pWP);

    if (ip > 0)
      pSelect->AddSelectableTrackSegment(pWP_src->m_lat, pWP_src->m_lon,
                                         pWP->m_lat, pWP->m_lon, pWP_src, pWP,
                                         track);
    ip++;
    pWP_src = pWP;

    pwpnode = pwpnode->GetNext();  // PlugInWaypoint
  }

  track->SetName(ptrack->m_NameString);
  track->m_TrackStartString = ptrack->m_StartString;
  track->m_TrackEndString = ptrack->m_EndString;
  track->m_GUID = ptrack->m_GUID;
  track->m_btemp = (b_permanent == false);

  g_TrackList.push_back(track);
  if (b_permanent) NavObj_dB::GetInstance().InsertTrack(track);
  // if (b_permanent) pConfig->AddNewTrack(track);

  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateTrkListCtrl();

  return true;
}

bool DeletePlugInTrack(wxString& GUID) {
  bool b_found = false;

  //  Find the Route
  Track* pTrack = g_pRouteMan->FindTrackByGUID(GUID);
  if (pTrack) {
    NavObj_dB::GetInstance().DeleteTrack(pTrack);
    RoutemanGui(*g_pRouteMan).DeleteTrack(pTrack);
    b_found = true;
  }

  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateTrkListCtrl();

  return b_found;
}

bool UpdatePlugInTrack(PlugIn_Track* ptrack) {
  bool b_found = false;

  //  Find the Track
  Track* pTrack = g_pRouteMan->FindTrackByGUID(ptrack->m_GUID);
  if (pTrack) b_found = true;

  if (b_found) {
    bool b_permanent = (pTrack->m_btemp == false);
    NavObj_dB::GetInstance().DeleteTrack(pTrack);
    RoutemanGui(*g_pRouteMan).DeleteTrack(pTrack);

    b_found = AddPlugInTrack(ptrack, b_permanent);
  }

  return b_found;
}

bool PlugInHasNormalizedViewPort(PlugIn_ViewPort* vp) {
#ifdef ocpnUSE_GL
  ViewPort ocpn_vp;
  ocpn_vp.m_projection_type = vp->m_projection_type;

  return glChartCanvas::HasNormalizedViewPort(ocpn_vp);
#else
  return false;
#endif
}

void PlugInMultMatrixViewport(PlugIn_ViewPort* vp, float lat, float lon) {
#ifdef ocpnUSE_GL
  ViewPort ocpn_vp;
  ocpn_vp.clat = vp->clat;
  ocpn_vp.clon = vp->clon;
  ocpn_vp.m_projection_type = vp->m_projection_type;
  ocpn_vp.view_scale_ppm = vp->view_scale_ppm;
  ocpn_vp.skew = vp->skew;
  ocpn_vp.rotation = vp->rotation;
  ocpn_vp.pix_width = vp->pix_width;
  ocpn_vp.pix_height = vp->pix_height;

// TODO fix for multicanvas    glChartCanvas::MultMatrixViewPort(ocpn_vp, lat,
// lon);
#endif
}

void PlugInNormalizeViewport(PlugIn_ViewPort* vp, float lat, float lon) {
#ifdef ocpnUSE_GL
  ViewPort ocpn_vp;
  glChartCanvas::NormalizedViewPort(ocpn_vp, lat, lon);

  vp->clat = ocpn_vp.clat;
  vp->clon = ocpn_vp.clon;
  vp->view_scale_ppm = ocpn_vp.view_scale_ppm;
  vp->rotation = ocpn_vp.rotation;
  vp->skew = ocpn_vp.skew;
#endif
}

//          Helper and interface classes

//-------------------------------------------------------------------------------
//    PlugIn_AIS_Target Implementation
//-------------------------------------------------------------------------------

PlugIn_AIS_Target* Create_PI_AIS_Target(AisTargetData* ptarget) {
  PlugIn_AIS_Target* pret = new PlugIn_AIS_Target;

  pret->MMSI = ptarget->MMSI;
  pret->Class = ptarget->Class;
  pret->NavStatus = ptarget->NavStatus;
  pret->SOG = ptarget->SOG;
  pret->COG = ptarget->COG;
  pret->HDG = ptarget->HDG;
  pret->Lon = ptarget->Lon;
  pret->Lat = ptarget->Lat;
  pret->ROTAIS = ptarget->ROTAIS;
  pret->ShipType = ptarget->ShipType;
  pret->IMO = ptarget->IMO;

  pret->Range_NM = ptarget->Range_NM;
  pret->Brg = ptarget->Brg;

  //      Per target collision parameters
  pret->bCPA_Valid = ptarget->bCPA_Valid;
  pret->TCPA = ptarget->TCPA;  // Minutes
  pret->CPA = ptarget->CPA;    // Nautical Miles

  pret->alarm_state = (plugin_ais_alarm_type)ptarget->n_alert_state;

  memcpy(pret->CallSign, ptarget->CallSign, sizeof(ptarget->CallSign) - 1);
  memcpy(pret->ShipName, ptarget->ShipName, sizeof(ptarget->ShipName) - 1);

  return pret;
}

//---------------------------------------------------------------------------
//    API 1.11
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//    API 1.12
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//    API 1.13
//---------------------------------------------------------------------------
double fromDMM_Plugin(wxString sdms) { return fromDMM(sdms); }

void SetCanvasRotation(double rotation) {
  gFrame->GetPrimaryCanvas()->DoRotateCanvas(rotation);
}

double GetCanvasTilt() { return gFrame->GetPrimaryCanvas()->GetVPTilt(); }

void SetCanvasTilt(double tilt) {
  gFrame->GetPrimaryCanvas()->DoTiltCanvas(tilt);
}

void SetCanvasProjection(int projection) {
  gFrame->GetPrimaryCanvas()->SetVPProjection(projection);
}

o_sound::Sound* g_PluginSound = o_sound::Factory();
static void onPlugInPlaySoundExFinished(void* ptr) {}

// Start playing a sound to a given device and return status to plugin
bool PlugInPlaySoundEx(wxString& sound_file, int deviceIndex) {
  bool ok = g_PluginSound->Load(sound_file, deviceIndex);
  if (!ok) {
    wxLogWarning("Cannot load sound file: %s", sound_file);
    return false;
  }
  auto cmd_sound = dynamic_cast<o_sound::SystemCmdSound*>(g_PluginSound);
  if (cmd_sound) cmd_sound->SetCmd(g_CmdSoundString.mb_str(wxConvUTF8));

  g_PluginSound->SetFinishedCallback(onPlugInPlaySoundExFinished, NULL);
  ok = g_PluginSound->Play();
  if (!ok) {
    wxLogWarning("Cannot play sound file: %s", sound_file);
  }
  return ok;
}

bool CheckEdgePan_PlugIn(int x, int y, bool dragging, int margin, int delta) {
  return gFrame->GetPrimaryCanvas()->CheckEdgePan(x, y, dragging, margin,
                                                  delta);
}

wxBitmap GetIcon_PlugIn(const wxString& name) {
  ocpnStyle::Style* style = g_StyleManager->GetCurrentStyle();
  return style->GetIcon(name);
}

void SetCursor_PlugIn(wxCursor* pCursor) {
  gFrame->GetPrimaryCanvas()->pPlugIn_Cursor = pCursor;
}

void AddChartDirectory(wxString& path) {
  if (g_options) {
    g_options->AddChartDir(path);
  }
}

void ForceChartDBUpdate() {
  if (g_options) {
    g_options->pScanCheckBox->SetValue(true);
    g_options->pUpdateCheckBox->SetValue(true);
  }
}

void ForceChartDBRebuild() {
  if (g_options) {
    g_options->pUpdateCheckBox->SetValue(true);
  }
}

wxDialog* GetActiveOptionsDialog() { return g_options; }

int PlatformDirSelectorDialog(wxWindow* parent, wxString* file_spec,
                              wxString Title, wxString initDir) {
  return g_Platform->DoDirSelectorDialog(parent, file_spec, Title, initDir);
}

int PlatformFileSelectorDialog(wxWindow* parent, wxString* file_spec,
                               wxString Title, wxString initDir,
                               wxString suggestedName, wxString wildcard) {
  return g_Platform->DoFileSelectorDialog(parent, file_spec, Title, initDir,
                                          suggestedName, wildcard);
}

//---------------------------------------------------------------------------
//    API 1.14
//---------------------------------------------------------------------------

ViewPort CreateCompatibleViewportEx(const PlugIn_ViewPort& pivp) {
  //    Create a system ViewPort
  ViewPort vp;

  vp.clat = pivp.clat;  // center point
  vp.clon = pivp.clon;
  vp.view_scale_ppm = pivp.view_scale_ppm;
  vp.skew = pivp.skew;
  vp.rotation = pivp.rotation;
  vp.chart_scale = pivp.chart_scale;
  vp.pix_width = pivp.pix_width;
  vp.pix_height = pivp.pix_height;
  vp.rv_rect = pivp.rv_rect;
  vp.b_quilt = pivp.b_quilt;
  vp.m_projection_type = pivp.m_projection_type;

  if (gFrame->GetPrimaryCanvas())
    vp.ref_scale = gFrame->GetPrimaryCanvas()->GetVP().ref_scale;
  else
    vp.ref_scale = vp.chart_scale;

  vp.SetBoxes();
  vp.Validate();  // This VP is valid

  return vp;
}

void PlugInAISDrawGL(wxGLCanvas* glcanvas, const PlugIn_ViewPort& vp) {
  ViewPort ocpn_vp = CreateCompatibleViewportEx(vp);

  ocpnDC dc(*glcanvas);
  dc.SetVP(ocpn_vp);

  AISDraw(dc, ocpn_vp, NULL);
}

bool PlugInSetFontColor(const wxString TextElement, const wxColour color) {
  return FontMgr::Get().SetFontColor(TextElement, color);
}

//---------------------------------------------------------------------------
//    API 1.15
//---------------------------------------------------------------------------

double PlugInGetDisplaySizeMM() { return g_Platform->GetDisplaySizeMM(); }

wxFont* FindOrCreateFont_PlugIn(int point_size, wxFontFamily family,
                                wxFontStyle style, wxFontWeight weight,
                                bool underline, const wxString& facename,
                                wxFontEncoding encoding) {
  return FontMgr::Get().FindOrCreateFont(point_size, family, style, weight,
                                         underline, facename, encoding);
}

int PluginGetMinAvailableGshhgQuality() {
  return gFrame->GetPrimaryCanvas()->GetMinAvailableGshhgQuality();
}
int PluginGetMaxAvailableGshhgQuality() {
  return gFrame->GetPrimaryCanvas()->GetMaxAvailableGshhgQuality();
}

// disable builtin console canvas, and autopilot nmea sentences
void PlugInHandleAutopilotRoute(bool enable) {
  g_bPluginHandleAutopilotRoute = enable;
}

bool LaunchDefaultBrowser_Plugin(wxString url) {
  if (g_Platform) g_Platform->platformLaunchDefaultBrowser(url);

  return true;
}

//---------------------------------------------------------------------------
//    API 1.16
//---------------------------------------------------------------------------
wxString GetSelectedWaypointGUID_Plugin() {
  ChartCanvas* cc = gFrame->GetFocusCanvas();
  if (cc && cc->GetSelectedRoutePoint()) {
    return cc->GetSelectedRoutePoint()->m_GUID;
  }
  return wxEmptyString;
}

wxString GetSelectedRouteGUID_Plugin() {
  ChartCanvas* cc = gFrame->GetFocusCanvas();
  if (cc && cc->GetSelectedRoute()) {
    return cc->GetSelectedRoute()->m_GUID;
  }
  return wxEmptyString;
}

wxString GetSelectedTrackGUID_Plugin() {
  ChartCanvas* cc = gFrame->GetFocusCanvas();
  if (cc && cc->GetSelectedTrack()) {
    return cc->GetSelectedTrack()->m_GUID;
  }
  return wxEmptyString;
}

std::unique_ptr<PlugIn_Waypoint> GetWaypoint_Plugin(const wxString& GUID) {
  std::unique_ptr<PlugIn_Waypoint> w(new PlugIn_Waypoint);
  GetSingleWaypoint(GUID, w.get());
  return w;
}

std::unique_ptr<PlugIn_Route> GetRoute_Plugin(const wxString& GUID) {
  std::unique_ptr<PlugIn_Route> r;
  Route* route = g_pRouteMan->FindRouteByGUID(GUID);
  if (route == nullptr) return r;

  r = std::unique_ptr<PlugIn_Route>(new PlugIn_Route);
  PlugIn_Route* dst_route = r.get();

  // PlugIn_Waypoint *pwp;
  RoutePoint* src_wp;
  for (RoutePoint* src_wp : *route->pRoutePointList) {
    PlugIn_Waypoint* dst_wp = new PlugIn_Waypoint();
    PlugInFromRoutePoint(dst_wp, src_wp);
    dst_route->pWaypointList->Append(dst_wp);
  }
  dst_route->m_NameString = route->m_RouteNameString;
  dst_route->m_StartString = route->m_RouteStartString;
  dst_route->m_EndString = route->m_RouteEndString;
  dst_route->m_GUID = route->m_GUID;

  return r;
}

std::unique_ptr<PlugIn_Track> GetTrack_Plugin(const wxString& GUID) {
  std::unique_ptr<PlugIn_Track> t;
  //  Find the Track
  Track* pTrack = g_pRouteMan->FindTrackByGUID(GUID);
  if (!pTrack) return t;

  std::unique_ptr<PlugIn_Track> tk =
      std::unique_ptr<PlugIn_Track>(new PlugIn_Track);
  PlugIn_Track* dst_track = tk.get();
  dst_track->m_NameString = pTrack->GetName();
  dst_track->m_StartString = pTrack->m_TrackStartString;
  dst_track->m_EndString = pTrack->m_TrackEndString;
  dst_track->m_GUID = pTrack->m_GUID;

  for (int i = 0; i < pTrack->GetnPoints(); i++) {
    TrackPoint* ptp = pTrack->GetPoint(i);

    PlugIn_Waypoint* dst_wp = new PlugIn_Waypoint();

    dst_wp->m_lat = ptp->m_lat;
    dst_wp->m_lon = ptp->m_lon;
    dst_wp->m_CreateTime = ptp->GetCreateTime();  // not const

    dst_track->pWaypointList->Append(dst_wp);
  }

  return tk;
}

wxWindow* PluginGetFocusCanvas() { return g_focusCanvas; }

wxWindow* PluginGetOverlayRenderCanvas() {
  // if(g_overlayCanvas)
  return g_overlayCanvas;
  // else
}

void CanvasJumpToPosition(wxWindow* canvas, double lat, double lon,
                          double scale) {
  auto oCanvas = dynamic_cast<ChartCanvas*>(canvas);
  if (oCanvas) gFrame->JumpToPosition(oCanvas, lat, lon, scale);
}

bool ShuttingDown() { return g_bquiting; }

wxWindow* GetCanvasUnderMouse() { return gFrame->GetCanvasUnderMouse(); }

int GetCanvasIndexUnderMouse() {
  ChartCanvas* l_canvas = gFrame->GetCanvasUnderMouse();
  if (l_canvas) {
    for (unsigned int i = 0; i < g_canvasArray.GetCount(); ++i) {
      if (l_canvas == g_canvasArray[i]) return i;
    }
  }
  return 0;
}

// std::vector<wxWindow *> GetCanvasArray()
// {
//     std::vector<wxWindow *> rv;
//     for(unsigned int i=0 ; i < g_canvasArray.GetCount() ; i++){
//         ChartCanvas *cc = g_canvasArray.Item(i);
//         rv.push_back(cc);
//     }
//
//     return rv;
// }

wxWindow* GetCanvasByIndex(int canvasIndex) {
  if (g_canvasConfig == 0)
    return gFrame->GetPrimaryCanvas();
  else {
    if ((canvasIndex >= 0) && g_canvasArray[canvasIndex]) {
      return g_canvasArray[canvasIndex];
    }
  }
  return NULL;
}

bool CheckMUIEdgePan_PlugIn(int x, int y, bool dragging, int margin, int delta,
                            int canvasIndex) {
  if (g_canvasConfig == 0)
    return gFrame->GetPrimaryCanvas()->CheckEdgePan(x, y, dragging, margin,
                                                    delta);
  else {
    if ((canvasIndex >= 0) && g_canvasArray[canvasIndex]) {
      return g_canvasArray[canvasIndex]->CheckEdgePan(x, y, dragging, margin,
                                                      delta);
    }
  }

  return false;
}

void SetMUICursor_PlugIn(wxCursor* pCursor, int canvasIndex) {
  if (g_canvasConfig == 0)
    gFrame->GetPrimaryCanvas()->pPlugIn_Cursor = pCursor;
  else {
    if ((canvasIndex >= 0) && g_canvasArray[canvasIndex]) {
      g_canvasArray[canvasIndex]->pPlugIn_Cursor = pCursor;
    }
  }
}

int GetCanvasCount() {
  if (g_canvasConfig == 1) return 2;
  //     else
  return 1;
}

int GetLatLonFormat() { return g_iSDMMFormat; }

wxRect GetMasterToolbarRect() {
  if (g_MainToolbar)
    return g_MainToolbar->GetToolbarRect();
  else
    return wxRect(0, 0, 1, 1);
}

//---------------------------------------------------------------------------
//    API 1.17
//---------------------------------------------------------------------------

void ZeroXTE() {
  if (g_pRouteMan) {
    g_pRouteMan->ZeroCurrentXTEToActivePoint();
  }
}

static PlugIn_ViewPort CreatePlugInViewportEx(const ViewPort& vp) {
  //    Create a PlugIn Viewport
  ViewPort tvp = vp;
  PlugIn_ViewPort pivp;

  pivp.clat = tvp.clat;  // center point
  pivp.clon = tvp.clon;
  pivp.view_scale_ppm = tvp.view_scale_ppm;
  pivp.skew = tvp.skew;
  pivp.rotation = tvp.rotation;
  pivp.chart_scale = tvp.chart_scale;
  pivp.pix_width = tvp.pix_width;
  pivp.pix_height = tvp.pix_height;
  pivp.rv_rect = tvp.rv_rect;
  pivp.b_quilt = tvp.b_quilt;
  pivp.m_projection_type = tvp.m_projection_type;

  pivp.lat_min = tvp.GetBBox().GetMinLat();
  pivp.lat_max = tvp.GetBBox().GetMaxLat();
  pivp.lon_min = tvp.GetBBox().GetMinLon();
  pivp.lon_max = tvp.GetBBox().GetMaxLon();

  pivp.bValid = tvp.IsValid();  // This VP is valid

  return pivp;
}

ListOfPI_S57Obj* PlugInManager::GetLightsObjRuleListVisibleAtLatLon(
    ChartPlugInWrapper* target, float zlat, float zlon, const ViewPort& vp) {
  ListOfPI_S57Obj* list = NULL;
  if (target) {
    PlugInChartBaseGLPlus2* picbgl =
        dynamic_cast<PlugInChartBaseGLPlus2*>(target->GetPlugInChart());
    if (picbgl) {
      PlugIn_ViewPort pi_vp = CreatePlugInViewportEx(vp);
      list = picbgl->GetLightsObjRuleListVisibleAtLatLon(zlat, zlon, &pi_vp);

      return list;
    }
    PlugInChartBaseExtendedPlus2* picbx =
        dynamic_cast<PlugInChartBaseExtendedPlus2*>(target->GetPlugInChart());
    if (picbx) {
      PlugIn_ViewPort pi_vp = CreatePlugInViewportEx(vp);
      list = picbx->GetLightsObjRuleListVisibleAtLatLon(zlat, zlon, &pi_vp);

      return list;
    } else
      return list;
  } else
    return list;
}

//      PlugInWaypointEx implementation

#include <wx/listimpl.cpp>
WX_DEFINE_LIST(Plugin_WaypointExList)

//  The class implementations
PlugIn_Waypoint_Ex::PlugIn_Waypoint_Ex() { InitDefaults(); }

PlugIn_Waypoint_Ex::PlugIn_Waypoint_Ex(
    double lat, double lon, const wxString& icon_ident, const wxString& wp_name,
    const wxString& GUID, const double ScaMin, const bool bNameVisible,
    const int nRangeRings, const double RangeDistance,
    const wxColor RangeColor) {
  InitDefaults();

  m_lat = lat;
  m_lon = lon;
  IconName = icon_ident;
  m_MarkName = wp_name;
  m_GUID = GUID;
  scamin = ScaMin;
  IsNameVisible = bNameVisible;
  nrange_rings = nRangeRings;
  RangeRingSpace = RangeDistance;
  RangeRingColor = RangeColor;
}

void PlugIn_Waypoint_Ex::InitDefaults() {
  m_HyperlinkList = nullptr;
  scamin = 1e9;
  b_useScamin = false;
  nrange_rings = 0;
  RangeRingSpace = 1;
  IsNameVisible = false;
  IsVisible = true;
  RangeRingColor = *wxBLACK;
  m_CreateTime = wxDateTime::Now().ToUTC();
  IsActive = false;
  m_lat = 0;
  m_lon = 0;
}

bool PlugIn_Waypoint_Ex::GetFSStatus() {
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(m_GUID);
  if (!prp) return false;

  if (prp->m_bIsInRoute && !prp->IsShared()) return false;

  return true;
}

int PlugIn_Waypoint_Ex::GetRouteMembershipCount() {
  // Search all routes to count the membership of this point
  RoutePoint* pWP = pWayPointMan->FindRoutePointByGUID(m_GUID);
  if (!pWP) return 0;

  int nCount = 0;
  for (Route* proute : *pRouteList) {
    for (RoutePoint* prp : *proute->pRoutePointList) {
      if (prp == pWP) nCount++;
    }
  }
  return nCount;
}

PlugIn_Waypoint_Ex::~PlugIn_Waypoint_Ex() {}

WX_DEFINE_LIST(Plugin_WaypointExV2List)

PlugIn_Waypoint_ExV2::PlugIn_Waypoint_ExV2() { InitDefaults(); }

PlugIn_Waypoint_ExV2::PlugIn_Waypoint_ExV2(
    double lat, double lon, const wxString& icon_ident, const wxString& wp_name,
    const wxString& GUID, const double ScaMin, const double ScaMax,
    const bool bNameVisible, const int nRangeRings, const double RangeDistance,
    const int RangeDistanceUnits, const wxColor RangeColor,
    const double WaypointArrivalRadius, const bool ShowWaypointRangeRings,
    const double PlannedSpeed, const wxString TideStation) {
  // Initialize all to defaults first
  InitDefaults();
  // Then set the specific values provided
  m_lat = lat;
  m_lon = lon;
  IconName = icon_ident;
  m_MarkName = wp_name;
  m_GUID = GUID;
  scamin = ScaMin;
  scamax = ScaMax;

  IsNameVisible = bNameVisible;
  nrange_rings = nRangeRings;
  RangeRingSpace = RangeDistance;
  RangeRingSpaceUnits = RangeDistanceUnits;  // 0 = nm, 1 = km
  RangeRingColor = RangeColor;
  m_TideStation = TideStation;

  m_PlannedSpeed = PlannedSpeed;
  m_WaypointArrivalRadius = WaypointArrivalRadius;
  m_bShowWaypointRangeRings = ShowWaypointRangeRings;
}

void PlugIn_Waypoint_ExV2::InitDefaults() {
  m_HyperlinkList = nullptr;
  scamin = 1e9;
  scamax = 1e6;
  b_useScamin = false;
  nrange_rings = 0;
  RangeRingSpace = 1;
  RangeRingSpaceUnits = 0;  // 0 = nm, 1 = km
  m_TideStation = wxEmptyString;
  IsNameVisible = false;
  IsVisible = true;
  RangeRingColor = *wxBLACK;
  m_CreateTime = wxDateTime::Now().ToUTC();
  IsActive = false;
  m_lat = 0;
  m_lon = 0;

  m_PlannedSpeed = 0.0;
  m_WaypointArrivalRadius = 0.0;
  m_bShowWaypointRangeRings = false;
}

PlugIn_Waypoint_ExV2::~PlugIn_Waypoint_ExV2() {}

bool PlugIn_Waypoint_ExV2::GetFSStatus() {
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(m_GUID);
  if (!prp) return false;
  if (prp->m_bIsInRoute && !prp->IsShared()) return false;
  return true;
}

int PlugIn_Waypoint_ExV2::GetRouteMembershipCount() {
  // Search all routes to count the membership of this point
  RoutePoint* pWP = pWayPointMan->FindRoutePointByGUID(m_GUID);
  if (!pWP) return 0;

  int nCount = 0;
  for (Route* proute : *pRouteList) {
    for (RoutePoint* prp : *proute->pRoutePointList) {
      if (prp == pWP) nCount++;
    }
  }

  return nCount;
}

PlugIn_Route_ExV2::PlugIn_Route_ExV2() {
  pWaypointList = new Plugin_WaypointExV2List;
  m_GUID = wxEmptyString;
  m_NameString = wxEmptyString;
  m_StartString = wxEmptyString;
  m_EndString = wxEmptyString;
  m_isActive = false;
  m_isVisible = true;
  m_Description = wxEmptyString;

  // Generate a unique GUID if none provided
  if (m_GUID.IsEmpty()) {
    wxDateTime now = wxDateTime::Now();
    m_GUID = wxString::Format("RT%d%d%d%d", (int)now.GetMillisecond(),
                              (int)now.GetSecond(), (int)now.GetMinute(),
                              (int)now.GetHour());
  }
}

PlugIn_Route_ExV2::~PlugIn_Route_ExV2() {
  if (pWaypointList) {
    pWaypointList->DeleteContents(true);
    delete pWaypointList;
  }
}

// translate O route class to PlugIn_Waypoint_ExV2
static void PlugInExV2FromRoutePoint(PlugIn_Waypoint_ExV2* dst,
                                     /* const*/ RoutePoint* src) {
  dst->m_lat = src->m_lat;
  dst->m_lon = src->m_lon;
  dst->IconName = src->GetIconName();
  dst->m_MarkName = src->GetName();
  dst->m_MarkDescription = src->GetDescription();
  dst->IconDescription = pWayPointMan->GetIconDescription(src->GetIconName());
  dst->IsVisible = src->IsVisible();
  dst->m_CreateTime = src->GetCreateTime();  // not const
  dst->m_GUID = src->m_GUID;

  //  Transcribe (clone) the html HyperLink List, if present
  if (src->m_HyperlinkList) {
    delete dst->m_HyperlinkList;
    dst->m_HyperlinkList = nullptr;

    if (src->m_HyperlinkList->size() > 0) {
      dst->m_HyperlinkList = new Plugin_HyperlinkList;

      for (Hyperlink* link : *src->m_HyperlinkList) {
        Plugin_Hyperlink* h = new Plugin_Hyperlink();
        h->DescrText = link->DescrText;
        h->Link = link->Link;
        h->Type = link->LType;
        dst->m_HyperlinkList->Append(h);
      }
    }
  }

  // Get the range ring info
  dst->nrange_rings = src->m_iWaypointRangeRingsNumber;
  dst->RangeRingSpace = src->m_fWaypointRangeRingsStep;
  dst->RangeRingSpaceUnits = src->m_iWaypointRangeRingsStepUnits;
  dst->RangeRingColor = src->m_wxcWaypointRangeRingsColour;
  dst->m_TideStation = src->m_TideStation;

  // Get other extended info
  dst->IsNameVisible = src->m_bShowName;
  dst->scamin = src->GetScaMin();
  dst->b_useScamin = src->GetUseSca();
  dst->IsActive = src->m_bIsActive;

  dst->scamax = src->GetScaMax();
  dst->m_PlannedSpeed = src->GetPlannedSpeed();
  dst->m_ETD = src->GetManualETD();
  dst->m_WaypointArrivalRadius = src->GetWaypointArrivalRadius();
  dst->m_bShowWaypointRangeRings = src->GetShowWaypointRangeRings();
}

bool GetSingleWaypointExV2(wxString GUID, PlugIn_Waypoint_ExV2* pwaypoint) {
  //  Find the RoutePoint
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(GUID);

  if (!prp) return false;

  PlugInExV2FromRoutePoint(pwaypoint, prp);

  return true;
}

static void cloneHyperlinkListExV2(RoutePoint* dst,
                                   const PlugIn_Waypoint_ExV2* src) {
  //  Transcribe (clone) the html HyperLink List, if present
  if (src->m_HyperlinkList == nullptr) return;

  if (src->m_HyperlinkList->GetCount() > 0) {
    wxPlugin_HyperlinkListNode* linknode = src->m_HyperlinkList->GetFirst();
    while (linknode) {
      Plugin_Hyperlink* link = linknode->GetData();

      Hyperlink* h = new Hyperlink();
      h->DescrText = link->DescrText;
      h->Link = link->Link;
      h->LType = link->Type;

      dst->m_HyperlinkList->push_back(h);

      linknode = linknode->GetNext();
    }
  }
}

RoutePoint* CreateNewPoint(const PlugIn_Waypoint_ExV2* src, bool b_permanent) {
  RoutePoint* pWP = new RoutePoint(src->m_lat, src->m_lon, src->IconName,
                                   src->m_MarkName, src->m_GUID);

  pWP->m_bIsolatedMark = true;  // This is an isolated mark

  cloneHyperlinkListExV2(pWP, src);

  pWP->m_MarkDescription = src->m_MarkDescription;

  if (src->m_CreateTime.IsValid())
    pWP->SetCreateTime(src->m_CreateTime);
  else {
    pWP->SetCreateTime(wxDateTime::Now().ToUTC());
  }

  pWP->m_btemp = (b_permanent == false);

  // Extended fields
  pWP->SetIconName(src->IconName);
  pWP->SetWaypointRangeRingsNumber(src->nrange_rings);
  pWP->SetWaypointRangeRingsStep(src->RangeRingSpace);
  pWP->SetWaypointRangeRingsStepUnits(src->RangeRingSpaceUnits);
  pWP->SetWaypointRangeRingsColour(src->RangeRingColor);
  pWP->SetTideStation(src->m_TideStation);
  pWP->SetScaMin(src->scamin);
  pWP->SetUseSca(src->b_useScamin);
  pWP->SetNameShown(src->IsNameVisible);
  pWP->SetVisible(src->IsVisible);

  pWP->SetWaypointArrivalRadius(src->m_WaypointArrivalRadius);
  pWP->SetShowWaypointRangeRings(src->m_bShowWaypointRangeRings);
  pWP->SetScaMax(src->scamax);
  pWP->SetPlannedSpeed(src->m_PlannedSpeed);
  if (src->m_ETD.IsValid())
    pWP->SetETD(src->m_ETD);
  else
    pWP->SetETD(wxEmptyString);
  return pWP;
}

bool AddSingleWaypointExV2(PlugIn_Waypoint_ExV2* pwaypointex,
                           bool b_permanent) {
  //  Validate the waypoint parameters a little bit

  //  GUID
  //  Make sure that this GUID is indeed unique in the Routepoint list
  bool b_unique = true;
  for (RoutePoint* prp : *pWayPointMan->GetWaypointList()) {
    if (prp->m_GUID == pwaypointex->m_GUID) {
      b_unique = false;
      break;
    }
  }

  if (!b_unique) return false;

  RoutePoint* pWP = CreateNewPoint(pwaypointex, b_permanent);

  pWP->SetShowWaypointRangeRings(pwaypointex->nrange_rings > 0);

  pSelect->AddSelectableRoutePoint(pWP->m_lat, pWP->m_lon, pWP);
  if (b_permanent) {
    // pConfig->AddNewWayPoint(pWP, -1);
    NavObj_dB::GetInstance().InsertRoutePoint(pWP);
  }

  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateWptListCtrl();

  return true;
}

bool UpdateSingleWaypointExV2(PlugIn_Waypoint_ExV2* pwaypoint) {
  //  Find the RoutePoint
  bool b_found = false;
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(pwaypoint->m_GUID);

  if (prp) b_found = true;

  if (b_found) {
    double lat_save = prp->m_lat;
    double lon_save = prp->m_lon;

    prp->m_lat = pwaypoint->m_lat;
    prp->m_lon = pwaypoint->m_lon;
    prp->SetIconName(pwaypoint->IconName);
    prp->SetName(pwaypoint->m_MarkName);
    prp->m_MarkDescription = pwaypoint->m_MarkDescription;
    prp->SetVisible(pwaypoint->IsVisible);
    if (pwaypoint->m_CreateTime.IsValid())
      prp->SetCreateTime(pwaypoint->m_CreateTime);

    //  Transcribe (clone) the html HyperLink List, if present

    if (pwaypoint->m_HyperlinkList) {
      prp->m_HyperlinkList->clear();
      for (Plugin_Hyperlink* link : *pwaypoint->m_HyperlinkList) {
        Hyperlink* h = new Hyperlink();
        h->DescrText = link->DescrText;
        h->Link = link->Link;
        h->LType = link->Type;
        prp->m_HyperlinkList->push_back(h);
      }
    }

    // Extended fields
    prp->SetWaypointRangeRingsNumber(pwaypoint->nrange_rings);
    prp->SetWaypointRangeRingsStep(pwaypoint->RangeRingSpace);
    prp->SetWaypointRangeRingsStepUnits(pwaypoint->RangeRingSpaceUnits);
    prp->SetWaypointRangeRingsColour(pwaypoint->RangeRingColor);
    prp->SetTideStation(pwaypoint->m_TideStation);
    prp->SetScaMin(pwaypoint->scamin);
    prp->SetUseSca(pwaypoint->b_useScamin);
    prp->SetNameShown(pwaypoint->IsNameVisible);

    prp->SetShowWaypointRangeRings(pwaypoint->nrange_rings > 0);

    if (prp) prp->ReLoadIcon();

    auto canvas = gFrame->GetPrimaryCanvas();
    SelectCtx ctx(canvas->m_bShowNavobjects, canvas->GetCanvasTrueScale(),
                  canvas->GetScaleValue());
    SelectItem* pFind =
        pSelect->FindSelection(ctx, lat_save, lon_save, SELTYPE_ROUTEPOINT);
    if (pFind) {
      pFind->m_slat = pwaypoint->m_lat;  // update the SelectList entry
      pFind->m_slon = pwaypoint->m_lon;
    }

    if (!prp->m_btemp) {
      // pConfig->UpdateWayPoint(prp);
      NavObj_dB::GetInstance().UpdateRoutePoint(prp);
    }

    if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
      pRouteManagerDialog->UpdateWptListCtrl();

    prp->SetPlannedSpeed(pwaypoint->m_PlannedSpeed);
    if (pwaypoint->m_ETD.IsValid())
      prp->SetETD(pwaypoint->m_ETD);
    else
      prp->SetETD(wxEmptyString);
    prp->SetWaypointArrivalRadius(pwaypoint->m_WaypointArrivalRadius);
    prp->SetShowWaypointRangeRings(pwaypoint->m_bShowWaypointRangeRings);
    prp->SetScaMax(pwaypoint->scamax);
  }

  return b_found;
}

std::unique_ptr<PlugIn_Waypoint_ExV2> GetWaypointExV2_Plugin(
    const wxString& GUID) {
  std::unique_ptr<PlugIn_Waypoint_ExV2> w(new PlugIn_Waypoint_ExV2);
  GetSingleWaypointExV2(GUID, w.get());
  return w;
}

// PlugIn_Route_ExV2 utilities

bool AddPlugInRouteExV2(PlugIn_Route_ExV2* proute, bool b_permanent) {
  Route* route = new Route();

  PlugIn_Waypoint_ExV2* pwaypointex;
  RoutePoint *pWP, *pWP_src;
  int ip = 0;
  wxDateTime plannedDeparture;

  wxPlugin_WaypointExV2ListNode* pwpnode = proute->pWaypointList->GetFirst();
  while (pwpnode) {
    pwaypointex = pwpnode->GetData();

    pWP = pWayPointMan->FindRoutePointByGUID(pwaypointex->m_GUID);
    if (!pWP) {
      pWP = CreateNewPoint(pwaypointex, b_permanent);
      pWP->m_bIsolatedMark = false;
    }

    route->AddPoint(pWP);

    pSelect->AddSelectableRoutePoint(pWP->m_lat, pWP->m_lon, pWP);

    if (ip > 0)
      pSelect->AddSelectableRouteSegment(pWP_src->m_lat, pWP_src->m_lon,
                                         pWP->m_lat, pWP->m_lon, pWP_src, pWP,
                                         route);

    plannedDeparture = pwaypointex->m_CreateTime;
    ip++;
    pWP_src = pWP;

    pwpnode = pwpnode->GetNext();  // PlugInWaypoint
  }

  route->m_PlannedDeparture = plannedDeparture;

  route->m_RouteNameString = proute->m_NameString;
  route->m_RouteStartString = proute->m_StartString;
  route->m_RouteEndString = proute->m_EndString;
  if (!proute->m_GUID.IsEmpty()) {
    route->m_GUID = proute->m_GUID;
  }
  route->m_btemp = (b_permanent == false);
  route->SetVisible(proute->m_isVisible);
  route->m_RouteDescription = proute->m_Description;

  pRouteList->push_back(route);

  if (b_permanent) {
    // pConfig->AddNewRoute(route);
    NavObj_dB::GetInstance().InsertRoute(route);
  }

  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateRouteListCtrl();

  return true;
}

bool UpdatePlugInRouteExV2(PlugIn_Route_ExV2* proute) {
  bool b_found = false;

  // Find the Route
  Route* pRoute = g_pRouteMan->FindRouteByGUID(proute->m_GUID);
  if (pRoute) b_found = true;

  if (b_found) {
    bool b_permanent = !pRoute->m_btemp;
    g_pRouteMan->DeleteRoute(pRoute);

    b_found = AddPlugInRouteExV2(proute, b_permanent);
  }

  return b_found;
}

std::unique_ptr<PlugIn_Route_ExV2> GetRouteExV2_Plugin(const wxString& GUID) {
  std::unique_ptr<PlugIn_Route_ExV2> r;
  Route* route = g_pRouteMan->FindRouteByGUID(GUID);
  if (route == nullptr) return r;

  r = std::unique_ptr<PlugIn_Route_ExV2>(new PlugIn_Route_ExV2);
  PlugIn_Route_ExV2* dst_route = r.get();

  for (RoutePoint* src_wp : *route->pRoutePointList) {
    PlugIn_Waypoint_ExV2* dst_wp = new PlugIn_Waypoint_ExV2();
    PlugInExV2FromRoutePoint(dst_wp, src_wp);
    dst_route->pWaypointList->Append(dst_wp);
  }
  dst_route->m_NameString = route->m_RouteNameString;
  dst_route->m_StartString = route->m_RouteStartString;
  dst_route->m_EndString = route->m_RouteEndString;
  dst_route->m_GUID = route->m_GUID;
  dst_route->m_isActive = g_pRouteMan->GetpActiveRoute() == route;
  dst_route->m_isVisible = route->IsVisible();
  dst_route->m_Description = route->m_RouteDescription;

  return r;
}

//      PlugInRouteExtended implementation
PlugIn_Route_Ex::PlugIn_Route_Ex() {
  pWaypointList = new Plugin_WaypointExList;
}

PlugIn_Route_Ex::~PlugIn_Route_Ex() {
  pWaypointList->DeleteContents(false);  // do not delete Waypoints
  pWaypointList->Clear();

  delete pWaypointList;
}

//  The utility methods implementations

// translate O route class to PlugIn_Waypoint_Ex
static void PlugInExFromRoutePoint(PlugIn_Waypoint_Ex* dst,
                                   /* const*/ RoutePoint* src) {
  dst->m_lat = src->m_lat;
  dst->m_lon = src->m_lon;
  dst->IconName = src->GetIconName();
  dst->m_MarkName = src->GetName();
  dst->m_MarkDescription = src->GetDescription();
  dst->IconDescription = pWayPointMan->GetIconDescription(src->GetIconName());
  dst->IsVisible = src->IsVisible();
  dst->m_CreateTime = src->GetCreateTime();  // not const
  dst->m_GUID = src->m_GUID;

  //  Transcribe (clone) the html HyperLink List, if present
  if (src->m_HyperlinkList) {
    delete dst->m_HyperlinkList;
    dst->m_HyperlinkList = nullptr;

    if (src->m_HyperlinkList->size() > 0) {
      dst->m_HyperlinkList = new Plugin_HyperlinkList;
      for (Hyperlink* link : *src->m_HyperlinkList) {
        Plugin_Hyperlink* h = new Plugin_Hyperlink();
        h->DescrText = link->DescrText;
        h->Link = link->Link;
        h->Type = link->LType;
        dst->m_HyperlinkList->Append(h);
      }
    }
  }

  // Get the range ring info
  dst->nrange_rings = src->m_iWaypointRangeRingsNumber;
  dst->RangeRingSpace = src->m_fWaypointRangeRingsStep;
  dst->RangeRingColor = src->m_wxcWaypointRangeRingsColour;

  // Get other extended info
  dst->IsNameVisible = src->m_bShowName;
  dst->scamin = src->GetScaMin();
  dst->b_useScamin = src->GetUseSca();
  dst->IsActive = src->m_bIsActive;
}

static void cloneHyperlinkListEx(RoutePoint* dst,
                                 const PlugIn_Waypoint_Ex* src) {
  //  Transcribe (clone) the html HyperLink List, if present
  if (src->m_HyperlinkList == nullptr) return;

  if (src->m_HyperlinkList->GetCount() > 0) {
    wxPlugin_HyperlinkListNode* linknode = src->m_HyperlinkList->GetFirst();
    while (linknode) {
      Plugin_Hyperlink* link = linknode->GetData();

      Hyperlink* h = new Hyperlink();
      h->DescrText = link->DescrText;
      h->Link = link->Link;
      h->LType = link->Type;

      dst->m_HyperlinkList->push_back(h);

      linknode = linknode->GetNext();
    }
  }
}

RoutePoint* CreateNewPoint(const PlugIn_Waypoint_Ex* src, bool b_permanent) {
  RoutePoint* pWP = new RoutePoint(src->m_lat, src->m_lon, src->IconName,
                                   src->m_MarkName, src->m_GUID);

  pWP->m_bIsolatedMark = true;  // This is an isolated mark

  cloneHyperlinkListEx(pWP, src);

  pWP->m_MarkDescription = src->m_MarkDescription;

  if (src->m_CreateTime.IsValid())
    pWP->SetCreateTime(src->m_CreateTime);
  else {
    pWP->SetCreateTime(wxDateTime::Now().ToUTC());
  }

  pWP->m_btemp = (b_permanent == false);

  // Extended fields
  pWP->SetIconName(src->IconName);
  pWP->SetWaypointRangeRingsNumber(src->nrange_rings);
  pWP->SetWaypointRangeRingsStep(src->RangeRingSpace);
  pWP->SetWaypointRangeRingsColour(src->RangeRingColor);
  pWP->SetScaMin(src->scamin);
  pWP->SetUseSca(src->b_useScamin);
  pWP->SetNameShown(src->IsNameVisible);
  pWP->SetVisible(src->IsVisible);

  return pWP;
}
bool GetSingleWaypointEx(wxString GUID, PlugIn_Waypoint_Ex* pwaypoint) {
  //  Find the RoutePoint
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(GUID);

  if (!prp) return false;

  PlugInExFromRoutePoint(pwaypoint, prp);

  return true;
}

bool AddSingleWaypointEx(PlugIn_Waypoint_Ex* pwaypointex, bool b_permanent) {
  //  Validate the waypoint parameters a little bit

  //  GUID
  //  Make sure that this GUID is indeed unique in the Routepoint list
  bool b_unique = true;
  for (RoutePoint* prp : *pWayPointMan->GetWaypointList()) {
    if (prp->m_GUID == pwaypointex->m_GUID) {
      b_unique = false;
      break;
    }
  }

  if (!b_unique) return false;

  RoutePoint* pWP = CreateNewPoint(pwaypointex, b_permanent);

  pWP->SetShowWaypointRangeRings(pwaypointex->nrange_rings > 0);

  pSelect->AddSelectableRoutePoint(pWP->m_lat, pWP->m_lon, pWP);
  if (b_permanent) {
    // pConfig->AddNewWayPoint(pWP, -1);
    NavObj_dB::GetInstance().InsertRoutePoint(pWP);
  }
  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateWptListCtrl();

  return true;
}

bool UpdateSingleWaypointEx(PlugIn_Waypoint_Ex* pwaypoint) {
  //  Find the RoutePoint
  bool b_found = false;
  RoutePoint* prp = pWayPointMan->FindRoutePointByGUID(pwaypoint->m_GUID);

  if (prp) b_found = true;

  if (b_found) {
    double lat_save = prp->m_lat;
    double lon_save = prp->m_lon;

    prp->m_lat = pwaypoint->m_lat;
    prp->m_lon = pwaypoint->m_lon;
    prp->SetIconName(pwaypoint->IconName);
    prp->SetName(pwaypoint->m_MarkName);
    prp->m_MarkDescription = pwaypoint->m_MarkDescription;
    prp->SetVisible(pwaypoint->IsVisible);
    if (pwaypoint->m_CreateTime.IsValid())
      prp->SetCreateTime(pwaypoint->m_CreateTime);

    //  Transcribe (clone) the html HyperLink List, if present

    if (pwaypoint->m_HyperlinkList) {
      prp->m_HyperlinkList->clear();
      if (pwaypoint->m_HyperlinkList->GetCount() > 0) {
        wxPlugin_HyperlinkListNode* linknode =
            pwaypoint->m_HyperlinkList->GetFirst();
        while (linknode) {
          Plugin_Hyperlink* link = linknode->GetData();

          Hyperlink* h = new Hyperlink();
          h->DescrText = link->DescrText;
          h->Link = link->Link;
          h->LType = link->Type;

          prp->m_HyperlinkList->push_back(h);

          linknode = linknode->GetNext();
        }
      }
    }

    // Extended fields
    prp->SetWaypointRangeRingsNumber(pwaypoint->nrange_rings);
    prp->SetWaypointRangeRingsStep(pwaypoint->RangeRingSpace);
    prp->SetWaypointRangeRingsColour(pwaypoint->RangeRingColor);
    prp->SetScaMin(pwaypoint->scamin);
    prp->SetUseSca(pwaypoint->b_useScamin);
    prp->SetNameShown(pwaypoint->IsNameVisible);

    prp->SetShowWaypointRangeRings(pwaypoint->nrange_rings > 0);

    if (prp) prp->ReLoadIcon();

    auto canvas = gFrame->GetPrimaryCanvas();
    SelectCtx ctx(canvas->m_bShowNavobjects, canvas->GetCanvasTrueScale(),
                  canvas->GetScaleValue());
    SelectItem* pFind =
        pSelect->FindSelection(ctx, lat_save, lon_save, SELTYPE_ROUTEPOINT);
    if (pFind) {
      pFind->m_slat = pwaypoint->m_lat;  // update the SelectList entry
      pFind->m_slon = pwaypoint->m_lon;
    }

    if (!prp->m_btemp) {
      // pConfig->UpdateWayPoint(prp);
      NavObj_dB::GetInstance().UpdateRoutePoint(prp);
    }

    if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
      pRouteManagerDialog->UpdateWptListCtrl();
  }

  return b_found;
}

bool AddPlugInRouteEx(PlugIn_Route_Ex* proute, bool b_permanent) {
  Route* route = new Route();

  PlugIn_Waypoint_Ex* pwaypointex;
  RoutePoint *pWP, *pWP_src;
  int ip = 0;
  wxDateTime plannedDeparture;

  wxPlugin_WaypointExListNode* pwpnode = proute->pWaypointList->GetFirst();
  while (pwpnode) {
    pwaypointex = pwpnode->GetData();

    pWP = pWayPointMan->FindRoutePointByGUID(pwaypointex->m_GUID);
    if (!pWP) {
      pWP = CreateNewPoint(pwaypointex, b_permanent);
      pWP->m_bIsolatedMark = false;
    }

    route->AddPoint(pWP);

    pSelect->AddSelectableRoutePoint(pWP->m_lat, pWP->m_lon, pWP);

    if (ip > 0)
      pSelect->AddSelectableRouteSegment(pWP_src->m_lat, pWP_src->m_lon,
                                         pWP->m_lat, pWP->m_lon, pWP_src, pWP,
                                         route);

    plannedDeparture = pwaypointex->m_CreateTime;
    ip++;
    pWP_src = pWP;

    pwpnode = pwpnode->GetNext();  // PlugInWaypoint
  }

  route->m_PlannedDeparture = plannedDeparture;

  route->m_RouteNameString = proute->m_NameString;
  route->m_RouteStartString = proute->m_StartString;
  route->m_RouteEndString = proute->m_EndString;
  if (!proute->m_GUID.IsEmpty()) {
    route->m_GUID = proute->m_GUID;
  }
  route->m_btemp = (b_permanent == false);
  route->SetVisible(proute->m_isVisible);
  route->m_RouteDescription = proute->m_Description;

  pRouteList->push_back(route);

  if (b_permanent) {
    // pConfig->AddNewRoute(route);
    NavObj_dB::GetInstance().InsertRoute(route);
  }

  if (pRouteManagerDialog && pRouteManagerDialog->IsShown())
    pRouteManagerDialog->UpdateRouteListCtrl();

  return true;
}

bool UpdatePlugInRouteEx(PlugIn_Route_Ex* proute) {
  bool b_found = false;

  //  Find the Route
  Route* pRoute = g_pRouteMan->FindRouteByGUID(proute->m_GUID);
  if (pRoute) b_found = true;

  if (b_found) {
    bool b_permanent = !pRoute->m_btemp;
    g_pRouteMan->DeleteRoute(pRoute);
    b_found = AddPlugInRouteEx(proute, b_permanent);
  }

  return b_found;
}

std::unique_ptr<PlugIn_Waypoint_Ex> GetWaypointEx_Plugin(const wxString& GUID) {
  std::unique_ptr<PlugIn_Waypoint_Ex> w(new PlugIn_Waypoint_Ex);
  GetSingleWaypointEx(GUID, w.get());
  return w;
}

std::unique_ptr<PlugIn_Route_Ex> GetRouteEx_Plugin(const wxString& GUID) {
  std::unique_ptr<PlugIn_Route_Ex> r;
  Route* route = g_pRouteMan->FindRouteByGUID(GUID);
  if (route == nullptr) return r;

  r = std::unique_ptr<PlugIn_Route_Ex>(new PlugIn_Route_Ex);
  PlugIn_Route_Ex* dst_route = r.get();

  // PlugIn_Waypoint *pwp;
  RoutePoint* src_wp;
  for (RoutePoint* src_wp : *route->pRoutePointList) {
    PlugIn_Waypoint_Ex* dst_wp = new PlugIn_Waypoint_Ex();
    PlugInExFromRoutePoint(dst_wp, src_wp);

    dst_route->pWaypointList->Append(dst_wp);
  }
  dst_route->m_NameString = route->m_RouteNameString;
  dst_route->m_StartString = route->m_RouteStartString;
  dst_route->m_EndString = route->m_RouteEndString;
  dst_route->m_GUID = route->m_GUID;
  dst_route->m_isActive = g_pRouteMan->GetpActiveRoute() == route;
  dst_route->m_isVisible = route->IsVisible();
  dst_route->m_Description = route->m_RouteDescription;

  return r;
}

wxString GetActiveWaypointGUID(
    void) {  // if no active waypoint, returns wxEmptyString
  RoutePoint* rp = g_pRouteMan->GetpActivePoint();
  if (!rp)
    return wxEmptyString;
  else
    return rp->m_GUID;
}

wxString GetActiveRouteGUID(
    void) {  // if no active route, returns wxEmptyString
  Route* rt = g_pRouteMan->GetpActiveRoute();
  if (!rt)
    return wxEmptyString;
  else
    return rt->m_GUID;
}

/** Comm Global Watchdog Query  */
int GetGlobalWatchdogTimoutSeconds() { return gps_watchdog_timeout_ticks; }

/** Comm Priority query support methods  */
std::vector<std::string> GetPriorityMaps() {
  return (CommBridge::GetInstance().GetPriorityMaps());
}

void UpdateAndApplyPriorityMaps(std::vector<std::string> map) {
  CommBridge::GetInstance().UpdateAndApplyMaps(map);
}

std::vector<std::string> GetActivePriorityIdentifiers() {
  std::vector<std::string> result;

  auto& comm_bridge = CommBridge::GetInstance();

  std::string id = comm_bridge.GetPriorityContainer("position").active_source;
  result.push_back(id);
  id = comm_bridge.GetPriorityContainer("velocity").active_source;
  result.push_back(id);
  id = comm_bridge.GetPriorityContainer("heading").active_source;
  result.push_back(id);
  id = comm_bridge.GetPriorityContainer("variation").active_source;
  result.push_back(id);
  id = comm_bridge.GetPriorityContainer("satellites").active_source;
  result.push_back(id);

  return result;
}

double OCPN_GetDisplayContentScaleFactor() {
  double rv = 1.0;
#if defined(__WXOSX__) || defined(__WXGTK3__)
  // Support scaled HDPI displays.
  if (gFrame) rv = gFrame->GetContentScaleFactor();
#endif
  return rv;
}
double OCPN_GetWinDIPScaleFactor() {
  double scaler = 1.0;
#ifdef __WXMSW__
  if (gFrame) scaler = (double)(gFrame->ToDIP(100)) / 100.;
#endif
  return scaler;
}

//---------------------------------------------------------------------------
//    API 1.18
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//    API 1.19
//---------------------------------------------------------------------------
void ExitOCPN() {}

bool GetFullScreen() { return gFrame->IsFullScreen(); }

void SetFullScreen(bool set_full_screen_on) {
  bool state = gFrame->IsFullScreen();
  if (set_full_screen_on && !state)
    gFrame->ToggleFullScreen();
  else if (!set_full_screen_on && state)
    gFrame->ToggleFullScreen();
}

extern bool g_useMUI;
void EnableMUIBar(bool enable, int CanvasIndex) {
  bool current_mui_state = g_useMUI;

  g_useMUI = enable;
  if (enable && !current_mui_state) {  // OFF going ON
    // ..For each canvas...
    for (unsigned int i = 0; i < g_canvasArray.GetCount(); i++) {
      ChartCanvas* cc = g_canvasArray.Item(i);
      if (cc) cc->CreateMUIBar();
    }
  } else if (!enable && current_mui_state) {  // ON going OFF
    // ..For each canvas...
    for (unsigned int i = 0; i < g_canvasArray.GetCount(); i++) {
      ChartCanvas* cc = g_canvasArray.Item(i);
      if (cc) cc->DestroyMuiBar();
    }
  }
}

bool GetEnableMUIBar(int CanvasIndex) { return g_useMUI; }

void EnableCompassGPSIcon(bool enable, int CanvasIndex) {
  g_bShowCompassWin = enable;
}

bool GetEnableCompassGPSIcon(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc)
      return cc->GetShowGPSCompassWindow();
    else
      return false;
  }
  return false;
}

extern bool g_bShowStatusBar;
void EnableStatusBar(bool enable) {
  g_bShowStatusBar = enable;
  gFrame->ConfigureStatusBar();
}

bool GetEnableStatusBar() { return g_bShowStatusBar; }

void EnableChartBar(bool enable, int CanvasIndex) {
  bool current_chartbar_state = g_bShowChartBar;
  for (unsigned int i = 0; i < g_canvasArray.GetCount(); i++) {
    ChartCanvas* cc = g_canvasArray.Item(i);
    if (current_chartbar_state && !enable) {
      gFrame->ToggleChartBar(cc);
      g_bShowChartBar = current_chartbar_state;
    } else if (!current_chartbar_state && enable) {
      gFrame->ToggleChartBar(cc);
      g_bShowChartBar = current_chartbar_state;
    }
  }
  g_bShowChartBar = enable;
}

bool GetEnableChartBar(int CanvasIndex) { return g_bShowChartBar; }

extern bool g_bShowMenuBar;
void EnableMenu(bool enable) {
  if (!enable) {
    if (g_bShowMenuBar) {
      g_bShowMenuBar = false;
      if (gFrame->m_pMenuBar) {
        gFrame->SetMenuBar(NULL);
        gFrame->m_pMenuBar->Destroy();
        gFrame->m_pMenuBar = NULL;
      }
    }
  } else {
    g_bShowMenuBar = true;
    gFrame->BuildMenuBar();
  }
}

bool GetEnableMenu() { return g_bShowMenuBar; }

void SetGlobalColor(std::string table, std::string name, wxColor color) {
  if (ps52plib) ps52plib->m_chartSymbols.UpdateTableColor(table, name, color);
}

wxColor GetGlobalColorD(std::string map_name, std::string name) {
  wxColor ret = wxColor(*wxRED);
  if (ps52plib) {
    int i_table = ps52plib->m_chartSymbols.FindColorTable(map_name.c_str());
    ret = ps52plib->m_chartSymbols.GetwxColor(name.c_str(), i_table);
  }
  return ret;
}

void EnableLatLonGrid(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowGrid(enable);
  }
}

void EnableChartOutlines(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowOutlines(enable);
  }
}

void EnableDepthUnitDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowDepthUnits(enable);
  }
}

void EnableAisTargetDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowAIS(enable);
  }
}

void EnableTideStationsDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->ShowTides(enable);
  }
}

void EnableCurrentStationsDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->ShowCurrents(enable);
  }
}

void EnableENCTextDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowENCText(enable);
  }
}

void EnableENCDepthSoundingsDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowENCDepth(enable);
  }
}

void EnableBuoyLightLabelsDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowENCBuoyLabels(enable);
  }
}

void EnableLightsDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowENCLights(enable);
  }
}

void EnableLightDescriptionsDisplay(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowENCLightDesc(enable);
  }
}

void SetENCDisplayCategory(PI_DisCat cat, int CanvasIndex) {
  int valSet = STANDARD;
  switch (cat) {
    case PI_DISPLAYBASE:
      valSet = DISPLAYBASE;
      break;
    case PI_STANDARD:
      valSet = STANDARD;
      break;
    case PI_OTHER:
      valSet = OTHER;
      break;
    case PI_MARINERS_STANDARD:
      valSet = MARINERS_STANDARD;
      break;
    default:
      valSet = STANDARD;
      break;
  }
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetENCDisplayCategory(valSet);
  }
}
PI_DisCat GetENCDisplayCategory(int CanvasIndex) {
  ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
  if (cc)
    return ((PI_DisCat)cc->GetENCDisplayCategory());
  else
    return PI_DisCat::PI_STANDARD;
}

void SetNavigationMode(PI_NavMode mode, int CanvasIndex) {
  int newMode = NORTH_UP_MODE;
  if (mode == PI_COURSE_UP_MODE)
    newMode = COURSE_UP_MODE;
  else if (mode == PI_HEAD_UP_MODE)
    newMode = HEAD_UP_MODE;

  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetUpMode(newMode);
  }
}
PI_NavMode GetNavigationMode(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return ((PI_NavMode)cc->GetUpMode());
  }
  return PI_NavMode::PI_NORTH_UP_MODE;
}

bool GetEnableLatLonGrid(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowGrid());
  }
  return false;
}

bool GetEnableChartOutlines(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowOutlines());
  }
  return false;
}

bool GetEnableDepthUnitDisplay(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowDepthUnits());
  }
  return false;
}

bool GetEnableAisTargetDisplay(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowAIS());
  }
  return false;
}

bool GetEnableTideStationsDisplay(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetbShowTide());
  }
  return false;
}

bool GetEnableCurrentStationsDisplay(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetbShowCurrent());
  }
  return false;
}

bool GetEnableENCTextDisplay(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowENCText());
  }
  return false;
}

bool GetEnableENCDepthSoundingsDisplay(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowENCDepth());
  }
  return false;
}

bool GetEnableBuoyLightLabelsDisplay(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowENCBuoyLabels());
  }
  return false;
}

bool GetEnableLightsDisplay(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowENCLights());
  }
  return false;
}

bool GetShowENCLightDesc(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetbShowCurrent());
  }
  return false;
}

void EnableTouchMode(bool enable) { g_btouch = enable; }

bool GetTouchMode() { return g_btouch; }

void EnableLookaheadMode(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->ToggleLookahead();
  }
}

bool GetEnableLookaheadMode(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetLookahead());
  }
  return false;
}

extern bool g_bTrackActive;
void SetTrackingMode(bool enable) {
  if (!g_bTrackActive && enable)
    gFrame->TrackOn();
  else if (g_bTrackActive && !enable)
    gFrame->TrackOff();
}
bool GetTrackingMode() { return g_bTrackActive; }

void SetAppColorScheme(PI_ColorScheme cs) {
  gFrame->SetAndApplyColorScheme((ColorScheme)cs);
}
PI_ColorScheme GetAppColorScheme() {
  return (PI_ColorScheme)global_color_scheme;
}

void RequestWindowRefresh(wxWindow* win, bool eraseBackground) {
  if (win) win->Refresh(eraseBackground);
}

void EnableSplitScreenLayout(bool enable) {
  if (g_canvasConfig == 1) {
    if (enable)
      return;
    else {                 // split to single
      g_canvasConfig = 0;  // 0 => "single canvas"
      gFrame->CreateCanvasLayout();
      gFrame->DoChartUpdate();
    }
  } else {
    if (enable) {          // single to split
      g_canvasConfig = 1;  // 1 => "two canvas"
      gFrame->CreateCanvasLayout();
      gFrame->DoChartUpdate();
    } else {
      return;
    }
  }
}

// ChartCanvas control utilities

void PluginZoomCanvas(int CanvasIndex, double factor) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->ZoomCanvasSimple(factor);
  }
}

bool GetEnableMainToolbar() { return (!g_disable_main_toolbar); }
void SetEnableMainToolbar(bool enable) {
  g_disable_main_toolbar = !enable;
  if (g_MainToolbar) g_MainToolbar->RefreshToolbar();
}

void ShowGlobalSettingsDialog() {
  if (gFrame) gFrame->ScheduleSettingsDialog();
}

void PluginCenterOwnship(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) {
      bool bfollow = cc->GetbFollow();
      cc->ResetOwnshipOffset();
      if (bfollow)
        cc->SetbFollow();
      else
        cc->JumpToPosition(gLat, gLon, cc->GetVPScale());
    }
  }
}

void PluginSetFollowMode(int CanvasIndex, bool enable_follow) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) {
      if (cc->GetbFollow() != enable_follow) cc->TogglebFollow();
    }
  }
}

bool PluginGetFollowMode(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return cc->GetbFollow();
  }
  return false;
}

void EnableCanvasFocusBar(bool enable, int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) cc->SetShowFocusBar(enable);
  }
}
bool GetEnableCanvasFocusBar(int CanvasIndex) {
  if (CanvasIndex < GetCanvasCount()) {
    ChartCanvas* cc = g_canvasArray.Item(CanvasIndex);
    if (cc) return (cc->GetShowFocusBar());
  }
  return false;
}

bool GetEnableTenHertzUpdate() { return g_btenhertz; }

void EnableTenHertzUpdate(bool enable) { g_btenhertz = enable; }

void ConfigFlushAndReload() {
  if (pConfig) {
    // Store current locale to detect changes
    wxString oldLocale = g_locale;
    pConfig->Flush();

    // Handle system general configuration options
    pConfig->LoadMyConfigRaw(false);

    // Handle S57 configuration options
    pConfig->LoadS57Config();

    // Handle chart canvas window configuration options
    pConfig->LoadCanvasConfigs(false);
    auto& config_array = ConfigMgr::Get().GetCanvasConfigArray();
    for (auto pcc : config_array) {
      if (pcc && pcc->canvas) {
        pcc->canvas->ApplyCanvasConfig(pcc);
        pcc->canvas->Refresh();
      }
    }

#if wxUSE_XLOCALE
    // Detect and apply locale changes
    if (g_locale != oldLocale && !g_locale.IsEmpty()) {
      wxLogMessage("ConfigFlushAndReload: Locale changed, applying...");
      g_Platform->ChangeLocale(g_locale, plocale_def_lang, &plocale_def_lang);
      ApplyLocale();  // Deactivates/reactivates plugins, rebuilds UI
    }
#endif
  }
}

/**
 * Plugin Notification Framework GUI support
 */
void EnableNotificationCanvasIcon(bool enable) {
  g_CanvasHideNotificationIcon = !enable;
}
