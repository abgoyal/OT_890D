

// This header contains WebView declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import "WebPreferences.h"
#import "WebViewPrivate.h"
#import "WebTypesInternal.h"

#ifdef __cplusplus
#import <WebCore/WebCoreKeyboardUIMode.h>

namespace WebCore {
    class String;
    class Frame;
    class KURL;
    class KeyboardEvent;
    class Page;
}
#endif

@class WebBasePluginPackage;
@class WebDownload;
@class WebNodeHighlight;

#ifdef __cplusplus

@interface WebView (WebViewEditingExtras)
- (BOOL)_interceptEditingKeyEvent:(WebCore::KeyboardEvent*)event shouldSaveCommand:(BOOL)shouldSave;
- (BOOL)_shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag;
@end

@interface WebView (AllWebViews)
+ (void)_makeAllWebViewsPerformSelector:(SEL)selector;
- (void)_removeFromAllWebViewsSet;
- (void)_addToAllWebViewsSet;
@end

@interface WebView (WebViewInternal)

- (WebCore::Frame*)_mainCoreFrame;

- (WebCore::String)_userAgentForURL:(const WebCore::KURL&)url;
- (WebCore::KeyboardUIMode)_keyboardUIMode;

- (BOOL)_becomingFirstResponderFromOutside;

#if ENABLE(ICONDATABASE)
- (void)_registerForIconNotification:(BOOL)listen;
- (void)_dispatchDidReceiveIconFromWebFrame:(WebFrame *)webFrame;
#endif

- (void)_setMouseDownEvent:(NSEvent *)event;
- (void)_cancelUpdateMouseoverTimer;
- (void)_stopAutoscrollTimer;
- (void)_updateMouseoverWithFakeEvent;
- (void)_selectionChanged;
- (void)_setToolTip:(NSString *)toolTip;

#if USE(ACCELERATED_COMPOSITING)
- (BOOL)_needsOneShotDrawingSynchronization;
- (void)_setNeedsOneShotDrawingSynchronization:(BOOL)needsSynchronization;
- (void)_startedAcceleratedCompositingForFrame:(WebFrame*)webFrame;
- (void)_stoppedAcceleratedCompositingForFrame:(WebFrame*)webFrame;
- (void)_scheduleCompositingLayerSync;
#endif

@end

#endif

// FIXME: Temporary way to expose methods that are in the wrong category inside WebView.
@interface WebView (WebViewOtherInternal)

+ (void)_setCacheModel:(WebCacheModel)cacheModel;
+ (WebCacheModel)_cacheModel;

#ifdef __cplusplus
- (WebCore::Page*)page;
#endif

- (NSMenu *)_menuForElement:(NSDictionary *)element defaultItems:(NSArray *)items;
- (id)_UIDelegateForwarder;
- (id)_editingDelegateForwarder;
- (id)_policyDelegateForwarder;
- (void)_pushPerformingProgrammaticFocus;
- (void)_popPerformingProgrammaticFocus;
- (void)_incrementProgressForIdentifier:(id)identifier response:(NSURLResponse *)response;
- (void)_incrementProgressForIdentifier:(id)identifier length:(int)length;
- (void)_completeProgressForIdentifier:(id)identifer;
- (void)_progressStarted:(WebFrame *)frame;
- (void)_didStartProvisionalLoadForFrame:(WebFrame *)frame;
+ (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType;
- (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType;
+ (NSString *)_MIMETypeForFile:(NSString *)path;
- (WebDownload *)_downloadURL:(NSURL *)URL;
+ (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme;
+ (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme;
- (BOOL)_isPerformingProgrammaticFocus;
- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(NSUInteger)modifierFlags;
- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request;
- (void)_writeImageForElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard;
- (void)_writeLinkElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard;
- (void)_openFrameInNewWindowFromMenu:(NSMenuItem *)sender;
- (void)_searchWithGoogleFromMenu:(id)sender;
- (void)_searchWithSpotlightFromMenu:(id)sender;
- (void)_progressCompleted:(WebFrame *)frame;
- (void)_didCommitLoadForFrame:(WebFrame *)frame;
- (void)_didFinishLoadForFrame:(WebFrame *)frame;
- (void)_didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
- (void)_didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
- (void)_willChangeValueForKey:(NSString *)key;
- (void)_didChangeValueForKey:(NSString *)key;
- (WebBasePluginPackage *)_pluginForMIMEType:(NSString *)MIMEType;
- (WebBasePluginPackage *)_pluginForExtension:(NSString *)extension;
- (BOOL)_isMIMETypeRegisteredAsPlugin:(NSString *)MIMEType;

- (void)setCurrentNodeHighlight:(WebNodeHighlight *)nodeHighlight;
- (WebNodeHighlight *)currentNodeHighlight;

- (void)addPluginInstanceView:(NSView *)view;
- (void)removePluginInstanceView:(NSView *)view;
- (void)removePluginInstanceViewsFor:(WebFrame*)webFrame;

- (void)_addObject:(id)object forIdentifier:(unsigned long)identifier;
- (id)_objectForIdentifier:(unsigned long)identifier;
- (void)_removeObjectForIdentifier:(unsigned long)identifier;

- (void)_setZoomMultiplier:(float)multiplier isTextOnly:(BOOL)isTextOnly;
- (float)_zoomMultiplier:(BOOL)isTextOnly;
- (float)_realZoomMultiplier;
- (BOOL)_realZoomMultiplierIsTextOnly;
- (BOOL)_canZoomOut:(BOOL)isTextOnly;
- (BOOL)_canZoomIn:(BOOL)isTextOnly;
- (IBAction)_zoomOut:(id)sender isTextOnly:(BOOL)isTextOnly;
- (IBAction)_zoomIn:(id)sender isTextOnly:(BOOL)isTextOnly;
- (BOOL)_canResetZoom:(BOOL)isTextOnly;
- (IBAction)_resetZoom:(id)sender isTextOnly:(BOOL)isTextOnly;

- (BOOL)_mustDrawUnionedRect:(NSRect)rect singleRects:(const NSRect *)rects count:(NSInteger)count;

+ (BOOL)_canHandleRequest:(NSURLRequest *)request forMainFrame:(BOOL)forMainFrame;

- (void)_setInsertionPasteboard:(NSPasteboard *)pasteboard;

@end