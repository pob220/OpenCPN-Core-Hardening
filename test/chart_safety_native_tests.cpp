/* GPL-2.0-or-later. Private GUI/core integration harness.
 * Compiled in place of ocpn_plugin_gui.cpp by the prototype qualification
 * script, so tests exercise its private production classifier without adding
 * test exports or changing the plugin ABI. Linked against the complete host. */
#include <gtest/gtest.h>
#include "../gui/src/ocpn_plugin_gui.cpp"
#include "mygeom.h"
#include <wx/init.h>
#include <wx/log.h>

// Provider plugins supply this constructor; the host uses borrowed PI objects.
// This fixture owns the native attributes and initialises only the ABI fields
// used by its semantic visitor.
PI_S57Obj::PI_S57Obj() : att_array(nullptr), attVal(nullptr), n_attr(0) {
  FeatureName[0] = 0;
}

namespace {
class FixtureChart : public cm93chart {
public:
  FixtureChart() {
    m_RAZBuilt = true;
    s_segment_safety_point_cache.clear();
  }
  ~FixtureChart() {
    for (int i = 0; i < PRIO_NUM; ++i)
      for (int j = 0; j < LUPNAME_NUM; ++j) razRules[i][j] = nullptr;
  }
  S57Obj *Add(const char *name, int primitive, int style = 4) {
    auto obj = std::make_unique<S57Obj>(name);
    obj->Primitive_type = static_cast<GeoPrim_t>(primitive);
    obj->Scamin = 1;
    obj->m_DisplayCat = OTHER;  // deliberately hidden at normal scale
    auto rule = std::unique_ptr<ObjRazRules>(new ObjRazRules{});
    rule->obj = obj.get();
    rule->next = razRules[0][style];
    razRules[0][style] = rule.get();
    auto *result = obj.get();
    objects.push_back(std::move(obj));
    rules.push_back(std::move(rule));
    return result;
  }
  void Point(S57Obj *obj, double lat = .0254, double lon = .0254) {
    obj->npt = 1;
    obj->x_rate = obj->y_rate = 1;
    obj->x_origin = obj->y_origin = 0;
    toSM(lat, lon, ref_lat, ref_lon, &obj->x, &obj->y);
    obj->BBObj.Set(lat, lon, lat, lon);
  }
  void Line(S57Obj *obj) {
    obj->npt = 2;
    obj->geoPt = static_cast<pt *>(calloc(2, sizeof(pt)));
    obj->x_rate = obj->y_rate = 1;
    obj->x_origin = obj->y_origin = 0;
    double x, y;
    toSM(.0254, .024, ref_lat, ref_lon, &x, &y);
    obj->geoPt[0].x = x;
    obj->geoPt[0].y = y;
    toSM(.0254, .027, ref_lat, ref_lon, &x, &y);
    obj->geoPt[1].x = x;
    obj->geoPt[1].y = y;
    obj->BBObj.Set(.0254, .024, .0254, .027);
  }
  void Triangle(S57Obj *obj, bool ocean = false) {
    const double lats[3] = {ocean ? -1.0 : .0252, ocean ? -1.0 : .0252,
                            ocean ? 2.0 : .0255};
    const double lons[3] = {ocean ? -1.0 : .0252, ocean ? 2.0 : .0255,
                            ocean ? -1.0 : .0252};
    obj->pPolyTessGeo = new PolyTessGeo;
    auto *group = new PolyTriGroup;
    group->m_bSMSENC = true;
    group->data_type = DATA_TYPE_DOUBLE;
    auto *triangle = new TriPrim;
    triangle->p_next = nullptr;
    triangle->type = PTG_TRIANGLES;
    triangle->nVert = 3;
    triangle->p_vertex = static_cast<double *>(calloc(6, sizeof(double)));
    for (int i = 0; i < 3; ++i)
      toSM(lats[i], lons[i], ref_lat, ref_lon, &triangle->p_vertex[2 * i],
           &triangle->p_vertex[2 * i + 1]);
    group->tri_prim_head = triangle;
    obj->pPolyTessGeo->Set_PolyTriGroup_head(group);
    obj->pPolyTessGeo->Set_OK(true);
    obj->BBObj.Set(
        *std::min_element(lats, lats + 3), *std::min_element(lons, lons + 3),
        *std::max_element(lats, lats + 3), *std::max_element(lons, lons + 3));
  }
  void Coverage(double minlat, double minlon, double maxlat, double maxlon) {
    auto c = std::make_unique<M_COVR_Desc>();
    c->m_nvertices = 4;
    c->pvertices = new float_2Dpt[4];
    c->pvertices[0].x = minlon;
    c->pvertices[0].y = minlat;
    c->pvertices[1].x = maxlon;
    c->pvertices[1].y = minlat;
    c->pvertices[2].x = maxlon;
    c->pvertices[2].y = maxlat;
    c->pvertices[3].x = minlon;
    c->pvertices[3].y = maxlat;
    m_pcovr_array_loaded.Add(c.get());
    coverage.push_back(std::move(c));
  }
  std::vector<std::unique_ptr<M_COVR_Desc>> coverage;
  void Ocean() {
    auto *obj = Add("DEPARE", GEO_AREA);
    Triangle(obj, true);
    obj->AddDoubleAttribute("DRVAL1", 1000);
  }
  std::vector<std::unique_ptr<S57Obj>> objects;
  std::vector<std::unique_ptr<ObjRazRules>> rules;
};
PlugInSegmentSafetyResult Classify(FixtureChart &chart) {
  auto rules = CollectCm93SafetyTileRules(&chart, 0, 0);
  ViewPort vp;
  PlugInSegmentSafetyResult result{};
  result.struct_size = sizeof(result);
  InitSegmentSafetyResult(&result);
  PlugInSegmentSafetySource source;
  auto cls = ChartPointSafetyClassAtPreparedCm93(
      &chart, 123, .025, .025, &vp, &source, nullptr, &result, rules);
  result.status = cls;
  return result;
}
TEST(ChartSafetyNative, CoverageProofUsesCompleteOriginalPolygons) {
  FixtureChart chart;
  chart.Coverage(-1, 359, 2, 362);
  EXPECT_EQ(chart.SafetyCoverageBoxRelation(.024, .026, .024, .026), 2);
  EXPECT_EQ(chart.SafetyCoverageBoxRelation(-1.1, -.9, .024, .026), 1);
  EXPECT_EQ(chart.SafetyCoverageBoxRelation(10, 11, .024, .026), 0);
  chart.coverage[0]->user_xoff = 1;
  EXPECT_EQ(chart.SafetyCoverageBoxRelation(.024, .026, .024, .026), -1);
}
TEST(ChartSafetyNative, DistantCoverageAcrossAntimeridianDoesNotHideLocalProof) {
  FixtureChart chart;
  chart.Coverage(-1, -1, 1, 1);
  chart.Coverage(-1, 179, 1, 181);
  EXPECT_EQ(chart.SafetyCoverageBoxRelation(.024, .026, .024, .026), 2);
  EXPECT_EQ(chart.SafetyCoverageBoxRelation(.024, .026, 179.4, 179.5), 2);
  EXPECT_EQ(chart.SafetyCoverageBoxRelation(.024, .026, -179.5, -179.4), 2);
  EXPECT_EQ(chart.SafetyCoverageBoxRelation(.024, .026, 178.9, 179.1), 1);
}
TEST(ChartSafetyNative,
     ExtractedHazardGeometrySurvivesChartMutationAndDestruction) {
  std::shared_ptr<const ocpn::chart_safety::PreparedGeometry> geometry;
  {
    FixtureChart chart;
    auto *danger = chart.Add("WRECKS", GEO_AREA);
    chart.Triangle(danger);
    geometry = chart.PrepareSafetyObjectGeometry(danger);
    danger->pPolyTessGeo->Get_PolyTriGroup_head()->tri_prim_head->p_vertex[0] =
        1e10;
  }
  EXPECT_EQ(s57chart::PreparedSafetyBoxRelation(*geometry, .024, .026, .024,
                                                .026, false),
            1);
}
TEST(ChartSafetyNative, DeepUniformAreaHasGeometricProof) {
  FixtureChart chart;
  chart.Ocean();
  double minimum = 0;
  EXPECT_TRUE(Cm93UniformDepthProof(CollectCm93SafetyTileRules(&chart, 0, 0),
                                    &minimum));
  EXPECT_DOUBLE_EQ(minimum, 1000);
}
TEST(ChartSafetyNative, GeographicPrefilterCannotHideIsolatedDangers) {
  for (auto name : {"UWTROC", "WRECKS", "OBSTRN"})
    for (auto primitive : {GEO_POINT, GEO_LINE, GEO_AREA}) {
      FixtureChart chart;
      auto *danger = chart.Add(name, primitive);
      if (primitive == GEO_POINT)
        chart.Point(danger);
      else if (primitive == GEO_LINE)
        chart.Line(danger);
      else
        chart.Triangle(danger);
      EXPECT_TRUE(s57chart::SafetyObjectMayIntersectBox(danger, .024, .026,
                                                        .024, .026));
      EXPECT_EQ(s57chart::SafetyObjectMayIntersectBox(danger, 10, 11, 10, 11),
                primitive == GEO_POINT);
      // Point bounds are not authoritative. An inconsistent point box must
      // not suppress the actual nearby coordinate.
      danger->BBObj.Set(10, 10, 11, 11);
      if (primitive == GEO_POINT) {
        EXPECT_TRUE(s57chart::SafetyObjectMayIntersectBox(danger, .024, .026,
                                                          .024, .026));
      }
    }
  S57Obj unresolved("OBSTRN");
  unresolved.Primitive_type = GEO_AREA;
  EXPECT_TRUE(s57chart::SafetyObjectMayIntersectBox(&unresolved, .024, .026,
                                                    .024, .026));
  unresolved.BBObj.Set(.02, 359.98, .03, 360.03);
  EXPECT_TRUE(s57chart::SafetyObjectMayIntersectBox(&unresolved, .024, .026,
                                                    -.01, .01));
}
TEST(ChartSafetyNative, EveryIsolatedDangerPrimitiveOverridesDeepWater) {
  for (auto name : {"UWTROC", "WRECKS", "OBSTRN"})
    for (auto primitive : {GEO_POINT, GEO_LINE, GEO_AREA}) {
      SCOPED_TRACE(std::string(name) +
                   " primitive=" + std::to_string(primitive));
      FixtureChart chart;
      chart.Ocean();
      auto *danger = chart.Add(name, primitive,
                               primitive == GEO_POINT  ? 1
                               : primitive == GEO_LINE ? 2
                                                       : 4);
      if (primitive == GEO_POINT)
        chart.Point(danger);
      else if (primitive == GEO_LINE)
        chart.Line(danger);
      else
        chart.Triangle(danger);
      danger->AddDoubleAttribute("VALSOU", 1.0);
      auto result = Classify(chart);
      EXPECT_EQ(result.status, SEGMENT_SAFETY_POINT_WATER);
      EXPECT_EQ(result.has_depth, 1);
      EXPECT_DOUBLE_EQ(result.min_depth_m, 1);
      EXPECT_NE(std::string(result.depth_source_attribute).find("VALSOU"),
                std::string::npos);
      double minimum = 0;
      EXPECT_FALSE(Cm93UniformDepthProof(
          CollectCm93SafetyTileRules(&chart, 0, 0), &minimum));
      auto *raw = chart.GetSafetyRulesAtLatLon(.025, .025, .001);
      ASSERT_NE(raw, nullptr);
      EXPECT_GE(raw->GetCount(), 2u);
      delete raw;
    }
}
TEST(ChartSafetyNative, UnknownDangerDepthCannotBorrowDeepSurroundings) {
  for (auto name : {"UWTROC", "WRECKS", "OBSTRN"})
    for (auto primitive : {GEO_POINT, GEO_LINE, GEO_AREA}) {
      SCOPED_TRACE(std::string(name) +
                   " primitive=" + std::to_string(primitive));
      FixtureChart chart;
      chart.Ocean();
      auto *danger = chart.Add(name, primitive,
                               primitive == GEO_POINT  ? 1
                               : primitive == GEO_LINE ? 2
                                                       : 4);
      if (primitive == GEO_POINT)
        chart.Point(danger);
      else if (primitive == GEO_LINE)
        chart.Line(danger);
      else
        chart.Triangle(danger);
      auto result = Classify(chart);
      EXPECT_EQ(result.has_depth, 0);
      EXPECT_EQ(result.status, SEGMENT_SAFETY_POINT_NO_DATA);
      EXPECT_NE(std::string(result.depth_source_attribute).find("unresolved"),
                std::string::npos);
    }
}
TEST(ChartSafetyNative, DeepKnownRockStillForbidsWholeTileShortcut) {
  FixtureChart chart;
  chart.Ocean();
  auto *rock = chart.Add("UWTROC", GEO_POINT, 0);
  chart.Point(rock);
  rock->AddDoubleAttribute("VALSOU", 50);
  double minimum = 0;
  EXPECT_FALSE(Cm93UniformDepthProof(CollectCm93SafetyTileRules(&chart, 0, 0),
                                     &minimum));
  EXPECT_DOUBLE_EQ(Classify(chart).min_depth_m, 50);
}
TEST(ChartSafetyNative, TinyLandAndDryingAreasBetweenSamplesAreRetained) {
  for (bool land : {true, false}) {
    FixtureChart chart;
    chart.Ocean();
    auto *hazard = chart.Add(land ? "LNDARE" : "OBSTRN", GEO_AREA);
    chart.Triangle(hazard);
    if (!land) hazard->AddIntegerAttribute("WATLEV", 4);
    EXPECT_EQ(Classify(chart).status,
              land ? SEGMENT_SAFETY_POINT_LAND : SEGMENT_SAFETY_POINT_DRYING);
  }
}
TEST(ChartSafetyNative, TinyShallowDepthAreaBetweenSamplesIsRetained) {
  FixtureChart chart;
  chart.Ocean();
  auto *reef = chart.Add("DEPARE", GEO_AREA);
  chart.Triangle(reef);
  reef->AddDoubleAttribute("DRVAL1", .5);
  EXPECT_DOUBLE_EQ(Classify(chart).min_depth_m, .5);
  double minimum = 0;
  EXPECT_FALSE(Cm93UniformDepthProof(CollectCm93SafetyTileRules(&chart, 0, 0),
                                     &minimum));
}
TEST(ChartSafetyNative, AllAreaTriangleEncodingsRetainIsolatedDangers) {
  for (auto name : {"UWTROC", "WRECKS", "OBSTRN"})
    for (auto type : {PTG_TRIANGLES, PTG_TRIANGLE_FAN, PTG_TRIANGLE_STRIP})
      for (bool use_float : {false, true}) {
        FixtureChart chart;
        chart.Ocean();
        auto *danger = chart.Add(name, GEO_AREA);
        chart.Triangle(danger);
        danger->AddDoubleAttribute("VALSOU", .25);
        auto *group = danger->pPolyTessGeo->Get_PolyTriGroup_head();
        auto *triangle = group->tri_prim_head;
        triangle->type = type;
        if (use_float) {
          auto *vertices = static_cast<float *>(calloc(6, sizeof(float)));
          for (int i = 0; i < 6; ++i) vertices[i] = triangle->p_vertex[i];
          free(triangle->p_vertex);
          triangle->p_vertex = reinterpret_cast<double *>(vertices);
          group->data_type = DATA_TYPE_FLOAT;
        }
        EXPECT_DOUBLE_EQ(Classify(chart).min_depth_m, .25);
      }
}
TEST(ChartSafetyNative, MissingDangerGeometryCannotBecomeClearWater) {
  FixtureChart chart;
  chart.Ocean();
  auto *danger = chart.Add("OBSTRN", GEO_AREA);
  danger->BBObj.Set(.025, .025, .026, .026);
  EXPECT_EQ(Classify(chart).status, SEGMENT_SAFETY_POINT_NO_DATA);
}
TEST(ChartSafetyNative, DateLinePointAndAreaUseQueryLongitudeBranch) {
  FixtureChart chart;
  auto *point = chart.Add("UWTROC", GEO_POINT, 1);
  chart.Point(point, .025, 179.9999);
  EXPECT_GT(
      chart.SafetyObjectBoxRelation(point, .024, .026, -180.001, -179.999), 0);
  auto *area = chart.Add("WRECKS", GEO_AREA);
  chart.Triangle(area, true);
  // Shift a native chart to its equivalent 0..360 branch.
  area->x_origin = 360 * DEGREE * WGS84_semimajor_axis_meters * mercator_k0;
  area->x_rate = area->y_rate = 1;
  area->y_origin = 0;
  auto *group = area->pPolyTessGeo->Get_PolyTriGroup_head();
  group->m_bSMSENC = false;
  area->BBObj.Set(-1, 359, 2, 362);
  EXPECT_EQ(chart.SafetyObjectBoxRelation(area, .024, .026, .024, .026), 2);
}
TEST(ChartSafetyNative, ShallowSoundingOverridesAreaButCannotCertifyCoverage) {
  FixtureChart chart;
  chart.Ocean();
  auto *obj = chart.Add("SOUNDG", GEO_POINT, 1);
  obj->npt = 1;
  obj->geoPtMulti = static_cast<double *>(calloc(2, sizeof(double)));
  obj->geoPtz = static_cast<double *>(calloc(3, sizeof(double)));
  obj->geoPtMulti[0] = .0254;
  obj->geoPtMulti[1] = .0254;
  obj->geoPtz[2] = .75;
  auto result = Classify(chart);
  EXPECT_EQ(result.has_depth, 1);
  EXPECT_DOUBLE_EQ(result.min_depth_m, .75);
  double minimum = 0;
  EXPECT_FALSE(Cm93UniformDepthProof(CollectCm93SafetyTileRules(&chart, 0, 0),
                                     &minimum));
}
TEST(ChartSafetyNative, NonFiniteDangerDepthRemainsUnknown) {
  FixtureChart chart;
  chart.Ocean();
  auto *rock = chart.Add("UWTROC", GEO_POINT, 1);
  chart.Point(rock);
  rock->AddDoubleAttribute("VALSOU", std::numeric_limits<double>::quiet_NaN());
  EXPECT_EQ(Classify(chart).has_depth, 0);
}
}  // namespace
extern "C" int __wrap_main(int argc, char **argv) {
  wxApp::SetInstance(new wxApp());
  if (!wxEntryStart(argc, argv)) return 2;
  delete wxLog::SetActiveTarget(new wxLogStderr());
  ::testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();
  wxEntryCleanup();
  return result;
}

