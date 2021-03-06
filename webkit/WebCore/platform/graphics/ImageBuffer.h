

#ifndef ImageBuffer_h
#define ImageBuffer_h

#include "TransformationMatrix.h"
#include "Image.h"
#include "IntSize.h"
#include "ImageBufferData.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    class GraphicsContext;
    class ImageData;
    class IntPoint;
    class IntRect;
    class String;

    enum ImageColorSpace {
        Unknown,
        DeviceRGB, // like sRGB
        GrayScale,
        LinearRGB
    };

    class ImageBuffer : public Noncopyable {
    public:
        // Will return a null pointer on allocation failure.
        static PassOwnPtr<ImageBuffer> create(const IntSize& size, ImageColorSpace colorSpace = DeviceRGB)
        {
            bool success = false;
            OwnPtr<ImageBuffer> buf(new ImageBuffer(size, colorSpace, success));
            if (success)
                return buf.release();
            return 0;
        }

        ~ImageBuffer();

        const IntSize& size() const { return m_size; }
        GraphicsContext* context() const;

        Image* image() const;

        void clearImage() { m_image.clear(); }

        PassRefPtr<ImageData> getImageData(const IntRect& rect) const;
        void putImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint);

        String toDataURL(const String& mimeType) const;
#if !PLATFORM(CG)
        TransformationMatrix baseTransform() const { return TransformationMatrix(); }
        void transformColorSpace(ImageColorSpace srcColorSpace, ImageColorSpace dstColorSpace);
        void platformTransformColorSpace(const Vector<int>&);
#else
        TransformationMatrix baseTransform() const { return TransformationMatrix(1, 0, 0, -1, 0, m_size.height()); }
#endif
    private:
        ImageBufferData m_data;

        IntSize m_size;
        OwnPtr<GraphicsContext> m_context;
        mutable RefPtr<Image> m_image;

#if !PLATFORM(CG)
        Vector<int> m_linearRgbLUT;
        Vector<int> m_deviceRgbLUT;
#endif

        // This constructor will place its success into the given out-variable
        // so that create() knows when it should return failure.
        ImageBuffer(const IntSize&, ImageColorSpace colorSpace, bool& success);
    };

} // namespace WebCore

#endif // ImageBuffer_h
