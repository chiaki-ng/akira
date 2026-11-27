#include <span>

#include "app/arisa/app.hpp"

int main(int argc, char* argv[])
{
    akira::app::arisa::App app(std::span(argv, static_cast<std::size_t>(argc)));
    return app.run();
}
