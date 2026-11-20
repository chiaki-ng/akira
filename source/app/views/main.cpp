/*
 * Akira - PlayStation Remote Play for Nintendo Switch
 * Built with Borealis UI Framework
 */

#include "app/views/borealis_app.hpp"

int main(int argc, char* argv[])
{
    akira::app::views::BorealisApp app(argc, argv);
    return app.run();
}
