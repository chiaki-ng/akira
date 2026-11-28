#include "test_util.hpp"

#include "settings/release_notes.hpp"

using akira::settings::ReleaseNotes;

TEST(release_notes_drop_the_footer_above_the_notes)
{
    const ReleaseNotes notes("<!-- akira-footer -->\r\n"
                             "Please read the [README](https://example.com#readme) before getting started.\r\n"
                             "\r\n"
                             "For issues, please reach out in [![discord](https://img.example/badge)](https://discord.gg/x).\r\n"
                             "\r\n"
                             "---\r\n"
                             "\r\n"
                             "<!-- /akira-footer -->\r\n"
                             "Disabled colour picture adjustment for NX1\r\n");
    CHECK_EQ(notes.readable(), std::string("Disabled colour picture adjustment for NX1"));
}

TEST(release_notes_drop_a_trailing_footer_without_a_close)
{
    const ReleaseNotes notes("Fixed things\n\n<!-- akira-footer -->\nPlease read the README.\n");
    CHECK_EQ(notes.readable(), std::string("Fixed things"));
}

TEST(release_notes_drop_build_tables_and_empty_headings)
{
    const ReleaseNotes notes("Disabled colour\n\n"
                             "## Built from\n\n"
                             "| Component | Version |\n"
                             "| --- | --- |\n"
                             "| akira-ounce | `27c53f6` |\n\n"
                             "## devkitPro packages\n\n"
                             "```\n"
                             "devkitA64 r30-1\n"
                             "```\n");
    CHECK_EQ(notes.readable(), std::string("Disabled colour"));
}

TEST(release_notes_reshape_headings_and_lists)
{
    const ReleaseNotes notes("## What's new\n"
                             "- Rebuilt interface\n"
                             "* Rumble from the input thread\n\n"
                             "Thanks to **Flal** for the [report](https://github.com/x).\n");
    CHECK_EQ(notes.readable(), std::string("**What's new**\n"
                                           "• Rebuilt interface\n"
                                           "• Rumble from the input thread\n"
                                           "\n"
                                           "Thanks to **Flal** for the [report](https://github.com/x)."));
}

TEST(release_notes_drop_comments_rules_and_images)
{
    const ReleaseNotes notes("<!-- akira-banner -->\n## Read this first\n<!-- /akira-banner -->\n"
                             "***\n"
                             "![shot](https://img.example/a.png)\n"
                             "Kept line\n");
    CHECK_EQ(notes.readable(), std::string("**Read this first**\nKept line"));
}

TEST(release_notes_empty_when_nothing_is_readable)
{
    CHECK(ReleaseNotes("<!-- akira-footer -->\nPlease read the README.\n").empty());
    CHECK(ReleaseNotes("").empty());
}

TEST(release_notes_link_bare_urls)
{
    const ReleaseNotes notes("**Full Changelog**: https://github.com/chiaki-ng/akira/compare/v0.7.0...v1.0.4\n"
                             "See [the docs](https://chiaki-ng.github.io/akira) or <https://discord.gg/x>.\n");
    CHECK_EQ(notes.readable(), std::string("**Full Changelog**: <https://github.com/chiaki-ng/akira/compare/v0.7.0...v1.0.4>\n"
                                           "See [the docs](https://chiaki-ng.github.io/akira) or <https://discord.gg/x>."));
}