TEST(ChartSafetyNative, DeepSoundingsCannotWeakenUniformAreaMinimum) {
  FixtureChart chart;
  chart.Ocean();
  auto *obj = chart.Add("SOUNDG", GEO_POINT, 1);
  obj->npt = 1;
  obj->geoPtMulti = static_cast<double *>(calloc(2, sizeof(double)));
  obj->geoPtz = static_cast<double *>(calloc(3, sizeof(double)));
  obj->geoPtMulti[0] = .0254;
  obj->geoPtMulti[1] = .0254;
  obj->geoPtz[2] = 1500;
  double minimum = 0;
  EXPECT_TRUE(Cm93UniformDepthProof(CollectCm93SafetyTileRules(&chart, 0, 0),
                                    &minimum));
  EXPECT_DOUBLE_EQ(minimum, 1000);
  obj->geoPtz[2] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(Cm93UniformDepthProof(CollectCm93SafetyTileRules(&chart, 0, 0),
                                     &minimum));
}

TEST(ChartSafetyNative, PluginProviderVisitorRetainsEveryIsolatedDangerClass) {
  for (auto name : {"UWTROC", "WRECKS", "OBSTRN"})
    for (int primitive : {1, 2, 3})
      for (bool known : {false, true}) {
        S57Obj ocean("DEPARE");
        ocean.AddDoubleAttribute("DRVAL1", 1000);
        S57Obj danger(name);
        if (known) danger.AddDoubleAttribute("VALSOU", 1);
        SegmentSafetyPluginBatchGroup group(1);
        group.active[0] = 1;
        const uint64_t hits = 1;
        auto visit = [&](S57Obj &source) {
          PI_S57Obj object;
          memcpy(object.FeatureName, source.FeatureName, 6);
          object.FeatureName[6] = 0;
          object.Primitive_type = primitive;
          object.att_array = source.att_array;
          object.attVal = source.attVal;
          object.n_attr = source.n_attr;
          SegmentSafetyPluginBatchVisit(&group, &object, &hits, 1);
        };
        visit(ocean);
        visit(danger);
        EXPECT_EQ(group.unknown_danger_depth[0], known ? 0 : 1);
        EXPECT_FLOAT_EQ(group.min_depth_m[0], known ? 1 : 1000);
      }
}

