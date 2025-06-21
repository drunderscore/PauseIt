/**
 * Do not edit below this line!
 */
public Extension __ext_rewind =
{
	name = "PauseItRewind",
	file = "PauseItRewind.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

/**
 * Returns whether or not the server is paused.
 *
 * @return  True if the server is paused, false otherwise.
 */
native bool IsPaused();

/**
 * Sets whether or not the server is paused.
 *
 * @param paused    True if the server should pause, false if the server should unpause.
 */
native void SetPaused(paused);
