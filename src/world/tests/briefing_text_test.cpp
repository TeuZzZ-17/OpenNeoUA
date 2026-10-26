#include "game/yw.h"
#include <cstdio>
#undef main
int main() {
    TLevelDescription level;
    ScriptParser::HandlersList handlers{new World::Parsers::LevelBriefingTextParser(&level), new World::Parsers::LevelMbMapParser(&level)};
    Engine::StringList lines={"begin_briefing_text", "Hello; punctuation = stays", "", "  spaces", "\\end", "\\\\path", "include = nonexistent.ldf", "", "end", "begin_mbmap", "name = MB_10.IFF", "size_x = 480", "size_y = 480", "end"};
    if (!ScriptParser::Parser::ParseStringList(lines,handlers,ScriptParser::FLAG_NO_SCOPE_SKIP)) return 1;
    if (level.BriefingText != "Hello; punctuation = stays\n\n  spaces\nend\n\\path\ninclude = nonexistent.ldf\n") return 2;
    if (level.Mbmaps.size()!=1 || level.Mbmaps[0].PicName!="MB_10.IFF") return 3;
    Engine::StringList empty={"begin_briefing_text", "  ", "", "end"};
    if (!ScriptParser::Parser::ParseStringList(empty,handlers,ScriptParser::FLAG_NO_SCOPE_SKIP) || !level.BriefingText.empty()) return 4;
    Engine::StringList broken={"begin_briefing_text", "missing end"};
    if (ScriptParser::Parser::ParseStringList(broken,handlers,ScriptParser::FLAG_NO_SCOPE_SKIP)) return 5;
    std::puts("Briefing: raw text, escaped lines, following scope, empty fallback, malformed EOF passed.");
    return 0;
}
