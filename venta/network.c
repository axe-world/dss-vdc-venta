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
#include <ctype.h>
#include <pthread.h>

#include <curl/curl.h>
#include <json.h>
#include <utlist.h>

#include <digitalSTROM/dsuid.h>
#include <dsvdc/dsvdc.h>

#include "venta.h"

struct memory_struct {
  char *memory;
  size_t size;
};

struct data {
  char trace_ascii; /* 1 or 0 */
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct memory_struct *mem = (struct memory_struct *) userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    vdc_report(LOG_ERR, "network module: not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
  
  return realsize;
}

static void DebugDump(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex) {
  size_t i;
  size_t c;

  unsigned int width = 0x10;

  if (nohex)
    /* without the hex output, we can fit more on screen */
    width = 0x40;

  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n", text, (long) size, (long) size);

  for (i = 0; i < size; i += width) {
    fprintf(stream, "%4.4lx: ", (long) i);

    if (!nohex) {
      /* hex not disabled, show it */
      for (c = 0; c < width; c++)
        if (i + c < size)
          fprintf(stream, "%02x ", ptr[i + c]);
        else
          fputs("   ", stream);
    }

    for (c = 0; (c < width) && (i + c < size); c++) {
      /* check for 0D0A; if found, skip past and start a new line of output */
      if (nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D && ptr[i + c + 1] == 0x0A) {
        i += (c + 2 - width);
        break;
      }
      fprintf(stream, "%c", (ptr[i + c] >= 0x20) && (ptr[i + c] < 0x80) ? ptr[i + c] : '.');
      /* check again for 0D0A, to avoid an extra \n if it's at width */
      if (nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D && ptr[i + c + 2] == 0x0A) {
        i += (c + 3 - width);
        break;
      }
    }
    fputc('\n', stream); /* newline */
  }
  fflush(stream);
}

static int DebugCallback(CURL *handle, curl_infotype type, char *data, size_t size, void *userp) {
  struct data *config = (struct data *) userp;
  const char *text;
  (void) handle; /* prevent compiler warning */

  switch (type) {
    case CURLINFO_TEXT:
      fprintf(stderr, "== Info: %s", data);
    default: /* in case a new one is introduced to shock us */
      return 0;

    case CURLINFO_HEADER_OUT:
      text = "=> Send header";
      break;
    case CURLINFO_DATA_OUT:
      text = "=> Send data";
      break;
    case CURLINFO_SSL_DATA_OUT:
      text = "=> Send SSL data";
      break;
    case CURLINFO_HEADER_IN:
      text = "<= Recv header";
      break;
    case CURLINFO_DATA_IN:
      text = "<= Recv data";
      break;
    case CURLINFO_SSL_DATA_IN:
      text = "<= Recv SSL data";
      break;
  }
  DebugDump(text, stdout, (unsigned char *) data, size, config->trace_ascii);
  return 0;
}

int decodeURIComponent (char *sSource, char *sDest) {
  int nLength;
  for (nLength = 0; *sSource; nLength++) {
    if (*sSource == '%' && sSource[1] && sSource[2] && isxdigit(sSource[1]) && isxdigit(sSource[2])) {
      sSource[1] -= sSource[1] <= '9' ? '0' : (sSource[1] <= 'F' ? 'A' : 'a')-10;
      sSource[2] -= sSource[2] <= '9' ? '0' : (sSource[2] <= 'F' ? 'A' : 'a')-10;
      sDest[nLength] = 16 * sSource[1] + sSource[2];
      sSource += 3;
      continue;
    }
    sDest[nLength] = *sSource++;
  }
  sDest[nLength] = '\0';
  return nLength;
}

struct memory_struct* http_post(const char *url, json_object *jsondata) {
  CURL *curl;
  CURLcode res;
  struct memory_struct *chunk;

  chunk = malloc(sizeof(struct memory_struct));
  if (chunk == NULL) {
    vdc_report(LOG_ERR, "network: not enough memory\n");
    return NULL;
  }
  chunk->memory = malloc(1);
  chunk->size = 0;

  curl = curl_easy_init();
  if (curl == NULL) {
    vdc_report(LOG_ERR, "network: curl init failure\n");
    free(chunk->memory);
    free(chunk);
    return NULL;
  }
      
