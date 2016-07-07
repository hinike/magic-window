#include "MagicWindow/MagicWindowApp.h"

using namespace ci;
using namespace ci::app;
using namespace ci::params;
using namespace magicwindow;

//
// Public Methods
//

bool MagicWindowApp::initialize(JsonTree data) {
    try {
        ctx.config.initialize(data);
    }
    catch (std::exception exc) {
        CI_LOG_EXCEPTION("Could not initialize.", exc);
        return false;
    }

    ctx.config.doShowCursor() ? showCursor() : hideCursor();
    initializeWindowConfiguration();
    return true;
}

bool MagicWindowApp::initialize(std::string configFilename)  {
    DataSourceRef cfgData;
    try {
        cfgData = loadAsset(configFilename);
    }
    catch (AssetLoadExc exc) {
        CI_LOG_EXCEPTION("Could not load config file. Goodbye.", exc);
        return false;
    }

    try {
        return initialize(JsonTree(cfgData));
    }
    catch (std::exception exc) {
        CI_LOG_EXCEPTION("Could not parse the config file. Adios.", exc);
        return false;
    }
}

void MagicWindowApp::initializeWindowConfiguration() {
    WindowRef window;
    JsonTree windowConfig = ctx.config.getWindowConfig();
	float appScale = ctx.config.getAppScale();

    // A window for each display with width and height matching the display
    if (ctx.config.getWindowMode() == WindowConfig::DISPLAY_SPAN) {
        
        std::vector<DisplayRef> displays = Display::getDisplays();
        
        for (int i = 0; i < displays.size(); i++) {
            if (i == 0) {
                window = getWindow();
            }
            else {
                window = createWindow();
            }

            Rectf bounds = displays[i]->getBounds();
            window->setUserData(new WindowConfig(i, bounds, vec2()));
            window->setPos(bounds.getUpperLeft());
            window->setSize(bounds.getSize());
            window->setBorderless();
            if (ctx.config.getFullScreen()) window->setFullScreen();
        }
    }

    // As many windows as defined in the window_config variable
    if (ctx.config.getWindowMode() == WindowConfig::DISPLAY_CUSTOM) {
        for (JsonTree::Iter windowIt = windowConfig.begin(); windowIt != windowConfig.end(); windowIt++) {
            int index = std::distance(windowConfig.begin(), windowIt);

			int x = windowIt->getChild("x").getValue<int>();
			int y = windowIt->getChild("y").getValue<int>();
            int w = windowIt->getChild("w").getValue<int>();
            int h = windowIt->getChild("h").getValue<int>();
			int xs = x * appScale;
			int ys = y * appScale;
			int ws = w * appScale;
			int hs = h * appScale;

            if (windowIt == windowConfig.begin()) {
                window = getWindow();
            }else{
                window = createWindow();
            }

            window->setUserData(
				new WindowConfig(index, 
				Rectf(x, y, x + w, y + h), vec2(-x, -y)));

            window->setBorderless();
            window->setPos(xs, ys);
            window->setSize(ws, hs);
            if (ctx.config.getFullScreen()) window->setFullScreen();
        }
    }
    
    if (ctx.config.getWindowMode() == WindowConfig::DISPLAY_GRID) {
        int rows = json::get(windowConfig, "rows", 1);
        int cols = json::get(windowConfig, "columns", 1);
		int w = json::get(windowConfig, "screen_width", 960);
		int h = json::get(windowConfig, "screen_height", 540);
		int ws = w * ctx.config.getAppScale();
		int hs = h * ctx.config.getAppScale();
        
        int index = 0;
        for(int r = 0; r < rows; r++) {
            for(int c = 0; c < cols; c++) {
                // User the default window if this is the first iteration
                if(r == 0 && c == 0) {
                    window = getWindow();
                } else {
                    window = createWindow();
                }
                
                // Calculate the coordinates of each window
                int x = c * w;
                int y = r * h;
				int xs = x * ctx.config.getAppScale();
				int ys = y * ctx.config.getAppScale();
                
                #if defined CINDER_MAC
                // This is an ugly hack to account for the OSX toolbar
                if(!ctx.config.getFullScreen()) if(r != 0) y += 23;
                #endif
                
                window->setBorderless();
                window->setSize(ws, hs);
                window->setPos(xs, ys);
                
                window->setUserData(new WindowConfig(index, Rectf(x, y, w, h), vec2(-x, -y)));
                if(ctx.config.getFullScreen()) window->setFullScreen();
                
                index++;
            }
        }
    }

    paramsWindow = createWindow();
    paramsWindow->setUserData(new WindowConfig(-1, Rectf(), vec2()));
    paramsWindow->setSize(500, 300);
    paramsWindow->setPos(ctx.config.getParamWindowCoords());
    ctx.params = InterfaceGl::create(paramsWindow, "Debug Params", vec2(470, 270));
    ctx.params->addText("FPS", "label='FPS should display here'");
    ctx.params->addSeparator();
    paramsWindowIsAvailable = true;
    if (!ctx.config.doShowParams()) paramsWindow->hide();
    paramsWindow->getSignalClose().connect([&] {
        paramsWindowIsAvailable = false;
    });
}

