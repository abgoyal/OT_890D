

#ifndef PopupMenuChromium_h
#define PopupMenuChromium_h

#include "config.h"
#include "PopupMenuClient.h"

#include "FramelessScrollView.h"
#include "IntRect.h"

namespace WebCore {

    class FrameView;
    class PopupListBox;

    // A container for the data for each menu item (e.g. represented by <option>
    // or <optgroup> in a <select> widget) and is used by PopupListBox.
    struct PopupItem {
        enum Type {
            TypeOption,
            TypeGroup,
            TypeSeparator
        };

        PopupItem(const String& label, Type type)
            : label(label), type(type), yOffset(0) { }
        String label;
        Type type;
        int yOffset;  // y offset of this item, relative to the top of the popup.
        bool enabled;
    };

    // FIXME: Our FramelessScrollView classes should probably implement HostWindow!

    // The PopupContainer class holds a PopupListBox (see cpp file).  Its sole purpose is to be
    // able to draw a border around its child.  All its paint/event handling is
    // just forwarded to the child listBox (with the appropriate transforms).
    // NOTE: this class is exposed so it can be instantiated direcly for the
    // autofill popup.  We cannot use the Popup class directly in that case as the
    // autofill popup should not be focused when shown and we want to forward the
    // key events to it (through handleKeyEvent).

    struct PopupContainerSettings {
        // Whether the popup should get the focus when displayed.
        bool focusOnShow;

        // Whether the PopupMenuClient should be told to change its text when a
        // new item is selected by using the arrow keys.
        bool setTextOnIndexChange;

        // Whether the selection should be accepted when the popup menu is
        // closed (through ESC being pressed or the focus going away).
        // Note that when TAB is pressed, the selection is always accepted
        // regardless of this setting.
        bool acceptOnAbandon;

        // Whether the we should move the selection to the first/last item when
        // the user presses down/up arrow keys and the last/first item is
        // selected.
        bool loopSelectionNavigation;
    };

    class PopupContainer : public FramelessScrollView {
    public:
        static PassRefPtr<PopupContainer> create(PopupMenuClient*,
                                                 const PopupContainerSettings&);

        // Whether a key event should be sent to this popup.
        virtual bool isInterestedInEventForKey(int keyCode);

        // FramelessScrollView
        virtual void paint(GraphicsContext*, const IntRect&);
        virtual void hide();
        virtual bool handleMouseDownEvent(const PlatformMouseEvent&);
        virtual bool handleMouseMoveEvent(const PlatformMouseEvent&);
        virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&);
        virtual bool handleWheelEvent(const PlatformWheelEvent&);
        virtual bool handleKeyEvent(const PlatformKeyboardEvent&);

        // PopupContainer methods

        // Show the popup
        void showPopup(FrameView*);

        // Used on Mac Chromium for HTML select popup menus.
        void showExternal(const IntRect&, FrameView*, int index);

        // Show the popup in the specified rect for the specified frame.
        // Note: this code was somehow arbitrarily factored-out of the Popup class
        // so WebViewImpl can create a PopupContainer. This method is used for
        // displaying auto complete popup menus on Mac Chromium, and for all
        // popups on other platforms.
        void show(const IntRect&, FrameView*, int index);

        // Hide the popup.  Do not call this directly: use client->hidePopup().
        void hidePopup();

        // Compute size of widget and children.
        void layout();

        PopupListBox* listBox() const { return m_listBox.get(); }

        // Gets the index of the item that the user is currently moused-over or
        // has selected with the keyboard up/down arrows.
        int selectedIndex() const;

        // Refresh the popup values from the PopupMenuClient.
        void refresh();

        // The menu per-item data.
        const WTF::Vector<PopupItem*>& popupData() const;

        // The height of a row in the menu.
        int menuItemHeight() const;

    private:
        friend class WTF::RefCounted<PopupContainer>;

        PopupContainer(PopupMenuClient*, const PopupContainerSettings&);
        ~PopupContainer();

        // Paint the border.
        void paintBorder(GraphicsContext*, const IntRect&);

        RefPtr<PopupListBox> m_listBox;

        PopupContainerSettings m_settings;
    };

} // namespace WebCore

#endif