namespace {
// Immutable chart fixtures exercise the production traversal and clearance
// dilation, without relying on a proprietary chart set in automated tests.
class DateLineSafety : public ::testing::Test {
protected:
  void SetUp() override {
    s_segment_safety_grid_cache.clear();
    s_segment_safety_route_mask_cache.clear();
    s_segment_safety_coarse_route_mask_cache.clear();
  }
  void WaterTiles() {
    for (long lat = -377; lat <= -374; ++lat)
      for (long lon : {3598L, 3599L, -3600L, -3599L}) {
        CachedPointSafetyGridTile tile;
        tile.built = true;
        tile.depth_complete = true;
        tile.group_index = SegmentSafetyCurrentGroupIndex();
        tile.lat_tile = lat;
        tile.lon_tile = lon;
        tile.min_lat = lat * .05;
        tile.min_lon = lon * .05;
        tile.resolution = .00125;
        tile.rows = tile.cols = 41;
        tile.source = PI_SEGMENT_SAFETY_SOURCE_CM93;
        tile.classes.assign(1681, SEGMENT_SAFETY_POINT_WATER);
        tile.hazard_flags.assign(1681, 0);
        tile.has_depth.assign(1681, 1);
        tile.min_depth_m.assign(1681, 200);
        tile.has_drying.assign(1681, 0);
        StoreSegmentSafetyGridTile(SegmentSafetyGridTileKeyForIndices(lat, lon),
                                   tile);
      }
  }
};
TEST_F(DateLineSafety, ClearanceHaloCrossesTheSeamAndRetainsEveryHazard) {
  for (bool west : {false, true})
    for (double margin : {.1, .4})
      for (int hazard = -1; hazard < 5; ++hazard) {
        SetUp();
        WaterTiles();
        const long target_lon = west ? 3599 : -3600;
        const long neighbour_lon = west ? -3600 : 3599;
        auto key = SegmentSafetyGridTileKeyForIndices(-376, neighbour_lon);
        auto tile = s_segment_safety_grid_cache.at(key);
        const int index = 20 * 41 + (west ? 0 : 39);
        if (hazard == 0) {
          tile.classes[index] = SEGMENT_SAFETY_POINT_LAND;
          tile.hazard_flags[index] = SEGMENT_SAFETY_HAZARD_LAND;
        }
        if (hazard == 1) {
          tile.classes[index] = SEGMENT_SAFETY_POINT_DRYING;
          tile.hazard_flags[index] = SEGMENT_SAFETY_HAZARD_DRYING;
        }
        if (hazard == 2) tile.min_depth_m[index] = 1;
        StoreSegmentSafetyGridTile(key, tile);
        // Unknown/no-chart evidence blocks the traversed cell. Existing
        // clearance policy dilates known land/drying/shallow hazards only.
        if (hazard >= 3) {
          key = SegmentSafetyGridTileKeyForIndices(-376, target_lon);
          tile = s_segment_safety_grid_cache.at(key);
          const int centre = 20 * 41 + (west ? 39 : 0);
          if (hazard == 3) tile.has_depth[centre] = 0;
          if (hazard == 4) {
            tile.classes[centre] = SEGMENT_SAFETY_POINT_NO_DATA;
            tile.hazard_flags[centre] = SEGMENT_SAFETY_HAZARD_NO_CHART;
          }
          StoreSegmentSafetyGridTile(key, tile);
        }
        SegmentSafetyCoreStats stats;
        const auto mask = BuildSegmentSafetyRouteMaskTile(
            -376, target_lon, margin, true, 1.5, &stats);
        ASSERT_TRUE(mask.built);
        const int target = 20 * 41 + (west ? 39 : 0);
        EXPECT_EQ(mask.block_flags[target] == SEGMENT_SAFETY_ROUTE_CLEAR,
                  hazard < 0)
            << "west=" << west << " margin=" << margin << " hazard=" << hazard;
        EXPECT_EQ(stats.unexpected_tile_builds, 0);
      }
}
TEST_F(DateLineSafety,
       PublicCrossingsAreBoundedAndKeepHazardsInBothDirections) {
  for (bool reverse : {false, true})
    for (bool blocked : {false, true}) {
      SetUp();
      for (long lon : {3599L, -3600L}) {
        CachedSegmentSafetyRouteMaskTile mask;
        mask.built = true;
        mask.authoritative_fine = true;
        mask.lat_tile = -376;
        mask.lon_tile = lon;
        mask.min_lat = -18.8;
        mask.min_lon = lon * .05;
        mask.resolution = .00125;
        mask.rows = mask.cols = 41;
        mask.source = PI_SEGMENT_SAFETY_SOURCE_CM93;
        mask.block_flags.assign(1681, SEGMENT_SAFETY_ROUTE_CLEAR);
        if (blocked && lon == -3600)
          mask.block_flags[24 * 41] = SEGMENT_SAFETY_ROUTE_BLOCK_LAND;
        StoreSegmentSafetyRouteMaskTile(
            SegmentSafetyRouteMaskKey(-376, lon, 0, true, 1.5), mask);
      }
      PlugInSegmentSafetyOptions options{};
      options.struct_size = sizeof(options);
      options.check_land = options.check_depth = 1;
      options.minimum_depth_m = 1.5;
      options.force_authoritative_fine_validation = 1;
      PlugInSegmentSafetyResult result{};
      result.struct_size = sizeof(result);
      ASSERT_TRUE(PlugIn_CheckSegmentSafety(-18.77, reverse ? -179.99 : 179.99,
                                            -18.77, reverse ? 179.99 : -179.99,
                                            &options, &result));
      EXPECT_EQ(result.status, blocked ? PI_SEGMENT_SAFETY_CROSSES_LAND
                                       : PI_SEGMENT_SAFETY_SAFE);
      EXPECT_LT(result.hit_sample_count, 30);
    }
}
TEST_F(DateLineSafety, CorridorFootprintUsesOnlyAdjacentCanonicalTiles) {
  for (bool reverse : {false, true}) {
    std::set<std::pair<long, long>> tiles;
    AddSegmentSafetyRouteMaskCorridorTiles(-18.77, reverse ? -179.99 : 179.99,
                                           -18.77, reverse ? 179.99 : -179.99,
                                           0, &tiles);
    EXPECT_EQ(tiles.size(), 2u);
    EXPECT_EQ(tiles.count({-376, 3599}), 1u);
    EXPECT_EQ(tiles.count({-376, -3600}), 1u);
  }
}
}  // namespace

