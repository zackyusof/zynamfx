# zynamfx distribution identity. Keep these identifiers stable after release.
# The TONE3000State serialization tag and upstream service names remain for
# preset compatibility and TONE3000 API integration.

option(T3K_FORK_NAMESPACED
  "Build with zynamfx distribution identities" ON)

set(T3K_FORK_DISPLAY_NAME "zynamfx"
  CACHE STRING "Fork plugin display name (host menus, window titles)")
set(T3K_FORK_PRODUCT_NAME "zynamfx"
  CACHE STRING "Fork bundle/file name (no spaces: .component/.vst3/.app)")
set(T3K_FORK_PLUGIN_CODE "Zyfx"
  CACHE STRING "Fork plugin type code, 4 chars (AU type + VST3/AAX UID seed)")
set(T3K_FORK_MANUFACTURER_CODE "ZYUS"
  CACHE STRING "Fork manufacturer code, 4 chars")
set(T3K_FORK_COMPANY_NAME "ZYUSOF"
  CACHE STRING "Fork vendor name (host menus, bundle IDs)")
set(T3K_FORK_LV2_URI "https://www.zyusof.net/zynamfx"
  CACHE STRING "Fork LV2 URI (permanent once released)")
set(T3K_FORK_CLAP_ID "net.zyusof.zynamfx"
  CACHE STRING "Fork CLAP ID (permanent once released)")
set(T3K_FORK_DATA_DIR "zynamfx"
  CACHE STRING "Fork app-data folder name (presets, settings, logs, models)")
# Which repository this build came from, and its channel. Development builds
# come from Gitea; public builds are cloned to GitHub. These drive the in-app
# logo link and the Settings build-identity line, so the two never mix.
set(T3K_FORK_REPO_URL "https://git.zyusof.net/zack/zynamfx"
  CACHE STRING "Repository URL the in-app logo and build identity point to")
set(T3K_FORK_BUILD_CHANNEL "development"
  CACHE STRING "Build channel: development (Gitea) or public (GitHub)")

if(T3K_FORK_NAMESPACED)
  set(T3K_DESKTOP_BUNDLE_ID "net.zyusof.zynamfx"
    CACHE STRING "Desktop app bundle identifier")
  set(PLUGIN_NAME "${T3K_FORK_DISPLAY_NAME}")
  set(PLUGIN_CODE "${T3K_FORK_PLUGIN_CODE}")
  set(T3K_DISPLAY_NAME "${T3K_FORK_DISPLAY_NAME}")
  set(T3K_PRODUCT_NAME "${T3K_FORK_PRODUCT_NAME}")
  set(T3K_COMPANY_NAME "${T3K_FORK_COMPANY_NAME}")
  set(T3K_MANUFACTURER_CODE "${T3K_FORK_MANUFACTURER_CODE}")
  set(T3K_LV2_URI "${T3K_FORK_LV2_URI}")
  set(T3K_CLAP_ID "${T3K_FORK_CLAP_ID}")
  set(T3K_APP_DATA_DIR_NAME "${T3K_FORK_DATA_DIR}")
  set(T3K_REPO_URL "${T3K_FORK_REPO_URL}")
  set(T3K_BUILD_CHANNEL "${T3K_FORK_BUILD_CHANNEL}")
endif()
