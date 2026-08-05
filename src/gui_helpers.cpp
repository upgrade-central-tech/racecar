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

void SettingsCombo( const char* label, RacecarSettings s )
{
    bool valid = RuntimeSettings::GetValid( s );
    int current = RuntimeSettings::GetValue( s );

    ImGui::BeginDisabled( !valid );
    if ( ImGui::BeginCombo( label, RuntimeSettings::GetOptionLabel( s, current ) ) ) {
        for ( int i = 0; i < RuntimeSettings::GetOptionCount( s ); i++ ) {
            bool is_selected = ( current == i );

            ImGui::BeginDisabled( !RuntimeSettings::GetOptionValid( s, i ) );
            if ( ImGui::Selectable( RuntimeSettings::GetOptionLabel( s, i ), is_selected ) ) {
                RuntimeSettings::SetValue( s, i );
            }
            ImGui::EndDisabled();

            if ( is_selected ) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
}

}
