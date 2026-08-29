#include "default.h"

namespace Locale {

// Runtime-visible vanilla text is loaded from the user's original
// LOCALE/LANGUAGE.DLL. These arrays deliberately remain empty and exist only
// to preserve the established accessor API and provide safe bounds-checked
// fallbacks when a catalogue entry is absent.
std::string DefaultStrings::YouPlay;
std::string DefaultStrings::WaitingForPlayers;

std::array<std::string, CMN_MAX> DefaultStrings::Commons{};
std::array<std::string, HUDSTR_MAX> DefaultStrings::HudStrings{};
std::array<std::string, WINNAME_MAX> DefaultStrings::WinNames{};
std::array<std::string, TITLE_MAX> DefaultStrings::Titles{};
std::array<std::string, BRIEF_MAX> DefaultStrings::BriefTexts{};
std::array<std::string, FEEDBACK_MAX> DefaultStrings::Feedbacks{};
std::array<std::string, DLG_MAX> DefaultStrings::Dialogs{};
std::array<std::string, NETDLG_MAX> DefaultStrings::NetDlgs{};
std::array<std::string, INPUTS_MAX> DefaultStrings::Inputs{};
std::array<std::string, GLOBMAP_MAX> DefaultStrings::GlobMaps{};
std::array<std::string, ADVICE_MAX> DefaultStrings::Advices{};
std::array<std::string, HELP_MAX> DefaultStrings::Helps{};
std::array<std::string, TIP_MAX> DefaultStrings::Tooltips{};
std::array<std::string, KEYNAME_MAX> DefaultStrings::Keynames{};
std::array<std::string, ADV_MAX> DefaultStrings::Advanced{};

}
