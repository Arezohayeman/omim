#pragma once

#include <jni.h>

#include "map/framework.hpp"
#include "map/place_page_info.hpp"

#include "search/result.hpp"

#include "drape_frontend/gui/skin.hpp"

#include "drape/pointers.hpp"
#include "drape/oglcontextfactory.hpp"

#include "platform/country_defines.hpp"
#include "platform/location.hpp"

#include "geometry/avg_vector.hpp"

#include "base/timer.hpp"

#include "indexer/map_style.hpp"

#include "std/map.hpp"
#include "std/mutex.hpp"
#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"
#include "std/cstdint.hpp"

namespace search
{
struct EverywhereSearchParams;
}

namespace android
{
  class Framework
  {
  private:
    drape_ptr<dp::ThreadSafeFactory> m_contextFactory;
    ::Framework m_work;

    math::LowPassVector<float, 3> m_sensors[2];
    double m_lastCompass;

    string m_searchQuery;

    bool m_isContextDestroyed;

    map<gui::EWidget, gui::Position> m_guiPositions;

    void TrafficStateChanged(TrafficManager::TrafficState state);

    void MyPositionModeChanged(location::EMyPositionMode mode, bool routingActive);

    location::TMyPositionModeChanged m_myPositionModeSignal;
    location::EMyPositionMode m_currentMode;
    bool m_isCurrentModeInitialized;

    TrafficManager::TrafficStateChangedFn m_onTrafficStateChangedFn;

    bool m_isChoosePositionMode;

    place_page::Info m_info;

  public:
    Framework();

    storage::Storage & GetStorage();

    void ShowNode(storage::TCountryId const & countryId, bool zoomToDownloadButton);

    void OnLocationError(int/* == location::TLocationStatus*/ newStatus);
    void OnLocationUpdated(location::GpsInfo const & info);
    void OnCompassUpdated(location::CompassInfo const & info, bool forceRedraw);
    void UpdateCompassSensor(int ind, float * arr);

    void Invalidate();

    bool CreateDrapeEngine(JNIEnv * env, jobject jSurface, int densityDpi, bool firstLaunch);
    bool IsDrapeEngineCreated();

    void DetachSurface(bool destroyContext);
    bool AttachSurface(JNIEnv * env, jobject jSurface);

    void SetMapStyle(MapStyle mapStyle);
    void MarkMapStyle(MapStyle mapStyle);
    MapStyle GetMapStyle() const;

    void SetupMeasurementSystem();

    void SetRouter(routing::RouterType type) { m_work.SetRouter(type); }
    routing::RouterType GetRouter() const { return m_work.GetRouter(); }
    routing::RouterType GetLastUsedRouter() const { return m_work.GetLastUsedRouter(); }

    void Resize(int w, int h);

    struct Finger
    {
      Finger(int64_t id, float x, float y)
        : m_id(id)
        , m_x(x)
        , m_y(y)
      {
      }

      int64_t m_id;
      float m_x, m_y;
    };

    void Touch(int action, Finger const & f1, Finger const & f2, uint8_t maskedPointer);

    bool Search(search::EverywhereSearchParams const & params);
    string GetLastSearchQuery() { return m_searchQuery; }
    void ClearLastSearchQuery() { m_searchQuery.clear(); }

    void AddLocalMaps();
    void RemoveLocalMaps();

    m2::PointD GetViewportCenter() const;

    void AddString(string const & name, string const & value);

    void Scale(::Framework::EScaleMode mode);
    void Scale(m2::PointD const & centerPt, int targetZoom, bool animate);

    void ReplaceBookmark(BookmarkAndCategory const & ind, BookmarkData & bm);
    size_t ChangeBookmarkCategory(BookmarkAndCategory const & ind, size_t newCat);

    ::Framework * NativeFramework();

    bool IsDownloadingActive();

    bool ShowMapForURL(string const & url);

    void DeactivatePopup();

    string GetOutdatedCountriesString();

    void ShowTrack(int category, int track);

    void SetMyPositionModeListener(location::TMyPositionModeChanged const & fn);
    location::EMyPositionMode GetMyPositionMode();
    void OnMyPositionModeChanged(location::EMyPositionMode mode);
    void SwitchMyPositionNextMode();

    void SetTrafficStateListener(TrafficManager::TrafficStateChangedFn const & fn);
    void EnableTraffic();
    void DisableTraffic();

    void Save3dMode(bool allow3d, bool allow3dBuildings);
    void Set3dMode(bool allow3d, bool allow3dBuildings);
    void Get3dMode(bool & allow3d, bool & allow3dBuildings);

    void SetChoosePositionMode(bool isChoosePositionMode, bool isBusiness, bool hasPosition, m2::PointD const & position);
    bool GetChoosePositionMode();

    void SetupWidget(gui::EWidget widget, float x, float y, dp::Anchor anchor);
    void ApplyWidgets();
    void CleanWidgets();

    void SetPlacePageInfo(place_page::Info const & info);
    place_page::Info & GetPlacePageInfo();
    void RequestBookingMinPrice(JNIEnv * env, jobject policy, 
                                string const & hotelId, string const & currency,
                                booking::GetMinPriceCallback const & callback);
    void RequestBookingInfo(JNIEnv * env, jobject policy, 
                            string const & hotelId, string const & lang,
                            booking::GetHotelInfoCallback const & callback);

    bool HasSpaceForMigration();
    storage::TCountryId PreMigrate(ms::LatLon const & position, storage::Storage::TChangeCountryFunction const & statusChangeListener,
                                                                storage::Storage::TProgressFunction const & progressListener);
    void Migrate(bool keepOldMaps);

    bool IsAutoRetryDownloadFailed();
    bool IsDownloadOn3gEnabled();
    void EnableDownloadOn3g();

    uint64_t RequestUberProducts(JNIEnv * env, jobject policy, ms::LatLon const & from,
                                 ms::LatLon const & to, uber::ProductsCallback const & callback,
                                 uber::ErrorCallback const & errorCallback);
    static uber::RideRequestLinks GetUberLinks(string const & productId, ms::LatLon const & from, ms::LatLon const & to);
  };
}

extern android::Framework * g_framework;
