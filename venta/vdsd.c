/*
 Author: Alexander Knauer <a-x-e@gmx.net>
 License: Apache 2.0
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include <libconfig.h>
#include <curl/curl.h>
#include <json.h>
#include <utlist.h>

#include <digitalSTROM/dsuid.h>
#include <dsvdc/dsvdc.h>

#include "venta.h"

#include "incbin.h"
INCBIN_EXTERN(VentaHumifier16);
INCBIN_EXTERN(VentaHumifier48);

void vdc_ping_cb(dsvdc_t *handle __attribute__((unused)), const char *dsuid, void *userdata __attribute__((unused))) {
  int ret;
  vdc_report(LOG_NOTICE, "received ping for dsuid %s\n", dsuid);
  if (strcasecmp(dsuid, g_vdc_dsuid) == 0) {
    ret = dsvdc_send_pong(handle, dsuid);
    vdc_report(LOG_NOTICE, "sent pong for vdc %s / return code %d\n", dsuid, ret);
    return;
  }
  if (strcasecmp(dsuid, g_lib_dsuid) == 0) {
    ret = dsvdc_send_pong(handle, dsuid);
    vdc_report(LOG_NOTICE, "sent pong for lib-dsuid %s / return code %d\n", dsuid, ret);
    return;
  }
  
  if (strcasecmp(dsuid, humifier_device->dsuidstring) == 0) {
      ret = dsvdc_send_pong(handle, humifier_device->dsuidstring);
      vdc_report(LOG_NOTICE, "sent pong for device %s / return code %d\n", dsuid, ret);
    return;
  }
  vdc_report(LOG_WARNING, "ping: no matching dsuid %s registered\n", dsuid);
}

void vdc_announce_device_cb(dsvdc_t *handle __attribute__((unused)), int code, void *arg, void *userdata __attribute__((unused))) {
  vdc_report(LOG_INFO, "announcement of device %s returned code: %d\n", (char *) arg, code);
}

void vdc_announce_container_cb(dsvdc_t *handle __attribute__((unused)), int code, void *arg, void *userdata __attribute__((unused))) {
  vdc_report(LOG_INFO, "announcement of container %s returned code: %d\n", (char *) arg, code);
}

void vdc_new_session_cb(dsvdc_t *handle, void *userdata) {
  (void)userdata;
  int ret;
  ret = dsvdc_announce_container(handle,
                                 g_vdc_dsuid,
                                 (void *) g_vdc_dsuid,
                                 vdc_announce_container_cb);
  if (ret != DSVDC_OK) {
    vdc_report(LOG_WARNING, "dsvdc_announce_container returned error %d\n", ret);
    return;
  }

  vdc_report(LOG_INFO, "new session, container announced successfully\n");
}

void vdc_end_session_cb(dsvdc_t *handle, void *userdata) {
  (void)userdata;
  int ret;

  vdc_report(LOG_WARNING, "end of session\n");
}

bool vdc_remove_cb(dsvdc_t *handle __attribute__((unused)), const char *dsuid, void *userdata) {
  (void)userdata;
  vdc_report(LOG_INFO, "received remove for dsuid %s\n", dsuid);

  // TODO: what to do on remove?

  return true;
}

void vdc_request_generic_cb(dsvdc_t *handle __attribute__((unused)), char *dsuid, char *method_name, dsvdc_property_t *property, const dsvdc_property_t *properties,  void *userdata) {
  int ret;
  uint8_t code = DSVDC_ERR_NOT_IMPLEMENTED;
  size_t i;
  
  vdc_report(LOG_INFO, "received request generic for dsuid %s, method name %s\n", dsuid, method_name);
  
  if (strcasecmp(humifier_device->dsuidstring, dsuid) == 0) {
    for (i = 0; i < dsvdc_property_get_num_properties(properties); i++) {
      char *name;
      ret = dsvdc_property_get_name(properties, i, &name);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "request_generic_cb: error getting property name\n");
        code = DSVDC_ERR_MISSING_DATA;
        break;
      }
      if (!name) {
        vdc_report(LOG_ERR, "request_generic_cb: not handling wildcard properties\n");
        code = DSVDC_ERR_NOT_IMPLEMENTED;
        break;
      }

      vdc_report(LOG_INFO, "request generic for name=\"%s\"\n", name);

      if (strcmp(name, "id") == 0) {
        char *id;
        ret = dsvdc_property_get_string(properties, i, &id);
        if (ret != DSVDC_OK) {
          vdc_report(LOG_ERR, "request_generic_cb: error getting property value from property %s\n", name);
          code = DSVDC_ERR_INVALID_VALUE_TYPE;
          break;
        }
        
        scene_t *scene_data;
        scene_data = malloc(sizeof(scene_t));
        if (!scene_data) {
          return;
        }
        memset(scene_data, 0, sizeof(scene_t));
          
        if(strcasecmp(id, "ActTurnOn") == 0) {
          vdc_report(LOG_DEBUG, "Exec Start Cabin command");
          //klafs_power_on();
          
        } else {
          vdc_report(LOG_NOTICE, "request_generic_cb: command = %s not implemented\n", id);
        }
      }
    }      
  }
}

void vdc_savescene_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t scene, int32_t *group, int32_t *zone_id, void *userdata) {
  vdc_report(LOG_NOTICE, "save scene %d\n", scene);
  if (strcasecmp(humifier_device->dsuidstring, *dsuid) == 0) {
    
  }
}
  
void vdc_callscene_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t scene, bool force, int32_t *group, int32_t *zone_id, void *userdata) {
  if (strcasecmp(humifier_device->dsuidstring, *dsuid) == 0) {
    vdc_report(LOG_NOTICE, "called scene: %d\n", scene);
    
    bool is_configured = is_scene_configured(scene);
    if(is_configured) {
      scene_t* scene_data = get_scene_configuration(scene);

      if (scene_data != NULL) {
        int i = 0;
        venta_get_data();
        if (scene_data->mode_sleep > 0) {            
          venta_set_mode_sleep(true);
        } else if (scene_data->mode_sleep == 0) {            
          venta_set_mode_sleep(false);
        }
       
        if (scene_data->mode_automatic > 0) {
          venta_set_mode_automatic(true);          
        } else if (scene_data->mode_automatic == 0) {
          venta_set_mode_automatic(false);          
        }
          
        if (scene_data->fan == 1) {            
          venta_set_fan(4);
          venta_set_fan(4);
        } else if (scene_data->fan == 2) {          
          if (humifier_current_values->fan == 1) {
            venta_set_fan(3);
          } else if (humifier_current_values->fan == 3) {
            venta_set_fan(4);
          }
        } else if (scene_data->fan == 3) {     
          venta_set_fan(3);
          venta_set_fan(3);
        }
      } else {
        vdc_report(LOG_INFO, "memory allocation for scene data failed!");   
      }
    } else {
      vdc_report(LOG_INFO, "called scene is not configured");  
    }
  }
}

void vdc_setprop_cb(dsvdc_t *handle, const char *dsuid, dsvdc_property_t *property, const dsvdc_property_t *properties, void *userdata) {
  (void) userdata;
  int ret;
  uint8_t code = DSVDC_ERR_NOT_IMPLEMENTED;
  size_t i;
  vdc_report(LOG_INFO, "set property request for dsuid \"%s\"\n", dsuid);

  /*
   * Properties for the VDC
   */
  if (strcasecmp(g_vdc_dsuid, dsuid) == 0) {
    for (i = 0; i < dsvdc_property_get_num_properties(properties); i++) {
      char *name;
      ret = dsvdc_property_get_name(properties, i, &name);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "setprop_cb: error getting property name\n");
        code = DSVDC_ERR_MISSING_DATA;
        break;
      }
      if (!name) {
        vdc_report(LOG_ERR, "setprop_cb: not handling wildcard properties\n");
        code = DSVDC_ERR_NOT_IMPLEMENTED;
        break;
      }

      vdc_report(LOG_INFO, "set request for name=\"%s\"\n", name);

      if (strcmp(name, "zoneID") == 0) {
        uint64_t zoneID;
        ret = dsvdc_property_get_uint(properties, i, &zoneID);
        if (ret != DSVDC_OK) {
          vdc_report(LOG_ERR, "setprop_cb: error getting property value from property %s\n", name);
          code = DSVDC_ERR_INVALID_VALUE_TYPE;
          break;
        }
        vdc_report(LOG_NOTICE, "setprop_cb: \"%s\" = %d\n", name, zoneID);
        g_default_zoneID = zoneID;
        code = DSVDC_OK;
      } else {
        code = DSVDC_ERR_NOT_FOUND;
        break;
      }

      free(name);
    }

    if (code == DSVDC_OK) {
      write_config();
    }

    dsvdc_send_set_property_response(handle, property, code);
    return;
  } 
  
  if (strcasecmp(humifier_device->dsuidstring, dsuid) != 0) {	  
    vdc_report(LOG_WARNING, "set property: unhandled dsuid %s\n", dsuid);
    dsvdc_property_free(property);
    return;
  }

  /*
   * Properties for the VDSD's
   */
  pthread_mutex_lock(&g_network_mutex);
  for (i = 0; i < dsvdc_property_get_num_properties(properties); i++) {
    char *name;

    int ret = dsvdc_property_get_name(properties, i, &name);
    if (ret != DSVDC_OK) {
      vdc_report(LOG_ERR, "getprop_cb: error getting property name, abort\n");
      dsvdc_send_get_property_response(handle, property);
      pthread_mutex_unlock(&g_network_mutex);
      return;
    }
    if (!name) {
      vdc_report(LOG_ERR, "getprop_cb: not yet handling wildcard properties\n");
      //dsvdc_send_property_response(handle, property);
      continue;
    }
    vdc_report(LOG_NOTICE, "get request name!!=\"%s\"\n", name);

    if (strcmp(name, "zoneID") == 0) {
      uint64_t zoneID;
      ret = dsvdc_property_get_uint(properties, i, &zoneID);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "setprop_cb: error getting property value from property %s\n", name);
        code = DSVDC_ERR_INVALID_VALUE_TYPE;
        break;
      }
      vdc_report(LOG_NOTICE, "setprop_cb: \"%s\" = %d\n", name, zoneID);
      humifier_device->humifier->zoneID = zoneID;
      code = DSVDC_OK;
    } else {
      code = DSVDC_OK;
    }
      

    free(name);
  }
  pthread_mutex_unlock(&g_network_mutex);

  dsvdc_send_set_property_response(handle, property, code);
}

