#ifndef AKIRA_VERSION_HPP
#define AKIRA_VERSION_HPP

#include <akira_version.h>

namespace akira::version {

inline const char* string() { return AKIRA_VERSION_STRING; }
inline const char* semver() { return AKIRA_VERSION_SEMVER; }
inline const char* channel() { return AKIRA_VERSION_CHANNEL; }
inline const char* commit() { return AKIRA_VERSION_COMMIT; }
inline const char* date() { return AKIRA_VERSION_DATE; }

inline int major() { return AKIRA_VERSION_MAJOR; }
inline int minor() { return AKIRA_VERSION_MINOR; }
inline int patch() { return AKIRA_VERSION_PATCH; }

inline bool isDev() { return channel()[0] == 'd'; }

}

#endif
