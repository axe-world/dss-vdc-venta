/*
 Author: Alexander Knauer <a-x-e@gmx.net>
 License: Apache 2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>

#include <digitalSTROM/dsuid.h>
#include <dsvdc/dsvdc.h>

#define MAX_SENSOR_VALUES 15
#define MAX_BINARY_VALUES 15
#define MAX_SCENES 128

typedef struct scene {
  int dsId;
  int current_temperature;
  int current_humidity;
  int target_humidity;
  int fan;
  int mode_automatic;
  int mode_sleep;
} scene_t;

typedef struct sensor_value {
  bool is_active;
  char *value_name;
  int sensor_type;
  int sensor_usage;
  double value;
  double last_value;
  time_t last_query;
  time_t last_reported;
} sensor_value_t;

typedef struct venta_humifier {
  dsuid_t dsuid;
  char *id;
  char *name;
  char *ip;
  char *configured_scenes;
  sensor_value_t sensor_values[MAX_SENSOR_VALUES];
  scene_t scenes[MAX_SCENES];
  uint16_t zoneID;
} venta_humifier_t;

typedef struct venta_data {
  venta_humifier_t humifier;
} venta_data_t;

typedef struct venta_vdcd {
  struct venta_vdcd* next;
  dsuid_t dsuid;
  char dsuidstring[36];
  bool announced;
  bool presentSignaled;
  bool present;
  venta_humifier_t* humifier;
} venta_vdcd_t;

#define VENTA_OK 0
#define VENTA_OUT_OF_MEMORY -1
#define VENTA_BAD_CONFIG -12
#define VENTA_CONNECT_FAILED -13
#define VENTA_GETMEASURE_FAILED -14
#define VENTA_CONFIGCHANGE_FAILED -15

extern const char *g_cfgfile;
extern int g_shutdown_flag;
extern venta_data_t venta;
extern venta_vdcd_t* humifier_device;
extern pthread_mutex_t g_network_mutex;
extern scene_t* humifier_current_values;

extern char g_vdc_modeluid[33];
extern char g_vdc_dsuid[35];
extern char g_lib_dsuid[35];

extern time_t g_reload_values;
extern int g_default_zoneID;

extern void vdc_new_session_cb(dsvdc_t *handle __attribute__((unused)), void *userdata);
extern void vdc_ping_cb(dsvdc_t *handle __attribute__((unused)), const char *dsuid, void *userdata __attribute__((unused)));
extern void vdc_announce_device_cb(dsvdc_t *handle __attribute__((unused)), int code, void *arg, void *userdata __attribute__((unused)));
extern void vdc_announce_container_cb(dsvdc_t *handle __attribute__((unused)), int code, void *arg, void *userdata __attribute__((unused)));
extern void vdc_end_session_cb(dsvdc_t *handle __attribute__((unused)), void *userdata);
extern bool vdc_remove_cb(dsvdc_t *handle __attribute__((unused)), const char *dsuid, void *userdata);
extern void vdc_blink_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t *group, int32_t *zone_id, void *userdata);
extern void vdc_getprop_cb(dsvdc_t *handle, const char *dsuid, dsvdc_property_t *property, const dsvdc_property_t *query, void *userdata);
extern void vdc_setprop_cb(dsvdc_t *handle, const char *dsuid, dsvdc_property_t *property, const dsvdc_property_t *properties, void *userdata);
extern void vdc_callscene_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t scene, bool force, int32_t *group, int32_t *zone_id, void *userdata);
extern void vdc_savescene_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t scene, int32_t *group, int32_t *zone_id, void *userdata);
extern void vdc_request_generic_cb(dsvdc_t *handle __attribute__((unused)), char *dsuid, char *method_name, dsvdc_property_t *property, const dsvdc_property_t *properties,  void *userdata);

int venta_get_data();
int venta_set_fan(int btn);
int venta_set_mode_automatic(bool on);
int venta_set_mode_sleep(bool on);
int venta_power_on_off();
int venta_toggle_automode(scene_t *scene_data);
int venta_toggle_sleepmod(scene_t *scene_data);
int venta_change_target_humidity(scene_t *scene_data);
int venta_change_fan(scene_t *scene_data);
void push_sensor_data();
void push_device_states();
bool is_scene_configured();
scene_t* get_scene_configuration(int scene);
int decodeURIComponent (char *sSource, char *sDest);
sensor_value_t* find_sensor_value_by_name(char *key);
void save_scene(int scene);

int write_config();
int read_config();

void vdc_init_report();
void vdc_set_debugLevel(int debug);
int vdc_get_debugLevel();
void vdc_report(int errlevel, const char *fmt, ... );
void vdc_report_extraLevel(int errlevel, int maxErrlevel, const char *fmt, ... );