dsvdc_property_t* create_action_property(char *id, char *title, char* description) {
  dsvdc_property_t *nProp;
  if (dsvdc_property_new(&nProp) != DSVDC_OK) {
    vdc_report(LOG_ERR, "failed to allocate reply property");
    return NULL;
  }
               
  dsvdc_property_add_string (nProp, "id", id);
  dsvdc_property_add_string (nProp, "action", id);
  dsvdc_property_add_string (nProp, "title", title);
  dsvdc_property_add_string (nProp, "description", description);
  
  return nProp;
}

void vdc_getprop_cb(dsvdc_t *handle, const char *dsuid, dsvdc_property_t *property, const dsvdc_property_t *query, void *userdata) {
  (void) userdata;
  int ret;
  size_t i;
  char *name;
  
  vdc_report(LOG_INFO, "get property for dsuid: %s\n", dsuid);

  /*
   * Properties for the VDC
   */
  if (strcasecmp(g_vdc_dsuid, dsuid) == 0) {
    for (i = 0; i < dsvdc_property_get_num_properties(query); i++) {

      int ret = dsvdc_property_get_name(query, i, &name);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "getprop_cb: error getting property name, abort\n");
        dsvdc_send_get_property_response(handle, property);
        return;
      }
      if (!name) {
        vdc_report(LOG_ERR, "getprop_cb: not yet handling wildcard properties\n");
        dsvdc_send_get_property_response(handle, property);
        return;
      }
      vdc_report(LOG_NOTICE, "get request name=\"%s\"\n", name);

      if ((strcmp(name, "hardwareGuid") == 0) || (strcmp(name, "displayId") == 0)) {
        char info[256];
        char buffer[32];
        size_t n;

        memset(info, 0, sizeof(info));
        if (strcmp(name, "hardwareGuid") == 0) {
          strcpy(info, "humifier-id:");
        }
        sprintf(buffer, "%d", venta.humifier.id);
        strcat(info, buffer);
        dsvdc_property_add_string(property, name, info);

      } else if (strcmp(name, "vendorId") == 0) {
      } else if (strcmp(name, "oemGuid") == 0) {

      } else if (strcmp(name, "implementationId") == 0) {
        dsvdc_property_add_string(property, name, "Venta Humifier");

      } else if (strcmp(name, "modelUID") == 0) {
        dsvdc_property_add_string(property, name, "Venta Humifier");

      } else if (strcmp(name, "modelGuid") == 0) {
        dsvdc_property_add_string(property, name, "Venta Humifier");

      } else if (strcmp(name, "name") == 0) {
        char info[256];
        strcpy(info, "Venta Humifier ");
        strcat(info, venta.humifier.name);
        dsvdc_property_add_string(property, name, info);

      } else if (strcmp(name, "model") == 0) {
        char hostname[HOST_NAME_MAX];
        gethostname(hostname, HOST_NAME_MAX);
        char servicename[HOST_NAME_MAX + 32];
        strcpy(servicename, "Venta Humifier Controller @");
        strcat(servicename, hostname);
        dsvdc_property_add_string(property, name, servicename);

      } else if (strcmp(name, "capabilities") == 0) {
        dsvdc_property_t *reply;
        ret = dsvdc_property_new(&reply);
        if (ret != DSVDC_OK) {
          vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
          free(name);
          continue;
        }
        dsvdc_property_add_bool(reply, "metering", false);
        dsvdc_property_add_bool(reply, "dynamicDefinitions", true);
        dsvdc_property_add_property(property, name, &reply);

      } else if (strcmp(name, "configURL") == 0) {


      } else if (strcmp(name, "zoneID") == 0) {
        dsvdc_property_add_uint(property, "zoneID", g_default_zoneID);

      /* user properties: user name, client_id, status */

      }
      free(name);
    }

    dsvdc_send_get_property_response(handle, property);
    return;
  } 

  if (strcasecmp(humifier_device->dsuidstring, dsuid) != 0) {	  
    vdc_report(LOG_WARNING, "get property: unhandled dsuid %s\n", dsuid);
    dsvdc_property_free(property);
    return;
  }

  /*
   * Properties for the VDSD's
   */
  pthread_mutex_lock(&g_network_mutex);
  for (i = 0; i < dsvdc_property_get_num_properties(query); i++) {

    int ret = dsvdc_property_get_name(query, i, &name);
    if (ret != DSVDC_OK) {
      vdc_report(LOG_ERR, "getprop_cb: error getting property name, abort\n");
      dsvdc_send_get_property_response(handle, property);
      pthread_mutex_unlock(&g_network_mutex);
      return;
    }
    if (!name) {
      vdc_report(LOG_ERR, "getprop_cb: not yet handling wildcard properties\n");
      //dsvdc_send_property_response(handle, property);
      continue;
    }
    vdc_report(LOG_NOTICE, "get request name=\"%s\"\n", name);

    if (strcmp(name, "primaryGroup") == 0) {
      dsvdc_property_add_uint(property, "primaryGroup", 8);
    } else if (strcmp(name, "zoneID") == 0) {
      dsvdc_property_add_uint(property, "zoneID", humifier_device->humifier->zoneID);
    } else if (strcmp(name, "buttonInputDescriptions") == 0) {
     

    } else if (strcmp(name, "buttonInputSettings") == 0) {
      
    } else if (strcmp(name, "dynamicActionDescriptions") == 0) { 
      dsvdc_property_t *reply;
      ret = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }

      int i = 0;
      char propIndex[64];     
      dsvdc_property_t *nProp;

      nProp = create_action_property("dynamic.ActTurnOn", "01-Einschalten", "01-Einschalten");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActTurnOn", &nProp);
      
      nProp = create_action_property("dynamic.ActTurnOff", "02-Ausschalten", "02-Ausschalten");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActTurnOff", &nProp);
      
      nProp = create_action_property("dynamic.ActFan1", "03-Fanspeed 1", "03-Fanspeed 1");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActFan1", &nProp);
      
      nProp = create_action_property("dynamic.ActFan2", "04-Fanspeed 2", "04-Fanspeed 2");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActFan1", &nProp);
      
      nProp = create_action_property("dynamic.ActFan3", "05-Fanspeed 3", "05-Fanspeed 3");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActFan1", &nProp);
      
      nProp = create_action_property("dynamic.ActSleepModeOn", "06-Schlafmodus an", "06-Schlafmodus an");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActSleepModeOn", &nProp);
      
      nProp = create_action_property("dynamic.ActSleepmodeOff", "07-Schlafmodus aus", "07-Schlafmodus aus");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActSleepModeOff", &nProp);
      
      nProp = create_action_property("dynamic.ActAutoModeOn", "08-Automatikmodus an", "08-Automatikmodus an");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActAutoModeOn", &nProp);
      
      nProp = create_action_property("dynamic.ActAutoModeOff", "09-Automatikmodus aus", "09-Automatikmodus aus");          
      //snprintf(propIndex, 64, "%d", i++);
      dsvdc_property_add_property(reply, "ActAutoModeOff", &nProp);
      

      dsvdc_property_add_property(property, name, &reply); 

    } else if (strcmp(name, "outputDescription") == 0) {
      dsvdc_property_t *reply;
      ret  = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }
      
      dsvdc_property_add_string(reply, "name", "Venta Humifier");
      dsvdc_property_add_uint(reply, "defaultGroup", 3);
      dsvdc_property_add_uint(reply, "function", 0);
      dsvdc_property_add_uint(reply, "outputUsage", 1);
      dsvdc_property_add_bool(reply, "variableRamp", true);
      dsvdc_property_add_uint(reply, "maxPower", 100);

      dsvdc_property_add_property(property, name, &reply);       

    } else if (strcmp(name, "outputSettings") == 0) {
      dsvdc_property_t *reply;
      dsvdc_property_t *groups;
      ret  = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }
      ret  = dsvdc_property_new(&groups);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate groups property for %s\n", name);
        free(name);
        continue;
      }

      dsvdc_property_add_bool(groups, "0", true);
      dsvdc_property_add_bool(groups, "3", true);
      dsvdc_property_add_property(reply, "groups", &groups);
      dsvdc_property_add_uint(reply, "mode", 1);
      dsvdc_property_add_bool(reply, "pushChanges", true);

      dsvdc_property_add_property(property, name, &reply);

    } else if (strcmp(name, "channelDescriptions") == 0) {
    } else if (strcmp(name, "channelSettings") == 0) {
    } else if (strcmp(name, "channelStates") == 0) {
    } else if (strcmp(name, "deviceStates") == 0) {
      
    } else if (strcmp(name, "deviceProperties") == 0) {
      
    } else if (strcmp(name, "devicePropertyDescriptions") == 0) {
    
    } else if (strcmp(name, "customActions") == 0) {

    } else if (strcmp(name, "binaryInputDescriptions") == 0) {      
        
    } else if (strcmp(name, "binaryInputSettings") == 0) {      
      
    } else if (strcmp(name, "sensorDescriptions") == 0) {
      dsvdc_property_t *reply;
      ret = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }

      int i = 0;
      char sensorName[64];
      char sensorIndex[64];
	    
      while(1) {
        if (humifier_device->humifier->sensor_values[i].is_active) {
          vdc_report(LOG_ERR, "************* %d %s\n", i, humifier_device->humifier->sensor_values[i].value_name);
        
          snprintf(sensorName, 64, "%s-%s", humifier_device->humifier->name, humifier_device->humifier->sensor_values[i].value_name);

          dsvdc_property_t *nProp;
          if (dsvdc_property_new(&nProp) != DSVDC_OK) {
            vdc_report(LOG_ERR, "failed to allocate reply property for %s/%s\n", name, sensorName);
            break;
          }
          dsvdc_property_add_string(nProp, "name", sensorName);
          dsvdc_property_add_uint(nProp, "sensorType", humifier_device->humifier->sensor_values[i].sensor_type);
          dsvdc_property_add_uint(nProp, "sensorUsage", humifier_device->humifier->sensor_values[i].sensor_usage);
          dsvdc_property_add_double(nProp, "aliveSignInterval", 300);

          snprintf(sensorIndex, 64, "%d", i);
          dsvdc_property_add_property(reply, sensorIndex, &nProp);

          vdc_report(LOG_INFO, "sensorDescription: dsuid %s sensorIndex %s: %s type %d usage %d\n", dsuid, sensorIndex, sensorName, humifier_device->humifier->sensor_values[i].sensor_type, humifier_device->humifier->sensor_values[i].sensor_usage); 
          
          i++;
        } else {
          break;
        }
      }

		  dsvdc_property_add_property(property, name, &reply);  
    } else if (strcmp(name, "sensorSettings") == 0) {
      dsvdc_property_t *reply;
      ret = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }

      char sensorIndex[64];
      int i = 0;
      while (1) {
        if (humifier_device->humifier->sensor_values[i].is_active) {
          dsvdc_property_t *nProp;
          if (dsvdc_property_new(&nProp) != DSVDC_OK) {
            vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
            break;
          }
          dsvdc_property_add_uint(nProp, "group", 8);
          dsvdc_property_add_uint(nProp, "minPushInterval", 5);
          dsvdc_property_add_double(nProp, "changesOnlyInterval", 5);

          snprintf(sensorIndex, 64, "%d", i);
          dsvdc_property_add_property(reply, sensorIndex, &nProp);
          
          i++;
        } else {
          break;
        }
      }
      dsvdc_property_add_property(property, name, &reply); 
      
      vdc_report(LOG_INFO, "sensorSettings: dsuid %s sensorIndex %s\n", dsuid, sensorIndex); 

    } else if (strcmp(name, "sensorStates") == 0) {
      dsvdc_property_t *reply;
      ret = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }

      int idx;
      char* sensorIndex;
      dsvdc_property_t *sensorRequest;
      dsvdc_property_get_property_by_index(query, 0, &sensorRequest);
      if (dsvdc_property_get_name(sensorRequest, 0, &sensorIndex) != DSVDC_OK) {
        vdc_report(LOG_DEBUG, "sensorStates: no index in request\n");
        idx = -1;
      } else {
        idx = strtol(sensorIndex, NULL, 10);
      }
      dsvdc_property_free(sensorRequest);

      time_t now = time(NULL);
      
      int i = 0;
      while (1) {
        if (humifier_device->humifier->sensor_values[i].is_active) {
          if (idx >= 0 && idx != i) {
            i++;
            continue;
          }

          dsvdc_property_t *nProp;
          if (dsvdc_property_new(&nProp) != DSVDC_OK) {
            vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
            break;
          }

          double val = humifier_device->humifier->sensor_values[i].value;

          dsvdc_property_add_double(nProp, "value", val);
          dsvdc_property_add_int(nProp, "age", now - humifier_device->humifier->sensor_values[i].last_query);
          dsvdc_property_add_int(nProp, "error", 0);

          char replyIndex[64];
          snprintf(replyIndex, 64, "%d", i);
          dsvdc_property_add_property(reply, replyIndex, &nProp);
          
          i++;
        } else {
          break;
        }
      }
      dsvdc_property_add_property(property, name, &reply);  

    } else if (strcmp(name, "binaryInputStates") == 0) {      

    } else if (strcmp(name, "name") == 0) {
      dsvdc_property_add_string(property, name, humifier_device->humifier->name);

    } else if (strcmp(name, "type") == 0) {
      dsvdc_property_add_string(property, name, "vDSD");

    } else if (strcmp(name, "model") == 0) {
      char info[256];
      strcpy(info, "Humifier");
     
      dsvdc_property_add_string(property, name, info);
    } else if (strcmp(name, "modelFeatures") == 0) {
      dsvdc_property_t *nProp;
      dsvdc_property_new(&nProp);
      dsvdc_property_add_bool(nProp, "dontcare", false);
      dsvdc_property_add_bool(nProp, "blink", false);
      dsvdc_property_add_bool(nProp, "outmode", false);
      dsvdc_property_add_bool(nProp, "jokerconfig", true);
      dsvdc_property_add_property(property, name, &nProp);
    } else if (strcmp(name, "modelUID") == 0) {      
      dsvdc_property_add_string(property, name, "Venta Humifier");

    } else if (strcmp(name, "modelVersion") == 0) {
      dsvdc_property_add_string(property, name, "0");

    } else if (strcmp(name, "deviceClass") == 0) {
    } else if (strcmp(name, "deviceClassVersion") == 0) {
    } else if (strcmp(name, "oemGuid") == 0) {
    } else if (strcmp(name, "oemModelGuid") == 0) {

    } else if (strcmp(name, "vendorId") == 0) {
      dsvdc_property_add_string(property, name, "vendor: Venta");

    } else if (strcmp(name, "vendorName") == 0) {
      dsvdc_property_add_string(property, name, "Venta");

    } else if (strcmp(name, "vendorGuid") == 0) {
      char info[256];
      strcpy(info, "Venta vDC ");
      strcat(info, humifier_device->humifier->id);
      dsvdc_property_add_string(property, name, info);

    } else if (strcmp(name, "hardwareVersion") == 0) {
      dsvdc_property_add_string(property, name, "0.0.0");

    } else if (strcmp(name, "configURL") == 0) {
      dsvdc_property_add_string(property, name, "");

    } else if (strcmp(name, "hardwareModelGuid") == 0) {
      dsvdc_property_add_string(property, name, "");

    } else if (strcmp(name, "deviceIcon16") == 0) {
        dsvdc_property_add_bytes(property, name, gVentaHumifier16Data, gVentaHumifier16Size);

    } else if (strcmp(name, "deviceIcon48") == 0) {
        dsvdc_property_add_bytes(property, name, gVentaHumifier48Data, gVentaHumifier48Size);

    } else if (strcmp(name, "deviceIconName") == 0) {
      char info[256];
      strcpy(info, "venta-humifier-16.png");
      
      dsvdc_property_add_string(property, name, info);

    } else {
      vdc_report(LOG_WARNING, "get property handler: unhandled name=\"%s\"\n", name);
    }

    free(name);
  }

  pthread_mutex_unlock(&g_network_mutex);
  dsvdc_send_get_property_response(handle, property);
}