  struct curl_slist *headers = NULL;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void * )chunk);

  curl_easy_setopt(curl, CURLOPT_POST, 1);

  if(jsondata != NULL) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_object_to_json_string(jsondata));
    headers = curl_slist_append(headers, "Content-Type: application/json");
  } /*else {
    vdc_report(LOG_ERR, "network: post data missing");
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);  
    free(chunk->memory);
    free(chunk);
    
    return NULL;
  }**/

  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/59.0.3071.71 Safari/537.36");  
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 42);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");  
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
 
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  if (vdc_get_debugLevel() > LOG_DEBUG) {
    struct data config;
    config.trace_ascii = 1; /* enable ascii tracing */

    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, DebugCallback);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
    /* the DEBUGFUNCTION has no effect until we enable VERBOSE */
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }
  
  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    vdc_report(LOG_ERR, "network: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    
    if (chunk != NULL) {
      if (chunk->memory != NULL) {
        free(chunk->memory);
      }
      free(chunk);
      chunk = NULL;
    }
  } else {
    //vdc_report(LOG_ERR, "Response: %s\n", chunk->memory);    // results in segmentation fault if response is too long

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code == 403 || response_code == 404 || response_code == 503) {
      vdc_report(LOG_ERR, "Venta Humifier response: %d - ignoring response\n", response_code);
      if (chunk != NULL) {
        if (chunk->memory != NULL) {
          free(chunk->memory);
        }
        free(chunk);
        chunk = NULL;
      }
    } 
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return chunk;
}

int parse_json_data(struct memory_struct *response) {
  bool changed_values = FALSE;
  time_t now;
    
  now = time(NULL);
  vdc_report(LOG_DEBUG, "network: venta humifier values response = %s\n", response->memory);
  
  json_object *jobj = json_tokener_parse(response->memory);
  
  if (NULL == jobj) {
    vdc_report(LOG_ERR, "network: parsing json data failed, length %d, data:\n%s\n", response->size, response->memory);
    return VENTA_GETMEASURE_FAILED;
  }

  //pthread_mutex_lock(&g_network_mutex);

  json_object_object_foreach(jobj, key, val) {
    enum json_type type = json_object_get_type(val);
    
    sensor_value_t* svalue;

    if (strcmp(key, "device") == 0) {
      json_object_object_foreach(val, key1, val1) {
        enum json_type type1 = json_object_get_type(val1);
        if (strcmp(key1, "hum") == 0) {
          humifier_current_values->current_humidity = json_object_get_int(val1);
        } else if (strcmp(key1, "temp") == 0)  {
          humifier_current_values->current_temperature = json_object_get_int(val1);
        } else if (strcmp(key1, "humt") == 0)  {
          humifier_current_values->target_humidity = json_object_get_int(val1);
        } else if (strcmp(key1, "auto") == 0)  {
          humifier_current_values->mode_automatic = json_object_get_int(val1);
        } else if (strcmp(key1, "sleep") == 0)  {
          humifier_current_values->mode_sleep = json_object_get_int(val1);
        } else if (strcmp(key1, "fan") == 0)  {
          humifier_current_values->fan = json_object_get_int(val1);
        }
    
        vdc_report(LOG_INFO, "current_hum %d\n", humifier_current_values->current_humidity);
        vdc_report(LOG_INFO, "current_temp %d\n", humifier_current_values->current_temperature);
        vdc_report(LOG_INFO, "target_hum %d\n", humifier_current_values->target_humidity);
    
        svalue = find_sensor_value_by_name(key1);
        if (svalue == NULL) {
          vdc_report(LOG_WARNING, "value %s is not configured for evaluation - ignoring\n", key1);
        } else {
          if (type1 == json_type_int) {
            vdc_report(LOG_WARNING, "network: getdata returned %s: %d\n", key1, json_object_get_int(val1));
        
            //if ((svalue->last_reported == 0) || (svalue->last_value != json_object_get_int(val)) || (now - svalue->last_reported) > 180) {
            if ((svalue->last_reported == 0) || (svalue->last_value != json_object_get_int(val1))) {
              changed_values = TRUE;
            } 
            svalue->last_value = svalue->value;
            svalue->value = json_object_get_int(val1);
            svalue->last_query = now;
          } else if (type1 == json_type_boolean) {
            vdc_report(LOG_WARNING, "network: getmeasure returned %s: %s\n", key, json_object_get_boolean(val1)? "true": "false");
          
            //if ((svalue->last_reported == 0) || (svalue->last_value != json_object_get_boolean(val)) || (now - svalue->last_reported) > 180) {
            if ((svalue->last_reported == 0) || (svalue->last_value != json_object_get_boolean(val1))) {
              changed_values = TRUE;
            }
            svalue->last_value = svalue->value;
            svalue->value = json_object_get_boolean(val1);
            svalue->last_query = now;
          }
        }
      }
    }
  }

	pthread_mutex_unlock(&g_network_mutex);
  
  json_object_put(jobj);
   
  if (changed_values ) {
    return 0;
  } else return 1;
}