TEST(ChartSafetyNative, OpenWaterRegionRequiresCompleteDepthArea) {
  FixtureChart chart;
  chart.Ocean();
  EXPECT_DOUBLE_EQ(
      OpenWaterDepthLowerBound(CollectSafetyBoxRules(&chart, 0, 0, .2, .02)),
      1000.0);
  // Same depths on part of a region cannot prove the remaining area.
  EXPECT_DOUBLE_EQ(
      OpenWaterDepthLowerBound(CollectSafetyBoxRules(&chart, 0, 0, 1, .02)),
      0.0);
}

TEST(ChartSafetyNative, OpenWaterRegionRejectsAnIsolatedDangerInItsHalo) {
  FixtureChart chart;
  chart.Ocean();
  auto* rock = chart.Add("UWTROC", GEO_POINT, 1);
  chart.Point(rock, .21, .1);
  rock->AddDoubleAttribute("VALSOU", 6.0);
  EXPECT_DOUBLE_EQ(
      OpenWaterDepthLowerBound(CollectSafetyBoxRules(&chart, 0, 0, .2, .02)),
      0.0);
}

TEST(ChartSafetyNative, InstalledOpenWaterProofIsAvailableWithoutPrewarm) {
  OpenWaterRegions previous;
  previous.swap(s_segment_safety_open_water_regions);
  const bool previous_requested = s_segment_safety_open_water_requested;
  s_segment_safety_open_water_requested = false;
  s_segment_safety_open_water_regions.emplace(
      SegmentSafetyOpenWaterKey(kOpenWaterSubregionTiles, 4, 4),
      SegmentSafetyOpenWaterRegion{PI_SEGMENT_SAFETY_SOURCE_CM93, 100.0});

  SegmentSafetyOpenWaterRegion proof;
  EXPECT_TRUE(SegmentSafetyOpenWaterForChord(.23, .23, .35, .35, .4, true,
                                             5.0, &proof));
  EXPECT_EQ(proof.source, PI_SEGMENT_SAFETY_SOURCE_CM93);
  EXPECT_TRUE(PrepareSegmentSafetyOpenWaterRegion(5, 5, .4, true, 5.0));
  EXPECT_FALSE(PrepareSegmentSafetyOpenWaterRegion(5, 5, .4, true, 100.0));
  EXPECT_FALSE(PrepareSegmentSafetyOpenWaterRegion(5, 9, .4, true, 5.0));
  EXPECT_EQ(s_segment_safety_open_water_regions.size(), 1u);

  s_segment_safety_open_water_regions.swap(previous);
  s_segment_safety_open_water_requested = previous_requested;
}

