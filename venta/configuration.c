/*
 Author: Alexander Knauer <a-x-e@gmx.net>
 License: Apache 2.0
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libconfig.h>
#include <utlist.h>
#include <limits.h>

#include <digitalSTROM/dsuid.h>
#include <dsvdc/dsvdc.h>

#include "venta.h"

int read_config() {
  config_t config;
  struct stat statbuf;
  int i;
  char *sval;
  int ivalue;
  bool bvalue;

  if (stat(g_cfgfile, &statbuf) != 0) {
    vdc_report(LOG_ERR, "Could not find configuration file %s\n", g_cfgfile);
    return -1;
  }
  if (!S_ISREG(statbuf.st_mode)) {
    vdc_report(LOG_ERR, "Configuration file \"%s\" is not a regular file", g_cfgfile);
    return -2;
  }

  config_init(&config);
  if (!config_read_file(&config, g_cfgfile)) {
    vdc_report(LOG_ERR, "Error in configuration: l.%d %s\n", config_error_line(&config), config_error_text(&config));
    config_destroy(&config);
    return -3;
  }

  if (config_lookup_string(&config, "vdcdsuid", (const char **) &sval))
    strncpy(g_vdc_dsuid, sval, sizeof(g_vdc_dsuid));
  if (config_lookup_string(&config, "libdsuid", (const char **) &sval))
    strncpy(g_lib_dsuid, sval, sizeof(g_lib_dsuid));
  if (config_lookup_int(&config, "reload_values", (int *) &ivalue))
    g_reload_values = ivalue;
  if (config_lookup_int(&config, "zone_id", (int *) &ivalue))
    g_default_zoneID = ivalue;
  if (config_lookup_int(&config, "debug", (int *) &ivalue)) {
    if (ivalue <= 10) {
      vdc_set_debugLevel(ivalue);
    }
  }
  if (config_lookup_string(&config, "humifier.name", (const char **) &sval))
    venta.humifier.name = strdup(sval);
  if (config_lookup_string(&config, "humifier.id", (const char **) &sval)) {
    venta.humifier.id = strdup(sval);
  } else {
    vdc_report(LOG_ERR, "mandatory parameter 'id' in section humifier: is not set in venta.cfg\n");  
    exit(0);
  }
  if (config_lookup_string(&config, "humifier.ip", (const char **) &sval)) {
    venta.humifier.ip = strdup(sval);
  } else {
    vdc_report(LOG_ERR, "mandatory parameter 'ip' in section humifier: is not set in venta.cfg\n");  
    exit(0);
  }
 
  char path[128];  
  i = 0;
  while(1) {
    sprintf(path, "sensor_values.s%d", i);
    if (i < MAX_SENSOR_VALUES && config_lookup(&config, path)) {
      sensor_value_t* value = &venta.humifier.sensor_values[i];
      
      sprintf(path, "sensor_values.s%d.value_name", i);
      if (config_lookup_string(&config, path, (const char **) &sval)) {
        value->value_name = strdup(sval);  
      } else {
        value->value_name = strdup("");  
      }
      
      sprintf(path, "sensor_values.s%d.sensor_type", i);
      if (config_lookup_int(&config, path, (int *) &ivalue))
        value->sensor_type = ivalue;  
      
      sprintf(path, "sensor_values.s%d.sensor_usage", i);
      if (config_lookup_int(&config, path, (int *) &ivalue))
        value->sensor_usage = ivalue;  
      
      value->is_active = true;
      
      i++;
    } else {
      while (i < MAX_SENSOR_VALUES) {
        venta.humifier.sensor_values[i].is_active = false;
        i++;
      }        
      break;
    }
  }

  venta.humifier.configured_scenes = strdup("-");
  i = 0;
  while(1) {
    sprintf(path, "humifier.scenes.s%d", i);
    if (i < MAX_SCENES && config_lookup(&config, path)) {
      scene_t* value = &venta.humifier.scenes[i];
      
      sprintf(path, "humifier.scenes.s%d.dsId", i);
      config_lookup_int(&config, path, &ivalue);
      value->dsId = ivalue;
      char str[4];
      sprintf(str, "%d", ivalue);
      venta.humifier.configured_scenes = realloc(venta.humifier.configured_scenes, strlen(venta.humifier.configured_scenes)+4);
      strcat(venta.humifier.configured_scenes, str);
      strcat(venta.humifier.configured_scenes, "-");
         
      sprintf(path, "humifier.scenes.s%d.fan", i);
      if (config_lookup_int(&config, path, &ivalue)) {
        value->fan = ivalue;
      } else {
        value->fan = -1;
      }
      
      sprintf(path, "humifier.scenes.s%d.mode_automatic", i);
      if (config_lookup_int(&config, path, &ivalue)) {
        value->mode_automatic = ivalue;
      } else {
        value->mode_automatic = -1;
      }
      
      sprintf(path, "humifier.scenes.s%d.mode_sleep", i);
      if (config_lookup_int(&config, path, &ivalue)) {
        value->mode_sleep = ivalue;
      } else {
        value->mode_sleep = -1;
      }
      
      i++;
    } else {
      while (i < MAX_SCENES) {
        venta.humifier.scenes[i].dsId = -1;
        i++;
      }        
      break;
    }
  }

  if (g_cfgfile != NULL) {
    config_destroy(&config);
  }

  venta_humifier_t* humifier = &venta.humifier;
  if (humifier->id) {
    char buffer[128];
    strcpy(buffer, humifier->id);
    
    humifier_device = malloc(sizeof(venta_vdcd_t));
    if (!humifier_device) {
      return VENTA_OUT_OF_MEMORY;
    }
    memset(humifier_device, 0, sizeof(venta_vdcd_t));

    humifier_device->announced = false;
    humifier_device->present = true; 
    humifier_device->humifier = humifier;

    dsuid_generate_v3_from_namespace(DSUID_NS_IEEE_MAC, buffer, &humifier_device->dsuid);
    dsuid_to_string(&humifier_device->dsuid, humifier_device->dsuidstring);
  }

	return 0;
}

int write_config() {
  config_t config;
  config_setting_t* cfg_root;
  config_setting_t* setting;
  config_setting_t* humifiersetting;
  int i;

  config_init(&config);
  cfg_root = config_root_setting(&config);

  setting = config_setting_add(cfg_root, "vdcdsuid", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(cfg_root, "vdcdsuid");
  }
  config_setting_set_string(setting, g_vdc_dsuid);
  
  if (g_lib_dsuid != NULL && strcmp(g_lib_dsuid,"") != 0) { 
    setting = config_setting_add(cfg_root, "libdsuid", CONFIG_TYPE_STRING);
    if (setting == NULL) {
      setting = config_setting_get_member(cfg_root, "libdsuid");
    }  
    config_setting_set_string(setting, g_lib_dsuid);
  }
 
  setting = config_setting_add(cfg_root, "reload_values", CONFIG_TYPE_INT);
  if (setting == NULL) {
    setting = config_setting_get_member(cfg_root, "reload_values");
  }
  config_setting_set_int(setting, g_reload_values);

  setting = config_setting_add(cfg_root, "zone_id", CONFIG_TYPE_INT);
  if (setting == NULL) {
    setting = config_setting_get_member(cfg_root, "zone_id");
  }
  config_setting_set_int(setting, g_default_zoneID);

  setting = config_setting_add(cfg_root, "debug", CONFIG_TYPE_INT);
  if (setting == NULL) {
    setting = config_setting_get_member(cfg_root, "debug");
  }
  config_setting_set_int(setting, vdc_get_debugLevel());

  humifiersetting = config_setting_add(cfg_root, "humifier", CONFIG_TYPE_GROUP);
    
  setting = config_setting_add(humifiersetting, "id", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(humifiersetting, "id");
  }
  config_setting_set_string(setting, venta.humifier.id);

  setting = config_setting_add(humifiersetting, "name", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(humifiersetting, "name");
  }
  config_setting_set_string(setting, venta.humifier.name);
  
  setting = config_setting_add(humifiersetting, "ip", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(humifiersetting, "ip");
  }
  config_setting_set_string(setting, venta.humifier.ip);
    
  char path[128];
  sprintf(path, "scenes");   
  config_setting_t *scenes_path = config_setting_add(humifiersetting, path, CONFIG_TYPE_GROUP);
  
  if (scenes_path == NULL) {
    scenes_path = config_setting_get_member(humifiersetting, path);
  }

  i = 0;
  while(1) {
    if (i < MAX_SCENES && venta.humifier.scenes[i].dsId != -1) {
      scene_t* value = &venta.humifier.scenes[i];
     
      sprintf(path, "s%d", i);   
      config_setting_t *v = config_setting_add(scenes_path, path, CONFIG_TYPE_GROUP);

      if (v == NULL) {
        v = config_setting_get_member(scenes_path, path);
      }
      
      setting = config_setting_add(v, "dsId", CONFIG_TYPE_INT);
      if (setting == NULL) {
        setting = config_setting_get_member(v, "dsId");
      }
      config_setting_set_int(setting, value->dsId);      
      
      if (value->fan > -1) {
        setting = config_setting_add(v, "fan", CONFIG_TYPE_INT);
        if (setting == NULL) {
          setting = config_setting_get_member(v, "fan");
        }
        config_setting_set_int(setting, value->fan);      
      }
      
      if (value->mode_sleep > -1) {
        setting = config_setting_add(v, "mode_sleep", CONFIG_TYPE_INT);
        if (setting == NULL) {
          setting = config_setting_get_member(v, "mode_sleep");
        }
        config_setting_set_int(setting, value->mode_sleep);      
      }
      
      if (value->mode_automatic > -1) {
        setting = config_setting_add(v, "mode_automatic", CONFIG_TYPE_INT);
        if (setting == NULL) {
          setting = config_setting_get_member(v, "mode_automatic");
        }
        config_setting_set_int(setting, value->mode_automatic);      
      }
      
      i++;
    } else {
      break;
    }
  }
  
  sprintf(path, "sensor_values");   
  config_setting_t *sensor_values_path = config_setting_add(cfg_root, path, CONFIG_TYPE_GROUP);

  if (sensor_values_path == NULL) {
    sensor_values_path = config_setting_get_member(setting, path);
  }
  
  i = 0;
  while(1) {
    if (i < MAX_SENSOR_VALUES && venta.humifier.sensor_values[i].value_name != NULL) {
      sensor_value_t* value = &venta.humifier.sensor_values[i];
      
      sprintf(path, "s%d", i);   
      config_setting_t *v = config_setting_add(sensor_values_path, path, CONFIG_TYPE_GROUP);
      
      setting = config_setting_add(v, "value_name", CONFIG_TYPE_STRING);
      if (setting == NULL) {
        setting = config_setting_get_member(v, "value_name");
      }
      config_setting_set_string(setting, value->value_name);
      
      setting = config_setting_add(v, "sensor_type", CONFIG_TYPE_INT);
      if (setting == NULL) {
        setting = config_setting_get_member(v, "sensor_type");
      }
      config_setting_set_int(setting, value->sensor_type);
      
      setting = config_setting_add(v, "sensor_usage", CONFIG_TYPE_INT);
      if (setting == NULL) {
        setting = config_setting_get_member(v, "sensor_usage");
      }
      config_setting_set_int(setting, value->sensor_usage);
      
      i++;
    } else {
      break;
    }   
  } 

  char tmpfile[PATH_MAX];
  sprintf(tmpfile, "%s.cfg.new", g_cfgfile);

  int ret = config_write_file(&config, tmpfile);
  if (!ret) {
    vdc_report(LOG_ERR, "Error while writing new configuration file %s\n", tmpfile);
    unlink(tmpfile);
  } else {
    rename(tmpfile, g_cfgfile);
  }

  config_destroy(&config);

  return 0;
}

sensor_value_t* find_sensor_value_by_name(char *key) {
  sensor_value_t* value;
  for (int i = 0; i < MAX_SENSOR_VALUES; i++) {
    value = &venta.humifier.sensor_values[i];
    if (value->value_name != NULL && strcasecmp(key, value->value_name) == 0) {
        return value;
    }
  }
  
  return NULL;
}

void save_scene(int scene) {
  int i = 0;
  scene_t* value = NULL;
  while(1) {
    if (i < MAX_SCENES && venta.humifier.scenes[i].dsId != -1) {
      //scene is already configured, so we will reconfigure the existing one
      if (venta.humifier.scenes[i].dsId == scene) {
        value = &venta.humifier.scenes[i];
        break;
      }
    } else if (i < MAX_SCENES)  {
      //scene is currently not configured and we have less than MAX_SCENES configured in config file, so we add a new scene config
      value = &venta.humifier.scenes[i];
      break;
    } else {
      //scene is not already configured in config file, but we have already MAX_SCENES configured in config file, so we ignore the save scene request
      break;
    }
    
    i++;
  }
  
  if (value != NULL) {
    value->dsId = scene;
  }
}