bool is_scene_configured(int scene) {
  char scene_str[5];
  sprintf(scene_str, "-%d-", scene);
  if (strstr(venta.humifier.configured_scenes, scene_str) != NULL) {
    return TRUE;
  } else return FALSE;
}

scene_t* get_scene_configuration(int scene) {
  scene_t *scene_data;
  int v;
  
  scene_data = malloc(sizeof(scene_t));
  if (!scene_data) {
    return NULL;
  }
  memset(scene_data, 0, sizeof(scene_t));
  
  v = 0;
  while (1) {
    if (&venta.humifier.scenes[v] != NULL) {
      if (venta.humifier.scenes[v].dsId == scene) {
        scene_data->fan = venta.humifier.scenes[v].fan;
        scene_data->mode_automatic = venta.humifier.scenes[v].mode_automatic;
        scene_data->mode_sleep = venta.humifier.scenes[v].mode_sleep;
        break;
      }
      
      v++;
    } else {
      break;
    }
  }
    
  return scene_data;
}

int venta_set_fan(int btn_val) {
  int rc;
  char url[100] = "http://";
  strcat(url, venta.humifier.ip);
  strcat(url, "/api/btn");
  
  vdc_report(LOG_NOTICE, "network: changing Venta Humifier fan speed btn val %d\n", btn_val);
  
  json_object *json1; 

  json_object *jint_btn = json_object_new_int(btn_val);
    
  json1 = json_object_new_object();

  json_object_object_add(json1,"btn", jint_btn);
    
  struct memory_struct *response = http_post(url, json1);
  
  //free mem
  json_object_put(json1);
  
  if (response == NULL) {
    vdc_report(LOG_ERR, "Venta config change failed\n");
    return VENTA_CONFIGCHANGE_FAILED;
  } 
    
  free(response->memory);
  free(response);
  
}

int venta_set_mode_sleep(bool on) {
  int rc;
  char url[100] = "http://";
  strcat(url, venta.humifier.ip);
  strcat(url, "/api/btn");
  
  vdc_report(LOG_NOTICE, "network: setting sleep mode for Venta Humifier\n");
  
  venta_get_data();
  
  if ((!humifier_current_values->mode_sleep && on) || (humifier_current_values->mode_sleep && !on)) {
    json_object *json1; 

    json_object *jint_btn = json_object_new_int(5);
      
    json1 = json_object_new_object();

    json_object_object_add(json1,"btn", jint_btn);
      
    struct memory_struct *response = http_post(url, json1);
    
    //free mem
    json_object_put(json1);
    
    if (response == NULL) {
      vdc_report(LOG_ERR, "Venta config change failed\n");
      return VENTA_CONFIGCHANGE_FAILED;
    } 
      
    free(response->memory);
    free(response);
  }    
}

int venta_set_mode_automatic(bool on) {
  int rc;
  char url[100] = "http://";
  strcat(url, venta.humifier.ip);
  strcat(url, "/api/btn");
  
  vdc_report(LOG_NOTICE, "network: setting sleep mode for Venta Humifier\n");
  
  venta_get_data();
  if ((!humifier_current_values->mode_automatic && on) || (humifier_current_values->mode_automatic && !on)) {
    json_object *json1; 

    json_object *jint_btn = json_object_new_int(6);
      
    json1 = json_object_new_object();

    json_object_object_add(json1,"btn", jint_btn);
      
    struct memory_struct *response = http_post(url, json1);
    
    //free mem
    json_object_put(json1);
    
    if (response == NULL) {
      vdc_report(LOG_ERR, "Venta config change failed\n");
      return VENTA_CONFIGCHANGE_FAILED;
    } 
      
    free(response->memory);
    free(response);
  }
}
  
int venta_get_data() {
  int rc;
  char url[100] = "http://";
  strcat(url, venta.humifier.ip);
  strcat(url, "/api/data");

  vdc_report(LOG_NOTICE, "network: reading Venta Humifier values\n");
  
  struct memory_struct *response = http_post(url, NULL);
  
  if (response == NULL) {
    vdc_report(LOG_ERR, "network: getting humifier values failed\n");
    return VENTA_CONNECT_FAILED;
  }
  
  rc = parse_json_data(response);
  
  free(response->memory);
  free(response);
  
  return rc;  
}

