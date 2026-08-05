#include "gui_helpers.hpp"

#include <imgui.h>

namespace RacecarGUI {

void SettingsCheckbox( const char* label, RacecarSettings s )
{
    bool valid = RuntimeSettings::GetValid( s );
    bool enabled = RuntimeSettings::GetEnabled( s );

    ImGui::BeginDisabled( !valid );
    if ( ImGui::Checkbox( label, &enabled ) ) {
        RuntimeSettings::SetEnabled( s, enabled );
    }
    ImGui::EndDisabled();
}

}