TEST(ChartSafetyNative, OpenWaterCrossingsNeedEvidenceForEveryTouchedRegion) {
  s_segment_safety_open_water_regions.clear();
  s_segment_safety_open_water_requested = true;
  const auto add = [](long lon, double depth) {
    s_segment_safety_open_water_regions.emplace(
        SegmentSafetyOpenWaterKey(kOpenWaterRegionTiles, 0, lon),
        SegmentSafetyOpenWaterRegion{PI_SEGMENT_SAFETY_SOURCE_CM93, depth});
  };
  add(0, 100.0);
  SegmentSafetyOpenWaterRegion proof;
  EXPECT_TRUE(SegmentSafetyOpenWaterForChord(.25, .25, .75, .75, .4, true,
                                             5.0, &proof));
  EXPECT_FALSE(SegmentSafetyOpenWaterForChord(.5, .5, .5, 1.5, .4, true,
                                              5.0, &proof));
  add(20, 60.0);
  EXPECT_TRUE(SegmentSafetyOpenWaterForChord(.5, .5, .5, 1.5, .4, true,
                                             5.0, &proof));
  EXPECT_DOUBLE_EQ(proof.minimum_depth_m, 60.0);
  EXPECT_FALSE(SegmentSafetyOpenWaterForChord(.5, .5, .5, 1.5, .4, true,
                                              70.0, &proof));
  EXPECT_FALSE(SegmentSafetyOpenWaterForChord(.5, .5, .5, 1.5, 2.5, false,
                                              0.0, &proof));
  EXPECT_FALSE(SegmentSafetyOpenWaterForChord(.5, 179.99, .5, 180.01, .4,
                                              false, 0.0, &proof));
  s_segment_safety_open_water_regions.clear();
  s_segment_safety_open_water_requested = false;
}

