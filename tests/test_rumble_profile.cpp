
#include "test_util.hpp"

#include "input/rumble_profile.hpp"


using namespace akira::input;


TEST(rumble_profile_switch_defaults_preserve_todays_joycon_feel)
{
    const RumbleProfile s = SwitchRumbleProfile();

    CHECK(s.ceiling > 0.62f && s.ceiling < 0.64f);
    CHECK(!s.per_motor);

    const RumbleProfile d = DefaultRumbleProfile();
    CHECK_EQ((int)(d.ceiling * 100.0f), 100);
    CHECK(d.per_motor);
}

TEST(rumble_profile_defaults_to_the_derived_source)
{
    const RumbleProfile d = DefaultRumbleProfile();
    CHECK(d.rumble_source == RumbleSource::Derived);
}


TEST(haptic_base_puts_normal_where_the_old_strong_preset_was)
{
    CHECK(HapticBaseForIntensity(HapticIntensity::Normal) == 50);

    CHECK(HapticBaseForIntensity(HapticIntensity::VeryWeak) >
          HapticBaseForIntensity(HapticIntensity::Weak));
    CHECK(HapticBaseForIntensity(HapticIntensity::Weak) >
          HapticBaseForIntensity(HapticIntensity::Normal));
    CHECK(HapticBaseForIntensity(HapticIntensity::Normal) >
          HapticBaseForIntensity(HapticIntensity::Strong));
    CHECK(HapticBaseForIntensity(HapticIntensity::Strong) >
          HapticBaseForIntensity(HapticIntensity::VeryStrong));

    CHECK(HapticBaseForIntensity(HapticIntensity::Off) > 0);
}