void MagicWindowApp::draw() {
    WindowRef window = getWindow();
    WindowConfig * data = window->getUserData<WindowConfig>();
    gl::setMatricesWindow(getWindowSize());

    if (window == paramsWindow) {
        gl::clear();
        ctx.params->draw();
    }
    else {
        if (data) {
            gl::clear();
            ctx.signals.preDrawTransform.emit();
            gl::pushMatrices();
            gl::scale(ctx.config.getAppScale(), ctx.config.getAppScale());
            gl::translate(data->getTranslation());
            ctx.signals.draw.emit();
            gl::popMatrices();
            ctx.signals.postDrawTransform.emit();
        }
        else {
            gl::clear();
            ctx.signals.draw.emit();
        }
    }
}

void MagicWindowApp::fileDrop(FileDropEvent e) { ctx.signals.fileDrop.emit(e); }

void MagicWindowApp::update() {
    ctx.info.averageFps = getAverageFps();
    if (ctx.config.doShowParams() && ctx.params) {
        ctx.params->setOptions("FPS", "label='FPS: " + toString(getAverageFps()) + "'");
    }
    
    ctx.state.update();
    ctx.signals.update.emit();
}

void MagicWindowApp::keyDown(KeyEvent e) {
    if (ctx.config.getDefaultKeyHandlersEnabled()) {
        switch (e.getCode()) {
        case KeyEvent::KEY_ESCAPE:
            quit();
            break;
        case KeyEvent::KEY_m:
            if (e.isControlDown()) {
                ctx.config.setCursorVisibility(!ctx.config.doShowCursor());
                ctx.config.doShowCursor() ? showCursor() : hideCursor();
            }
            break;
        case KeyEvent::KEY_p:
            if (paramsWindowIsAvailable) {
                paramsWindow->isHidden() ? paramsWindow->show() : paramsWindow->hide();
            }
            break;
        }
    }
    ctx.signals.keyDown.emit(e);
}

void MagicWindowApp::keyUp(KeyEvent e) { ctx.signals.keyUp.emit(e); }
void MagicWindowApp::mouseDown(MouseEvent e) { ctx.signals.mouseDown.emit(e); }
void MagicWindowApp::mouseDrag(MouseEvent e) { ctx.signals.mouseDrag.emit(e); }
void MagicWindowApp::mouseMove(MouseEvent e) { ctx.signals.mouseMove.emit(e); }
void MagicWindowApp::mouseUp(MouseEvent e) { ctx.signals.mouseUp.emit(e); }
void MagicWindowApp::mouseWheel(MouseEvent e) { ctx.signals.mouseWheel.emit(e); }
void MagicWindowApp::cleanup() { 
    ctx.signals.cleanup.emit();
    App::cleanup();
}