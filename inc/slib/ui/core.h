#ifndef CHECKHEADER_SLIB_UI_CORE
#define CHECKHEADER_SLIB_UI_CORE

#include "definition.h"

#include "event.h"

#include "../core/app.h"
#include "../core/string.h"
#include "../core/thread.h"

#include "../graphics/context.h"

SLIB_UI_NAMESPACE_BEGIN

class Window;
class Screen;
class Cursor;
class Menu;

class SLIB_EXPORT UI
{
public:
	// Graphics
	static void setDefaultFontName(const String& fontName);

	static Size getFontTextSize(const Ref<Font>& font, const String& text);
	
	static Rectangle getPathBounds(const Ref<GraphicsPath>& path);
	
	static sl_bool checkPointInPath(const Ref<GraphicsPath>& path, const Point& pt);
	
	static Ref<Drawable> createDrawableFromImage(const ImageDesc& desc);
	
	static Ref<Drawable> loadDrawableFromMemory(const void* mem, sl_size size);
	
	static Ref<Bitmap> createBitmap(sl_uint32 width, sl_uint32 height);
	
	static Ref<Drawable> createDrawableFromBitmap(const Ref<Bitmap>& bitmap);
	
	static Ref<Bitmap> loadBitmapFromMemory(const void* mem, sl_size size);

	static Ref<GraphicsContext> getGraphicsContext();
	
	static Ref<Drawable> createDrawableFromImage(const Ref<Image>& image);
	
	static Ref<Drawable> loadDrawableFromMemory(const Memory& mem);
	
	static Ref<Drawable> loadDrawableFromFile(const String& filePath);
	
	static Ref<Drawable> loadDrawableFromAsset(const String& path);
	
	static Ref<Bitmap> createBitmapFromImage(const ImageDesc& desc);
	
	static Ref<Bitmap> createBitmapFromImage(const Ref<Image>& image);
	
	static Ref<Bitmap> loadBitmapFromMemory(const Memory& mem);
	
	static Ref<Bitmap> loadBitmapFromFile(const String& filePath);
	
	static Ref<Bitmap> loadBitmapFromAsset(const String& path);


	// Screens
	static List< Ref<Screen> > getScreens();
	
	static Ref<Screen> getPrimaryScreen();
	
	static Ref<Screen> getFocusedScreen();
	
	static Rectangle getScreenRegion();
	
	static Rectangle getScreenRegion(const Ref<Screen>& screen);
	
	static Rectangle getScreenBounds();
	
	static Rectangle getScreenBounds(const Ref<Screen>& screen);
	
	static Size getScreenSize();
	
	static Size getScreenSize(const Ref<Screen>& screen);
	
	
	// Message Box
	// Run on UI thread
	static void alert(const String& text);
	
	// Run on UI thread
	static void alert(const String& caption, const String& text);
	
	static void showAlert(const String& text, const Ref<Runnable>& onOk);
	
	static void showAlert(const String& caption, const String& text, const Ref<Runnable>& onOk);
	
	
	// HID related functions (Platform Specific, only for desktop apps)
	static sl_bool checkKeyPressed(Keycode key);
	
	static sl_bool checkCapsLockOn();
	
	static sl_bool checkScrollLockOn();
	
	static sl_bool checkNumLockOn();
	
	static Point getCursorPos();
	
	static sl_bool checkLeftButtonPressed();
	
	static sl_bool checkRightButtonPressed();
	
	static sl_bool checkMiddleButtonPressed();
	
	static String getStatusDescription();
	
	
	// HID related functions
	static String getKeyName(Keycode key, sl_bool flagShort = sl_false);

	
	// UI Thread
	static sl_bool isUiThread();
	
	static void dispatchToUiThread(const Ref<Runnable>& callback);
	
	static void runOnUiThread(const Ref<Runnable>& callback);
	
	
	// Run Loop
	static void runLoop();
	
	static void quitLoop();
	
	static void runApp();
	
	static void quitApp();

	// App related
	static Ref<Window> getMainWindow();

	static void setMainWindow(const Ref<Window>& window);

	static Ref<Menu> getApplicationMenu();

	static void setApplicationMenu(const Ref<Menu>& menu);

};

class SLIB_EXPORT UIApp : public Application
{
	SLIB_DECLARE_OBJECT

public:
	static Ref<UIApp> getApp();

public:
	// override
	AppType getAppType();
	
	// override
	void run(const String& param);
	
	void run();

public:
	virtual void onStart();
	
	static void dispatchStart();
	
	virtual void onExit();
	
	static void dispatchExit();

public:
	// mobile
	virtual void onMobileCreate();
	
	static void dispatchMobileCreate();
	
	virtual void onMobilePause();
	
	static void dispatchMobilePause();

	virtual void onMobileResume();
	
	static void dispatchMobileResume();

	virtual void onMobileDestroy();
	
	static void dispatchMobileDestroy();
	
	virtual sl_bool onMobileBack();

	static sl_bool dispatchMobileBack();

	static void quit();

	static sl_bool isMobilePaused();

	static Ref<Window> getMainWindow();

	static void setMainWindow(const Ref<Window>& window);

	// Menu Bar (Mainly used in OSX)
	static Ref<Menu> getMenu();

	static void setMenu(const Ref<Menu>& menu);

private:
	static sl_bool m_flagMobilePaused;

};

SLIB_UI_NAMESPACE_END

#endif
