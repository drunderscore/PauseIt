#pragma once

#define SMEXT_CONF_NAME "Pause It: Rewind"
#define SMEXT_CONF_DESCRIPTION "Fix bugs during server processing and simulation whilst paused"
#define SMEXT_CONF_VERSION "1.0.0.0"
#define SMEXT_CONF_AUTHOR "resin"
#define SMEXT_CONF_URL "https://jame.xyz"
#define SMEXT_CONF_LOGTAG "PAUSEIT"
#define SMEXT_CONF_LICENSE "BSD-2-Clause"
#define SMEXT_CONF_DATESTRING ""

#define SMEXT_LINK(name) SDKExtension* g_pExtensionIface = name;

/**
 * @brief Sets whether or not this plugin required Metamod.
 * NOTE: Uncomment to enable, comment to disable.
 */
#define SMEXT_CONF_METAMOD

/** Enable interfaces you want to use here by uncommenting lines */
// #define SMEXT_ENABLE_FORWARDSYS
// #define SMEXT_ENABLE_HANDLESYS
// #define SMEXT_ENABLE_PLAYERHELPERS
// #define SMEXT_ENABLE_DBMANAGER
// #define SMEXT_ENABLE_GAMECONF
#define SMEXT_ENABLE_MEMUTILS
// #define SMEXT_ENABLE_GAMEHELPERS
// #define SMEXT_ENABLE_TIMERSYS
// #define SMEXT_ENABLE_THREADER
// #define SMEXT_ENABLE_LIBSYS
// #define SMEXT_ENABLE_MENUS
// #define SMEXT_ENABLE_ADTFACTORY
// #define SMEXT_ENABLE_PLUGINSYS
// #define SMEXT_ENABLE_ADMINSYS
// #define SMEXT_ENABLE_TEXTPARSERS
// #define SMEXT_ENABLE_USERMSGS
// #define SMEXT_ENABLE_TRANSLATOR
// #define SMEXT_ENABLE_ROOTCONSOLEMENU
