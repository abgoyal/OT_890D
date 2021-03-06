

#ifndef Geolocation_h
#define Geolocation_h

#include "EventListener.h"
#include "GeolocationService.h"
#include "Geoposition.h"
#include "PositionCallback.h"
#include "PositionError.h"
#include "PositionErrorCallback.h"
#include "PositionOptions.h"
#include "Timer.h"
#include <wtf/Platform.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Frame;
class CachedPositionManager;


class Geolocation : public GeolocationServiceClient, public EventListener {
public:
    static PassRefPtr<Geolocation> create(Frame* frame) { return adoptRef(new Geolocation(frame)); }

    virtual ~Geolocation();

    void disconnectFrame();
    
    Geoposition* lastPosition() const { return m_service->lastPosition(); }

    void getCurrentPosition(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    int watchPosition(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    void clearWatch(int watchId);

    void suspend();
    void resume();
    
    void setIsAllowed(bool);
    bool isAllowed() const { return m_allowGeolocation == Yes; }
    bool isDenied() const { return m_allowGeolocation == No; }
    
    void setShouldClearCache(bool shouldClearCache) { m_shouldClearCache = shouldClearCache; }
    bool shouldClearCache() const { return m_shouldClearCache; }

    static void setDatabasePath(String);

private:
    Geolocation(Frame*);

    class GeoNotifier : public RefCounted<GeoNotifier> {
    public:
        static PassRefPtr<GeoNotifier> create(Geolocation* geolocation, PassRefPtr<PositionCallback> positionCallback, PassRefPtr<PositionErrorCallback> positionErrorCallback, PassRefPtr<PositionOptions> options) { return adoptRef(new GeoNotifier(geolocation, positionCallback, positionErrorCallback, options)); }
        
        void setFatalError(PassRefPtr<PositionError> error);
        void setCachedPosition(Geoposition* cachedPosition);
        void startTimerIfNeeded();
        void timerFired(Timer<GeoNotifier>*);
        
        Geolocation* m_geolocation;
        RefPtr<PositionCallback> m_successCallback;
        RefPtr<PositionErrorCallback> m_errorCallback;
        RefPtr<PositionOptions> m_options;
        Timer<GeoNotifier> m_timer;
        RefPtr<PositionError> m_fatalError;
        RefPtr<Geoposition> m_cachedPosition;

    private:
        GeoNotifier(Geolocation* geolocation, PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    };

    bool hasListeners() const { return !m_oneShots.isEmpty() || !m_watchers.isEmpty(); }

    void sendError(Vector<RefPtr<GeoNotifier> >&, PositionError*);
    void sendPosition(Vector<RefPtr<GeoNotifier> >&, Geoposition*);

    static void stopTimer(Vector<RefPtr<GeoNotifier> >&);
    void stopTimersForOneShots();
    void stopTimersForWatchers();
    void stopTimers();
    
    void makeSuccessCallbacks();
    void handleError(PositionError*);

    void requestPermission();
    PassRefPtr<GeoNotifier> makeRequest(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);

    // GeolocationServiceClient
    virtual void geolocationServicePositionChanged(GeolocationService*);
    virtual void geolocationServiceErrorOccurred(GeolocationService*);

    // EventListener
    virtual void handleEvent(Event*, bool isWindowEvent);

    void fatalErrorOccurred(GeoNotifier* notifier);
    void requestTimedOut(GeoNotifier* notifier);
    void requestReturnedCachedPosition(GeoNotifier* notifier);
    bool haveSuitableCachedPosition(PositionOptions*);

    typedef HashSet<RefPtr<GeoNotifier> > GeoNotifierSet;
    typedef HashMap<int, RefPtr<GeoNotifier> > GeoNotifierMap;
    
    GeoNotifierSet m_oneShots;
    GeoNotifierMap m_watchers;
    Frame* m_frame;
    OwnPtr<GeolocationService> m_service;

    enum {
        Unknown,
        InProgress,
        Yes,
        No
    } m_allowGeolocation;
    bool m_shouldClearCache;

    CachedPositionManager* m_cachedPositionManager;
    GeoNotifierSet m_requestsAwaitingCachedPosition;
};
    
} // namespace WebCore

#endif // Geolocation_h