TEST(ChartTileHelperCodec, RoundTripRetainsHazardsDepthAndDatelineIdentity) {
  CachedPointSafetyGridTile tile;
  tile.group_index = SegmentSafetyCurrentGroupIndex();
  tile.lat_tile = -400; tile.lon_tile = -3600;
  tile.rows = tile.cols = 41;
  tile.source = PI_SEGMENT_SAFETY_SOURCE_CM93;
  snprintf(tile.chart_path, sizeof(tile.chart_path), "CM93.A");
  snprintf(tile.dependency_identity, sizeof(tile.dependency_identity), "fixture-dependency");
  tile.hazard_flags.assign(1681, SEGMENT_SAFETY_HAZARD_NONE);
  tile.has_depth.assign(1681, 1); tile.min_depth_m.assign(1681, 40.f);
  tile.hazard_flags[7] = SEGMENT_SAFETY_HAZARD_LAND;
  tile.hazard_flags[8] = SEGMENT_SAFETY_HAZARD_DRYING;
  std::stringstream stream;
  ASSERT_TRUE(WriteHelperTile(stream, tile));
  CachedPointSafetyGridTile decoded;
  ASSERT_TRUE(ReadHelperTile(stream, &decoded));
  EXPECT_EQ(decoded.lon_tile, -3600);
  EXPECT_EQ(decoded.land_count, 1);
  EXPECT_EQ(decoded.drying_count, 1);
  EXPECT_EQ(decoded.min_depth_m, tile.min_depth_m);
  EXPECT_EQ(decoded.hazard_flags, tile.hazard_flags);
  EXPECT_STREQ(decoded.dependency_identity, "fixture-dependency");
  tile.hazard_flags[10] = 0x8000;
  std::stringstream invalid;
  ASSERT_TRUE(WriteHelperTile(invalid, tile));
  EXPECT_FALSE(ReadHelperTile(invalid, &decoded));
  tile.hazard_flags[10] = 0;
  tile.source = PI_SEGMENT_SAFETY_SOURCE_PLUGIN_VECTOR;
  std::stringstream unsupported;
  ASSERT_TRUE(WriteHelperTile(unsupported, tile));
  EXPECT_FALSE(ReadHelperTile(unsupported, &decoded));
  tile.persistent_cache_allowed = true;
  std::stringstream permitted;
  ASSERT_TRUE(WriteHelperTile(permitted, tile));
  EXPECT_TRUE(ReadHelperTile(permitted, &decoded));
  EXPECT_TRUE(decoded.persistent_cache_allowed);
  std::stringstream truncated(stream.str().substr(0, 200));
  EXPECT_FALSE(ReadHelperTile(truncated, &decoded));
}

#if defined(__linux__) && !defined(__ANDROID__)
TEST(ChartTileHelpers, RejectsStaleCatalogueWithoutPublishingClearWater) {
  StopSegmentSafetyTileHelpers();
  const auto root = wxFileName::CreateTempFileName("chart-helper-reject-");
  wxRemoveFile(root); ASSERT_TRUE(wxFileName::Mkdir(root));
  s_tile_helpers.root = root;
  s_tile_helpers.identity = s_segment_safety_chart_identity;
  s_tile_helpers.local_catalogue = s_segment_safety_chart_catalog_identity;
  s_tile_helpers.catalogue = "expected-catalogue";
  s_tile_helpers.group = SegmentSafetyCurrentGroupIndex();
  s_tile_helpers.memory_checked = std::chrono::steady_clock::now();
  s_tile_helpers.lanes.push_back({root,0,{{-600,-2600}},std::chrono::steady_clock::now()});
  const auto before = s_segment_safety_grid_cache.size();
  {
    std::ofstream out((root+"/response.bin").ToStdString(),std::ios::binary);
    ASSERT_TRUE(SegmentSafetyWriteBinary(out,uint32_t{0x54494c31u}));
    ASSERT_TRUE(SegmentSafetyWriteBinaryString(out,s_tile_helpers.identity.ToStdString()));
    ASSERT_TRUE(SegmentSafetyWriteBinaryString(out,"stale-catalogue"));
    ASSERT_TRUE(SegmentSafetyWriteBinary(out,uint32_t{0}));
  }
  PollSegmentSafetyTileHelpers();
  EXPECT_TRUE(s_tile_helpers.disabled);
  EXPECT_EQ(s_tile_helpers.unsupported.count({-600,-2600}),1u);
  EXPECT_EQ(s_segment_safety_grid_cache.size(),before);
  StopSegmentSafetyTileHelpers();
}

