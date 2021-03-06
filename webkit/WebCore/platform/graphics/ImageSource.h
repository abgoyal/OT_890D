

#ifndef ImageSource_h
#define ImageSource_h

#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

#if PLATFORM(WX)
class wxBitmap;
class wxGraphicsBitmap;
#elif PLATFORM(CG)
typedef struct CGImageSource* CGImageSourceRef;
typedef struct CGImage* CGImageRef;
typedef const struct __CFData* CFDataRef;
#elif PLATFORM(QT)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QPixmap;
QT_END_NAMESPACE
#elif PLATFORM(CAIRO)
struct _cairo_surface;
typedef struct _cairo_surface cairo_surface_t;
#elif PLATFORM(ANDROID) && PLATFORM(SGL)
#include "SkString.h"
class SkBitmapRef;
class PrivateAndroidImageSourceRec;
#elif PLATFORM(SKIA)
class NativeImageSkia;
#elif PLATFORM(WINCE)
#include "SharedBitmap.h"
#endif

namespace WebCore {

class IntSize;
class SharedBuffer;
class String;

#if PLATFORM(WX)
class ImageDecoder;
typedef ImageDecoder* NativeImageSourcePtr;
typedef const Vector<char>* NativeBytePtr;
#if USE(WXGC)
typedef wxGraphicsBitmap* NativeImagePtr;
#else
typedef wxBitmap* NativeImagePtr;
#endif
#elif PLATFORM(CG)
typedef CGImageSourceRef NativeImageSourcePtr;
typedef CGImageRef NativeImagePtr;
#elif PLATFORM(QT)
class ImageDecoderQt;
typedef ImageDecoderQt* NativeImageSourcePtr;
typedef QPixmap* NativeImagePtr;
#elif PLATFORM(ANDROID)
#if PLATFORM(SGL)
class String;
#ifdef ANDROID_ANIMATED_GIF
class ImageDecoder;
#endif
struct NativeImageSourcePtr {
    SkString m_url; 
    PrivateAndroidImageSourceRec* m_image;
#ifdef ANDROID_ANIMATED_GIF
    ImageDecoder* m_gifDecoder;
#endif
};
typedef const Vector<char>* NativeBytePtr;
typedef SkBitmapRef* NativeImagePtr;
#elif PLATFORM(SKIA) // ANDROID
class ImageDecoder;
typedef ImageDecoder* NativeImageSourcePtr;
typedef NativeImageSkia* NativeImagePtr;
#endif
#elif PLATFORM(CAIRO)
class ImageDecoder;
typedef ImageDecoder* NativeImageSourcePtr;
typedef cairo_surface_t* NativeImagePtr;
#elif PLATFORM(SKIA)
class ImageDecoder;
typedef ImageDecoder* NativeImageSourcePtr;
typedef NativeImageSkia* NativeImagePtr;
#elif PLATFORM(WINCE)
class ImageDecoder;
typedef ImageDecoder* NativeImageSourcePtr;
typedef RefPtr<SharedBitmap> NativeImagePtr;
#endif

const int cAnimationLoopOnce = -1;
const int cAnimationNone = -2;

class ImageSource : public Noncopyable {
public:
    ImageSource();
    ~ImageSource();

    // Tells the ImageSource that the Image no longer cares about decoded frame
    // data -- at all (if |destroyAll| is true), or before frame
    // |clearBeforeFrame| (if |destroyAll| is false).  The ImageSource should
    // delete cached decoded data for these frames where possible to keep memory
    // usage low.  When |destroyAll| is true, the ImageSource should also reset
    // any local state so that decoding can begin again.
    //
    // Implementations that delete less than what's specified above waste
    // memory.  Implementations that delete more may burn CPU re-decoding frames
    // that could otherwise have been cached, or encounter errors if they're
    // asked to decode frames they can't decode due to the loss of previous
    // decoded frames.
    //
    // Callers should not call clear(false, n) and subsequently call
    // createFrameAtIndex(m) with m < n, unless they first call clear(true).
    // This ensures that stateful ImageSources/decoders will work properly.
    //
    // The |data| and |allDataReceived| parameters should be supplied by callers
    // who set |destroyAll| to true if they wish to be able to continue using
    // the ImageSource.  This way implementations which choose to destroy their
    // decoders in some cases can reconstruct them correctly.
    void clear(bool destroyAll,
               size_t clearBeforeFrame = 0,
               SharedBuffer* data = NULL,
               bool allDataReceived = false);

    bool initialized() const;

    void setData(SharedBuffer* data, bool allDataReceived);
    String filenameExtension() const;

    bool isSizeAvailable();
    IntSize size() const;
    IntSize frameSizeAtIndex(size_t) const;

    int repetitionCount();

    size_t frameCount() const;

    // Callers should not call this after calling clear() with a higher index;
    // see comments on clear() above.
    NativeImagePtr createFrameAtIndex(size_t);

    float frameDurationAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t); // Whether or not the frame actually used any alpha.
    bool frameIsCompleteAtIndex(size_t); // Whether or not the frame is completely decoded.

#if PLATFORM(ANDROID)
#if PLATFORM(SGL)
    void clearURL();
    void setURL(const String& url);
#endif
#endif
private:
#if PLATFORM(ANDROID)
    // FIXME: This is protected only to allow ImageSourceSkia to set ICO decoder
    // with a preferred size. See ImageSourceSkia.h for discussion.
protected:
#endif
    NativeImageSourcePtr m_decoder;
};

}

#endif
