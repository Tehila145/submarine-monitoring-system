#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H
/* TODO(team): choose sensible first-boot defaults for YOUR sensors and units.
 * The specification (§2.6) requires defaults to exist but does not fix values.
 * Keep the macro names; set the numbers. Until set, first boot loads zeros
 * (every channel would read as Error), so fill these before on-target Task 15. */
#define DEF_TEMP_NORM_LO   /* TODO */ 0
#define DEF_TEMP_NORM_HI   /* TODO */ 0
#define DEF_TEMP_WARN_LO   /* TODO */ 0
#define DEF_TEMP_WARN_HI   /* TODO */ 0
#define DEF_HUM_NORM_LO    /* TODO */ 0
#define DEF_HUM_WARN_LO    /* TODO */ 0
#define DEF_LIGHT_NORM_LO  /* TODO */ 0
#define DEF_LIGHT_WARN_LO  /* TODO */ 0
#define DEF_BATT_NORM_LO   /* TODO */ 0
#define DEF_BATT_WARN_LO   /* TODO */ 0
#endif