TEST(ChartTileHelpers, MissingRequiredDepthFallsBackInsteadOfRequeueingForever) {
  StopSegmentSafetyTileHelpers();
  const auto root = wxFileName::CreateTempFileName("chart-helper-depth-");
  wxRemoveFile(root); ASSERT_TRUE(wxFileName::Mkdir(root));
  s_tile_helpers.root = root;
  s_tile_helpers.identity = s_segment_safety_chart_identity;
  s_tile_helpers.local_catalogue = s_segment_safety_chart_catalog_identity;
  s_tile_helpers.catalogue = "same-catalogue";
  s_tile_helpers.group = SegmentSafetyCurrentGroupIndex();
  s_tile_helpers.memory_checked = std::chrono::steady_clock::now();
  s_tile_helpers.lanes.push_back({root,0,{{-600,-2600}},std::chrono::steady_clock::now(),true});
  CachedPointSafetyGridTile tile;
  tile.group_index = SegmentSafetyCurrentGroupIndex();
  tile.lat_tile=-600;tile.lon_tile=-2600;tile.rows=tile.cols=41;
  tile.source=PI_SEGMENT_SAFETY_SOURCE_CM93;tile.depth_complete=false;
  snprintf(tile.chart_path,sizeof(tile.chart_path),"CM93.A");
  snprintf(tile.dependency_identity,sizeof(tile.dependency_identity),"depth-unproven");
  tile.hazard_flags.assign(1681,0);tile.has_depth.assign(1681,0);tile.min_depth_m.assign(1681,0);
  const auto before=s_segment_safety_grid_cache.size();
  {
    std::ofstream out((root+"/response.bin").ToStdString(),std::ios::binary);
    ASSERT_TRUE(SegmentSafetyWriteBinary(out,uint32_t{0x54494c31u}));
    ASSERT_TRUE(SegmentSafetyWriteBinaryString(out,s_tile_helpers.identity.ToStdString()));
    ASSERT_TRUE(SegmentSafetyWriteBinaryString(out,"same-catalogue"));
    ASSERT_TRUE(SegmentSafetyWriteBinary(out,uint32_t{1}));
    ASSERT_TRUE(WriteHelperTile(out,tile));
  }
  PollSegmentSafetyTileHelpers();
  EXPECT_EQ(s_tile_helpers.imported,0u);
  EXPECT_EQ(s_tile_helpers.unsupported.count({-600,-2600}),1u);
  EXPECT_FALSE(TileHelperPending(-600,-2600));
  EXPECT_EQ(s_segment_safety_grid_cache.size(),before);
  StopSegmentSafetyTileHelpers();
}
#endif

TEST(ChartTileHelpers, AdmissionRequiresRepeatedExpensiveLocalBuilds) {
  TileHelperCostSamples costs;
  EXPECT_FALSE(costs.WorthOffloading());
  costs.Add(600); costs.Add(1); costs.Add(2); costs.Add(1);
  EXPECT_FALSE(costs.WorthOffloading());
  for (int n=0; n<8; ++n) costs.Add(30);
  EXPECT_TRUE(costs.WorthOffloading());
  for (int n=0; n<8; ++n) costs.Add(2);
  EXPECT_FALSE(costs.WorthOffloading());
}
TEST(ChartTileHelpers, AdmissionSeparatesDepthRegionsAndCatalogue) {
  s_tile_helper_costs.clear();
  for (int n=0; n<4; ++n) RecordTileHelperBuildCost(-1,-1,true,30);
  EXPECT_TRUE(TileHelperCostAllows(-2,-2,true));
  EXPECT_FALSE(TileHelperCostAllows(0,-2,true));
  EXPECT_FALSE(TileHelperCostAllows(-2,-2,false));
  const auto original = s_segment_safety_chart_catalog_identity;
  s_segment_safety_chart_catalog_identity += "-changed";
  EXPECT_FALSE(TileHelperCostAllows(-2,-2,true));
  s_segment_safety_chart_catalog_identity = original;
  RefreshTileHelperCostIdentity();
}
#if defined(__linux__) && !defined(__ANDROID__)
TEST(ChartTileHelpers, CachedLiveMaskDoesNotProbeUnrelatedPendingRequests) {
  StopSegmentSafetyTileHelpers();
  auto saved_grid = std::move(s_segment_safety_grid_cache);
  auto saved_pending = std::move(s_segment_safety_pending_route_mask_requests);
  const auto saved_callbacks = s_segment_safety_external_tile_cache;
  const bool saved_enabled = s_tile_helpers_enabled;
  const unsigned saved_limit = s_tile_helper_process_limit;
  const auto saved_prepared = s_tile_helper_prepared;
  wxString saved_flag;
  const bool had_flag = wxGetEnv("OCPN_CHART_TILE_HELPERS", &saved_flag);
  wxSetEnv("OCPN_CHART_TILE_HELPERS", "1");
  s_tile_helpers_enabled = true;
  s_tile_helper_process_limit = 2;
  int lookups = 0;
  s_segment_safety_external_tile_cache = {};
  s_segment_safety_external_tile_cache.context = &lookups;
  s_segment_safety_external_tile_cache.lookup =
      [](void* context, long, long, int, PlugInSegmentSafetyTile*) {
        ++*static_cast<int*>(context); return 0;
      };
  SegmentSafetyRouteMaskRequest request;
  request.lat_tile = 700; request.lon_tile = 2000;
  request.safety_margin_nm = 0.4; request.check_depth = false;
  for (int dy=-1; dy<=1; ++dy) for (int dx=-1; dx<=1; ++dx) {
    CachedPointSafetyGridTile tile; tile.built = true;
    s_segment_safety_grid_cache[SegmentSafetyGridTileKeyForIndices(700+dy,2000+dx)] = tile;
  }
  for (int n=0; n<32; ++n) {
    auto other = request; other.lat_tile = -600+n; other.lon_tile = -2000;
    s_segment_safety_pending_route_mask_requests[std::to_string(n)] = other;
  }
  EXPECT_FALSE(DeferSegmentSafetyTileRequest(request));
  EXPECT_EQ(lookups, 0);
  EXPECT_TRUE(s_tile_helpers.lanes.empty());
  s_segment_safety_grid_cache.erase(SegmentSafetyGridTileKeyForIndices(700,2000));
  s_tile_helpers.lanes.push_back({"",0,{{700,2000}},std::chrono::steady_clock::now()});
  EXPECT_TRUE(DeferSegmentSafetyTileRequest(request));
  EXPECT_EQ(lookups, 0);
  wxSetEnv("OCPN_CHART_TILE_HELPERS", "auto");
  EXPECT_FALSE(DeferSegmentSafetyTileRequest(request));
  EXPECT_EQ(lookups, 0);
  wxSetEnv("OCPN_CHART_TILE_HELPERS", "1");
  bool depth = true;
  const auto keys = BackgroundTileHelperKeys(&depth);
  EXPECT_FALSE(depth);
  EXPECT_FALSE(keys.empty());
  EXPECT_LE(keys.size(),18u);
  EXPECT_EQ(lookups,0);
  if (!keys.empty()) {
    const auto first = *keys.begin();
    s_tile_helper_prepared.emplace(SegmentSafetyCurrentGroupIndex(),first.first,first.second);
    EXPECT_EQ(BackgroundTileHelperKeys(&depth).count(first),0u);
  }
  s_tile_helper_prepared = saved_prepared;
  PlugIn_SetSegmentSafetyHelperProcessLimit(0);
  EXPECT_TRUE(s_tile_helpers.lanes.empty());
  EXPECT_FALSE(DeferSegmentSafetyTileRequest(request));
  s_tile_helper_process_limit = saved_limit;
  s_segment_safety_grid_cache = std::move(saved_grid);
  s_segment_safety_pending_route_mask_requests = std::move(saved_pending);
  s_segment_safety_external_tile_cache = saved_callbacks;
  s_tile_helpers_enabled = saved_enabled;
  if (had_flag) wxSetEnv("OCPN_CHART_TILE_HELPERS", saved_flag);
  else wxUnsetEnv("OCPN_CHART_TILE_HELPERS");
}
#endif

