#include "overlaymanager.h"
#include "path.h"

using namespace Overlay;

OverlayManager::OverlayManager() :
    m_Renderer(nullptr),
    m_FontData(Path::readDataFile("ModeSeven.ttf"))
{
    memset(m_Overlays, 0, sizeof(m_Overlays));

    // 获取默认显示器的DPI
    float ddpi, hdpi, vdpi;
    if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "无法获取显示器DPI: %s", SDL_GetError());
        ddpi = 96.0f; // 使用默认DPI
    }
    
    // DPI缩放因子（基于标准96 DPI）
    float dpiScale = ddpi / 96.0f;
    
    // 使用DPI缩放调整字体大小
    m_Overlays[OverlayType::OverlayDebug].color = {0xBD, 0xF9, 0xE7, 0xFF};
    m_Overlays[OverlayType::OverlayDebug].fontSize = (int)(20 * dpiScale);
    m_Overlays[OverlayType::OverlayDebug].bgcolor = {0x00, 0x00, 0x00, 0x96};

    m_Overlays[OverlayType::OverlayStatusUpdate].color = {0xCC, 0x00, 0x00, 0xFF};
    m_Overlays[OverlayType::OverlayStatusUpdate].fontSize = (int)(36 * dpiScale);

    // While TTF will usually not be initialized here, it is valid for that not to
    // be the case, since Session destruction is deferred and could overlap with
    // the lifetime of a new Session object.
    //SDL_assert(TTF_WasInit() == 0);

    if (TTF_Init() != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "TTF_Init() failed: %s",
                    TTF_GetError());
        return;
    }
}

OverlayManager::~OverlayManager()
{
    for (int i = 0; i < OverlayType::OverlayMax; i++) {
        if (m_Overlays[i].surface != nullptr) {
            SDL_FreeSurface(m_Overlays[i].surface);
        }
        if (m_Overlays[i].font != nullptr) {
            TTF_CloseFont(m_Overlays[i].font);
        }
    }

    TTF_Quit();

    // For similar reasons to the comment in the constructor, this will usually,
    // but not always, deinitialize TTF. In the cases where Session objects overlap
    // in lifetime, there may be an additional reference on TTF for the new Session
    // that means it will not be cleaned up here.
    //SDL_assert(TTF_WasInit() == 0);
}

bool OverlayManager::isOverlayEnabled(OverlayType type)
{
    return m_Overlays[type].enabled;
}

char* OverlayManager::getOverlayText(OverlayType type)
{
    return m_Overlays[type].text;
}

void OverlayManager::updateOverlayText(OverlayType type, const char* text)
{
    strncpy(m_Overlays[type].text, text, sizeof(m_Overlays[0].text));
    m_Overlays[type].text[getOverlayMaxTextLength() - 1] = '\0';

    setOverlayTextUpdated(type);
}

int OverlayManager::getOverlayMaxTextLength()
{
    return sizeof(m_Overlays[0].text);
}

int OverlayManager::getOverlayFontSize(OverlayType type)
{
    return m_Overlays[type].fontSize;
}

SDL_Surface* OverlayManager::getUpdatedOverlaySurface(OverlayType type)
{
    // If a new surface is available, return it. If not, return nullptr.
    // Caller must free the surface on success.
    return (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, nullptr);
}

void OverlayManager::setOverlayTextUpdated(OverlayType type)
{
    // Only update the overlay state if it's enabled. If it's not enabled,
    // the renderer has already been notified by setOverlayState().
    if (m_Overlays[type].enabled) {
        notifyOverlayUpdated(type);
    }
}

void OverlayManager::setOverlayState(OverlayType type, bool enabled)
{
    bool stateChanged = m_Overlays[type].enabled != enabled;

    m_Overlays[type].enabled = enabled;

    if (stateChanged) {
        if (!enabled) {
            // Set the text to empty string on disable
            m_Overlays[type].text[0] = 0;
        }

        notifyOverlayUpdated(type);
    }
}

SDL_Color OverlayManager::getOverlayColor(OverlayType type)
{
    return m_Overlays[type].color;
}

void OverlayManager::setOverlayRenderer(IOverlayRenderer* renderer)
{
    m_Renderer = renderer;
}

void OverlayManager::notifyOverlayUpdated(OverlayType type)
{
    if (m_Renderer == nullptr) {
        return;
    }

    // Construct the required font to render the overlay
    if (m_Overlays[type].font == nullptr) {
        if (m_FontData.isEmpty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL overlay font failed to load");
            return;
        }

        // m_FontData must stay around until the font is closed
        m_Overlays[type].font = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()),
                                               1,
                                               m_Overlays[type].fontSize);
        if (m_Overlays[type].font == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "TTF_OpenFont() failed: %s",
                        TTF_GetError());

            // Can't proceed without a font
            return;
        }
    }

    SDL_Surface* oldSurface = (SDL_Surface*)SDL_AtomicSetPtr((void**)&m_Overlays[type].surface, nullptr);

    // Free the old surface
    if (oldSurface != nullptr) {
        SDL_FreeSurface(oldSurface);
    }

    if (m_Overlays[type].enabled)
    {
        TTF_SetFontWrappedAlign(m_Overlays[type].font, TTF_WRAPPED_ALIGN_CENTER);
        
        // 添加字体检查和错误处理
        if (m_Overlays[type].font == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "字体无效，无法渲染文本");
            return;
        }
        
        // 首先渲染文本
        SDL_Surface *textSurface = TTF_RenderUTF8_Blended(m_Overlays[type].font,
                                                    m_Overlays[type].text,
                                                    m_Overlays[type].color);
                                                    
        // 检查文本渲染是否成功
        if (textSurface == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "文本渲染失败: %s", TTF_GetError());
            return;
        }
        
        // 创建一个带有内边距的新表面
        float ddpi, hdpi, vdpi;
        int padding = 2; // 默认内边距
        if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0) {
            float dpiScale = ddpi / 96.0f;
            padding = (int)(padding * dpiScale);
        }
        
        // 创建一个带有内边距的新表面
        SDL_Surface *paddedSurface = SDL_CreateRGBSurfaceWithFormat(
            0,
            textSurface->w + padding * 2,
            textSurface->h + padding * 2,
            32,
            textSurface->format->format
        );
        
        // 用背景色填充paddedSurface
        SDL_FillRect(paddedSurface, NULL, 
                    SDL_MapRGBA(paddedSurface->format, 
                               m_Overlays[type].bgcolor.r,
                               m_Overlays[type].bgcolor.g,
                               m_Overlays[type].bgcolor.b,
                               m_Overlays[type].bgcolor.a));
        
        // 将文本表面复制到带内边距的表面中央
        SDL_Rect destRect = {padding, padding, textSurface->w, textSurface->h};
        SDL_BlitSurface(textSurface, NULL, paddedSurface, &destRect);
        
        // 释放原始文本表面
        SDL_FreeSurface(textSurface);
        
        // 使用带内边距的表面
        SDL_AtomicSetPtr((void **)&m_Overlays[type].surface, paddedSurface);
    }

    // Notify the renderer
    m_Renderer->notifyOverlayUpdated(type);
}
