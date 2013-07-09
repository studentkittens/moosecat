#ifndef MC_ZEROCONF_BROWSE_HH
#define MC_ZEROCONF_BROWSE_HH

struct mc_ZeroconfBrowser;

/**
 * @brief Instance a new Zeroconf-Browser
 *
 * @return a newly allocated Browser, free with mc_zeroconf_destroy()
 */
struct mc_ZeroconfBrowser * mc_zeroconf_new(void);

/**
 * @brief Destrpoy an allocagted Zeroconf Browser
 *
 * @param self Browser to deallocate
 */
void mc_zeroconf_destroy(struct mc_ZeroconfBrowser * self);

#endif /* end of include guard: MC_ZEROCONF_BROWSE_HH */