namespace {
struct GridEvictionFixture {
  decltype(s_segment_safety_grid_cache) grid=std::move(s_segment_safety_grid_cache);
  decltype(s_segment_safety_pinned_grid_keys) pins=std::move(s_segment_safety_pinned_grid_keys);
  decltype(s_segment_safety_pinned_route_mask_keys) masks=std::move(s_segment_safety_pinned_route_mask_keys);
  decltype(s_segment_safety_grid_eviction) index=std::move(s_segment_safety_grid_eviction);
  decltype(s_segment_safety_external_tile_cache) callbacks=s_segment_safety_external_tile_cache;
  bool persistent=s_segment_safety_persistent_cache_enabled;
  decltype(s_segment_safety_grid_cache_evictions) evictions=s_segment_safety_grid_cache_evictions;
  GridEvictionFixture(){s_segment_safety_grid_cache.clear();s_segment_safety_pinned_grid_keys.clear();s_segment_safety_pinned_route_mask_keys.clear();s_segment_safety_grid_eviction.Reset();s_segment_safety_external_tile_cache={};s_segment_safety_persistent_cache_enabled=false;}
  ~GridEvictionFixture(){s_segment_safety_grid_cache=std::move(grid);s_segment_safety_pinned_grid_keys=std::move(pins);s_segment_safety_pinned_route_mask_keys=std::move(masks);s_segment_safety_grid_eviction=std::move(index);s_segment_safety_external_tile_cache=callbacks;s_segment_safety_persistent_cache_enabled=persistent;s_segment_safety_grid_cache_evictions=evictions;}
};
CachedPointSafetyGridTile CacheFixtureTile(long lat,long lon){
  CachedPointSafetyGridTile tile;tile.built=true;tile.lat_tile=lat;tile.lon_tile=lon;
  tile.group_index=SegmentSafetyCurrentGroupIndex();tile.rows=tile.cols=1;
  tile.classes={SEGMENT_SAFETY_POINT_WATER};tile.hazard_flags={SEGMENT_SAFETY_HAZARD_NONE};
  tile.has_depth={1};tile.min_depth_m={200.0f};tile.has_drying={0};tile.water_count=1;
  tile.depth_complete=true;tile.source=PI_SEGMENT_SAFETY_SOURCE_CM93;
  return tile;
}
}
TEST(ChartGridEviction, OverflowPinsKeepPayloadsAndEvictionOrder){
  GridEvictionFixture fixture;
  std::set<std::pair<long,long>> coords;
  for(int i=0;i<4200;++i)coords.emplace(900+i/64,-1400+i%64);
  PinSegmentSafetyRouteMaskTiles(coords,0,false,0);
  for(auto xy:coords){auto tile=CacheFixtureTile(xy.first,xy.second);StoreSegmentSafetyGridTile(SegmentSafetyGridTileKeyForIndices(xy.first,xy.second),tile);}
  EXPECT_EQ(s_segment_safety_grid_cache.size(),coords.size());
  for(int i=0;i<40;++i){
    auto key=SegmentSafetyGridTileKeyForIndices(1200,-1000+i);
    StoreSegmentSafetyGridTile(key,CacheFixtureTile(1200,-1000+i));
    ASSERT_EQ(s_segment_safety_grid_cache.size(),coords.size()+1);
    if(i) {
      EXPECT_FALSE(s_segment_safety_grid_cache.count(SegmentSafetyGridTileKeyForIndices(1200,-1001+i)));
    }
  }
  for(auto xy:coords){
    CachedPointSafetyGridTile tile;
    ASSERT_TRUE(LookupSegmentSafetyGridTile(SegmentSafetyGridTileKeyForIndices(xy.first,xy.second),&tile));
    EXPECT_EQ(tile.classes,std::vector<unsigned char>{SEGMENT_SAFETY_POINT_WATER});
    EXPECT_EQ(tile.min_depth_m,std::vector<float>{200.0f});
  }
  PlugIn_ReleaseSegmentSafetyRouteMaskPins();
  ASSERT_EQ(s_segment_safety_grid_cache.size(),kMaxSegmentSafetyGridTiles);
  const auto victim=s_segment_safety_grid_cache.begin()->first;
  StoreSegmentSafetyGridTile(SegmentSafetyGridTileKeyForIndices(1300,-1200),CacheFixtureTile(1300,-1200));
  EXPECT_FALSE(s_segment_safety_grid_cache.count(victim));
}
TEST(ChartGridEviction, LatePinRemovesTheSameFirstEligibleKey){
  GridEvictionFixture fixture;
  for(size_t i=0;i<=kMaxSegmentSafetyGridTiles;++i){long lat=900+i/64,lon=-1400+i%64;StoreSegmentSafetyGridTile(SegmentSafetyGridTileKeyForIndices(lat,lon),CacheFixtureTile(lat,lon));}
  ASSERT_EQ(s_segment_safety_grid_cache.size(),kMaxSegmentSafetyGridTiles);
  const auto protected_key=s_segment_safety_grid_cache.begin()->first;
  const auto protected_tile=s_segment_safety_grid_cache.begin()->second;
  const auto victim=std::next(s_segment_safety_grid_cache.begin())->first;
  PinSegmentSafetyRouteMaskTiles({{protected_tile.lat_tile,protected_tile.lon_tile}},0,false,0);
  StoreSegmentSafetyGridTile(SegmentSafetyGridTileKeyForIndices(1300,-1100),CacheFixtureTile(1300,-1100));
  EXPECT_TRUE(s_segment_safety_grid_cache.count(protected_key));
  EXPECT_FALSE(s_segment_safety_grid_cache.count(victim));
}
