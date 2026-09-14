#include "FontManager.h"

#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include <utf8.h>

#include "../core/Strings.h"
#include "../items/ItemFactory.h"
#include "../modes/ModeFactory.h"
#include "../utils/ConfigLoader.h"
#include "../utils/Logger.h"

FontManager& FontManager::Instance() {
    static FontManager sInstance;
    return sInstance;
}

// 读取 UTF-8 文本文件全部内容（失败返回空串）
static std::string ReadFileContent(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool FontManager::Init(int baseSize) {
    if (mInited) return mCjkLoaded;
    mInited = true;

    // ---- 1. 收集需要渲染的全部码点 ----
    //    a) 界面文案（Strings.h，保证界面 100% 覆盖）
    //    b) 道具名 / 模式名（动态注册的显示文本）
    //    c) assets/cjk_common.txt：GB2312 一级常用字（3760 字），
    //       覆盖玩家昵称等用户输入文本，避免未加载字形渲染成方块
    //    d) ASCII 可打印字符（32~126）
    std::string corpus = S::All();
    corpus += ItemFactory::Instance().AllNamesText();
    corpus += ModeFactory::Instance().AllNamesText();

    // 定位自带常用字表：优先 exe 目录，其次工作目录（开发期）
    std::string appDir = GetApplicationDirectory();  // raylib：可执行文件所在目录
    while (!appDir.empty() && (appDir.back() == '\\' || appDir.back() == '/'))
        appDir.pop_back();  // 去掉尾部分隔符，避免拼接出现 "\/" 双斜杠
    const std::string txtCandidates[] = {
        appDir + "/assets/cjk_common.txt",
        "assets/cjk_common.txt",
    };
    for (const std::string& p : txtCandidates) {
        std::string s = ReadFileContent(p);
        if (!s.empty()) {
            corpus += s;
            Logger::Instance().Info("已加载常用字表: " + p + "（" + std::to_string(s.size()) + " 字节）");
            break;
        }
    }

    std::set<int> unique;
    for (char c = 32; c < 127; ++c) unique.insert((int)c);  // 始终包含 ASCII 可打印字符
    try {
        std::string::iterator it = corpus.begin();
        while (it != corpus.end()) {
            unsigned int cp = utf8::next(it, corpus.end());  // utfcpp：UTF-8 解码
            unique.insert((int)cp);
        }
    } catch (const utf8::exception&) {
        Logger::Instance().Warn("字形语料包含非法 UTF-8 序列");
    }
    std::vector<int> codepoints(unique.begin(), unique.end());

    // ---- 2. 字体优先级：项目自带 simhei.ttf > 系统黑体/宋体/等线 ----
    //    不再使用 msyh.ttc（微软雅黑版权属微软，不宜随程序分发；
    //    且依赖系统目录在不同 Windows 版本可能缺失）。
    //    simhei.ttf（黑体）为可自由分发字体，随 exe 携带确保开箱即用。
    int size = ConfigLoader::Instance().FontSize();
    if (size <= 0) size = baseSize;

    const std::string fontCandidates[] = {
        appDir + "/assets/simhei.ttf",   // 发布：exe 同级 assets/
        "assets/simhei.ttf",             // 开发：工作目录下
        appDir + "/simhei.ttf",          // 兼容：exe 根目录
        "C:/Windows/Fonts/simhei.ttf",   // 系统兜底：黑体
        "C:/Windows/Fonts/simsun.ttc",   // 系统兜底：宋体
        "C:/Windows/Fonts/Deng.ttf",     // 系统兜底：等线
    };

    for (const std::string& path : fontCandidates) {
        if (!FileExists(path.c_str())) continue;
        mFont = LoadFontEx(path.c_str(), size, codepoints.data(), (int)codepoints.size());
        if (mFont.texture.id != 0) {
            SetTextureFilter(mFont.texture, TEXTURE_FILTER_BILINEAR);  // 缩放平滑
            mCjkLoaded = true;
            Logger::Instance().Info("已加载中文字体: " + path + "，字形数: " +
                                    std::to_string(codepoints.size()));
            return true;
        }
    }

    // ---- 3. 回退默认字体（无 CJK，中文将无法显示，但游戏仍可运行）----
    mFont = GetFontDefault();
    Logger::Instance().Warn("未找到可用 CJK 字体（simhei.ttf 缺失），回退默认字体");
    return false;
}

void FontManager::DrawText(const std::string& utf8Text, float x, float y, float fontSize,
                           Color color, float spacing) const {
    DrawTextEx(mFont, utf8Text.c_str(), {x, y}, fontSize, spacing, color);
}

void FontManager::DrawTextCentered(const std::string& utf8Text, Rectangle rec, float fontSize,
                                   Color color, float spacing) const {
    Vector2 sz = Measure(utf8Text, fontSize, spacing);
    float x = rec.x + (rec.width - sz.x) * 0.5f;
    float y = rec.y + (rec.height - sz.y) * 0.5f;
    if (x < rec.x) x = rec.x;
    DrawTextEx(mFont, utf8Text.c_str(), {x, y}, fontSize, spacing, color);
}

Vector2 FontManager::Measure(const std::string& utf8Text, float fontSize, float spacing) const {
    return MeasureTextEx(mFont, utf8Text.c_str(), fontSize, spacing);
}
