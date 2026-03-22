#include "app_profiles.h"

void AppProfiles::OnKnobDelta(int32_t delta)
{
    // In L2 submenu, knob scrolls the profile list
    (void)delta;
}

uint8_t AppProfiles::GetSubItemCount() const
{
    return profileStore.GetSlotCount();
}

const char* AppProfiles::GetSubItemName(uint8_t idx) const
{
    if (profileStore.IsSlotUsed(idx))
        return profileStore.GetSlotName(idx);
    return "(empty)";
}

void AppProfiles::OnSubItemSelected(uint8_t idx)
{
    selectedSlot_ = idx;
    if (profileStore.IsSlotUsed(idx)) {
        profileStore.LoadProfile(idx);
    }
}
